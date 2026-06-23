package com.shadps4.emulator.data.repository

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import com.shadps4.emulator.data.model.GameInfo
import com.shadps4.emulator.data.model.ParamSfo
import com.shadps4.emulator.data.model.PkgInstallResult
import com.shadps4.emulator.data.native.ShadPs4Native
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.withContext
import java.io.File

/**
 * Repository of installed games.
 *
 * Phase 0: returned sample data only.
 * Phase 1: can parse real `param.sfo` files from disk via [ShadPs4Native], so
 * the user can pick a `sce_sys/param.sfo` file via SAF (Storage Access
 * Framework) and we'll convert it into a [GameInfo] entry.
 *
 * The full PKG installer (which decrypts + extracts the entire PKG into
 * app-private storage) is Phase 2 — for now we just read metadata.
 */
class GameRepository {

    private val _games = MutableStateFlow<List<GameInfo>>(seedSampleGames())
    val games: StateFlow<List<GameInfo>> = _games.asStateFlow()

    /** True once the user has at least one real game in the library. */
    val isEmpty: Boolean get() = _games.value.isEmpty()

    /**
     * Phase 1 ships with sample data so the UI is explorable out of the box.
     * Once the user imports their first real `param.sfo`, this sample data
     * stays around as a "demo" — they can swipe to remove individual entries.
     */
    private fun seedSampleGames(): List<GameInfo> =
        com.shadps4.emulator.data.model.SampleGames.all

    fun getGameById(id: String): GameInfo? = _games.value.firstOrNull { it.id == id }

    fun search(query: String): List<GameInfo> {
        val q = query.trim().lowercase()
        if (q.isEmpty()) return _games.value
        return _games.value.filter {
            it.title.lowercase().contains(q) ||
                it.serial.lowercase().contains(q)
        }
    }

    fun addGame(game: GameInfo) {
        _games.value = (_games.value + game).distinctBy { it.id }
    }

    fun removeGame(id: String) {
        _games.value = _games.value.filterNot { it.id == id }
    }

    /**
     * Try to parse a `param.sfo` from a SAF [uri] and add it as a new
     * [GameInfo]. Returns the created entry, or null if parsing failed.
     *
     * Must be called off the main thread (uses disk I/O + JNI).
     */
    suspend fun importFromParamSfo(context: Context, uri: Uri): GameInfo? =
        withContext(Dispatchers.IO) {
            // 1. Copy the picked file into app-private cache so we have an
            //    absolute path to feed to the native parser. (NDK cannot
            //    open SAF Uri streams directly.)
            val cached = copyUriToCache(context, uri) ?: return@withContext null
            val path = cached.absolutePath

            // 2. Ask native side to parse it.
            val psf: ParamSfo? = try {
                ShadPs4Native.nativeReadParamSfo(path)
            } catch (_: UnsatisfiedLinkError) {
                null
            }

            // 3. Clean up the cache file — we only needed it for parsing.
            cached.delete()

            if (psf == null) return@withContext null

            val game = GameInfo(
                id = psf.titleId.ifBlank { "imported-${System.currentTimeMillis()}" },
                title = psf.title.ifBlank { "Unknown title" },
                serial = psf.titleId,
                version = psf.appVer.ifBlank { "1.00" },
                category = psf.categoryDescription,
                firmware = psf.systemVersionFormatted,
                sizeBytes = 0L,
                installDateMs = System.currentTimeMillis(),
                lastPlayedMs = 0L,
                playtimeMs = 0L,
                compatibility = com.shadps4.emulator.data.model.CompatibilityStatus.UNKNOWN,
                pkgPath = uri.toString(),
            )
            addGame(game)
            game
        }

    /**
     * Install a PS4 PKG file picked via SAF.
     *
     * Flow:
     *  1. Copy the PKG into app-private cache (so we have an absolute path
     *     the NDK can `fopen()`).
     *  2. Compute a per-game destination directory under `context.filesDir/games/<TITLE_ID>/sce_sys/`.
     *  3. Call the native installer: it parses the PKG header, extracts the
     *     plaintext `sce_sys/` entries (param.sfo, icon0.png, pic1.png, ...)
     *     into the destination, and returns a structured result.
     *  4. Map the result into a [GameInfo] entry, with the cover icon pointing
     *     at the extracted `icon0.png` on disk — so the UI can render it
     *     directly via Coil.
     *  5. Delete the cached PKG copy.
     *
     * Returns the parsed [PkgInstallResult] (always non-null — check
     * [PkgInstallResult.isSuccess]).
     */
    suspend fun installFromPkg(context: Context, uri: Uri): PkgInstallResult =
        withContext(Dispatchers.IO) {
            // 1. Copy PKG to cache (file can be GB-sized for retail games — but
            //    Phase 2 only reads the first ~few MB for header + sce_sys/,
            //    so we cap the copy at 64MB to avoid OOM on phones).
            val cachedPkg = copyUriToCache(context, uri, maxBytes = 64L * 1024 * 1024)
                ?: return@withContext PkgInstallResult(
                    paramSfo = null, iconPath = null, pic1Path = null,
                    destDir = "", entryCount = 0, isDrmFree = false,
                    error = "Failed to copy PKG from SAF. File too large or unreadable.",
                )

            try {
                // 2. Compute dest dir — we use a temporary name based on the URI
                //    hash; once we know the TITLE_ID we'll rename it.
                val tmpDest = File(context.filesDir, "games/_pending_${System.currentTimeMillis()}/sce_sys")
                tmpDest.mkdirs()

                // 3. Call native.
                val result = try {
                    ShadPs4Native.nativeInstallPkg(cachedPkg.absolutePath, tmpDest.absolutePath)
                } catch (e: UnsatisfiedLinkError) {
                    return@withContext PkgInstallResult(
                        paramSfo = null, iconPath = null, pic1Path = null,
                        destDir = "", entryCount = 0, isDrmFree = false,
                        error = "Native module not loaded: ${e.message}",
                    )
                }

                if (!result.isSuccess) {
                    // Cleanup the partial extraction.
                    tmpDest.parentFile?.deleteRecursively()
                    return@withContext result
                }

                // 4. Rename the pending dir to the real TITLE_ID (if available).
                val titleId = result.paramSfo?.titleId?.takeIf { it.isNotBlank() }
                    ?: "IMPORTED_${System.currentTimeMillis()}"
                val finalDest = File(context.filesDir, "games/$titleId/sce_sys")
                if (finalDest.exists()) finalDest.deleteRecursively()
                tmpDest.parentFile?.renameTo(File(context.filesDir, "games/$titleId"))

                // 5. Build a GameInfo entry that references the extracted icon.
                val iconPath = result.iconPath?.let { path ->
                    // The JNI returned a path under tmpDest; remap to finalDest.
                    val name = File(path).name
                    File(finalDest, name).takeIf { it.exists() }?.absolutePath
                } ?: result.iconPath

                val game = GameInfo(
                    id = titleId,
                    title = result.paramSfo?.title?.ifBlank { "Unknown title" } ?: "Unknown title",
                    serial = result.paramSfo?.titleId.orEmpty(),
                    version = result.paramSfo?.appVer?.ifBlank { "1.00" } ?: "1.00",
                    category = result.paramSfo?.categoryDescription ?: "—",
                    firmware = result.paramSfo?.systemVersionFormatted ?: "—",
                    sizeBytes = cachedPkg.length(),
                    installDateMs = System.currentTimeMillis(),
                    lastPlayedMs = 0L,
                    playtimeMs = 0L,
                    compatibility = com.shadps4.emulator.data.model.CompatibilityStatus.UNKNOWN,
                    pkgPath = uri.toString(),
                    coverPath = iconPath,
                    iconPath = iconPath,
                )
                addGame(game)

                // Return a result with the final iconPath (post-rename).
                result.copy(iconPath = iconPath, destDir = finalDest.absolutePath)
            } finally {
                // 5. Always delete the cached PKG to free up space.
                cachedPkg.delete()
            }
        }

    private fun copyUriToCache(
        context: Context,
        uri: Uri,
        suffix: String = ".bin",
        maxBytes: Long = Long.MAX_VALUE,
    ): File? = try {
        val input = context.contentResolver.openInputStream(uri) ?: return null
        val outFile = File(context.cacheDir, "pkg_${System.currentTimeMillis()}$suffix")
        outFile.outputStream().use { output ->
            input.use { stream ->
                val buf = ByteArray(64 * 1024)
                var total = 0L
                while (true) {
                    val n = stream.read(buf)
                    if (n <= 0) break
                    output.write(buf, 0, n)
                    total += n
                    if (total >= maxBytes) break
                }
            }
        }
        outFile
    } catch (e: Exception) {
        null
    }
}
