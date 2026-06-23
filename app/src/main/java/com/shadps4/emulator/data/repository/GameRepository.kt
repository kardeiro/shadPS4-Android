package com.shadps4.emulator.data.repository

import android.content.Context
import android.net.Uri
import com.shadps4.emulator.data.model.GameInfo
import com.shadps4.emulator.data.model.ParamSfo
import com.shadps4.emulator.data.model.PkgInstallResult
import com.shadps4.emulator.data.native.ShadPs4Native
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.withContext
import java.io.File

/**
 * Repository of installed games.
 *
 * The library starts EMPTY — no sample data. The user installs PKG files
 * via the "Install PKG" button and games appear here.
 *
 * Two install modes:
 *   - [installFromPkg] (Phase 2): extracts ONLY the plaintext `sce_sys/`
 *     entries (param.sfo, icon0.png, pic1.png). Fast (~seconds), small
 *     (a few KB), works for any PKG including retail NPDRM. Doesn't
 *     enable gameplay — just library metadata + cover art.
 *
 *   - [installFullFromPkg] (Phase 3): extracts the FULL PKG including the
 *     PFS body. Uses Crypto++ (RSA-2048 + AES-XTS) + zlib. Slow (minutes
 *     for big games), large (full game size on disk). Only works for
 *     FPKG (DRM-free) packages — retail NPDRM keys aren't bundled.
 */
class GameRepository {

    private val _games = MutableStateFlow<List<GameInfo>>(emptyList())
    val games: StateFlow<List<GameInfo>> = _games.asStateFlow()

    /** True if the user has no games installed yet. */
    val isEmpty: Boolean get() = _games.value.isEmpty()

    /** Progress events for the UI: (currentFile, totalFiles, currentFilename). */
    private val _installProgress = MutableSharedFlow<InstallProgress>(extraBufferCapacity = 16)
    val installProgress: SharedFlow<InstallProgress> = _installProgress.asSharedFlow()

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
        // Also delete the extracted files from disk.
        // (Caller should pass the TITLE_ID; we delete the entire game folder.)
        // We don't have a Context here, so the UI layer handles deletion.
    }

    /**
     * Try to parse a `param.sfo` from a SAF [uri] and add it as a new
     * [GameInfo]. Returns the created entry, or null if parsing failed.
     */
    suspend fun importFromParamSfo(context: Context, uri: Uri): GameInfo? =
        withContext(Dispatchers.IO) {
            val cached = copyUriToCache(context, uri) ?: return@withContext null
            val path = cached.absolutePath

            val psf: ParamSfo? = try {
                ShadPs4Native.nativeReadParamSfo(path)
            } catch (_: UnsatisfiedLinkError) {
                null
            }

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
     * Install a PS4 PKG file — Phase 2 mode (metadata only).
     *
     * Extracts ONLY the plaintext `sce_sys/` entries: param.sfo, icon0.png,
     * pic1.png, pic0.png, snd0.at9. Fast (a few seconds) and works for any
     * PKG including retail NPDRM.
     *
     * For full extraction including the PFS body, use [installFullFromPkg].
     */
    suspend fun installFromPkg(context: Context, uri: Uri): PkgInstallResult =
        withContext(Dispatchers.IO) {
            val cachedPkg = copyUriToCache(context, uri, maxBytes = 64L * 1024 * 1024)
                ?: return@withContext PkgInstallResult(
                    paramSfo = null, iconPath = null, pic1Path = null,
                    destDir = null, entryCount = 0, isDrmFree = false,
                    error = "Failed to copy PKG from SAF. File too large or unreadable.",
                )

            try {
                val tmpDest = File(context.filesDir, "games/_pending_${System.currentTimeMillis()}/sce_sys")
                tmpDest.mkdirs()

                val result = try {
                    ShadPs4Native.nativeInstallPkg(cachedPkg.absolutePath, tmpDest.absolutePath)
                } catch (e: UnsatisfiedLinkError) {
                    return@withContext PkgInstallResult(
                        paramSfo = null, iconPath = null, pic1Path = null,
                        destDir = null, entryCount = 0, isDrmFree = false,
                        error = "Native module not loaded: ${e.message}",
                    )
                }

                if (!result.isSuccess) {
                    tmpDest.parentFile?.deleteRecursively()
                    return@withContext result
                }

                val titleId = result.paramSfo?.titleId?.takeIf { it.isNotBlank() }
                    ?: "IMPORTED_${System.currentTimeMillis()}"
                val finalDest = File(context.filesDir, "games/$titleId/sce_sys")
                if (finalDest.exists()) finalDest.deleteRecursively()
                tmpDest.parentFile?.renameTo(File(context.filesDir, "games/$titleId"))

                val iconPath = result.iconPath?.let { path ->
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

                result.copy(iconPath = iconPath, destDir = finalDest.absolutePath)
            } finally {
                cachedPkg.delete()
            }
        }

    /**
     * Install a PS4 PKG file — Phase 3 mode (FULL extraction).
     *
     * Performs the complete extraction pipeline:
     *   - RSA-2048 decrypt entry_keys[3] → dk3
     *   - SHA256 + AES-CBC to derive imgKey + ekpfsKey
     *   - AES-XTS decrypt PFS body block by block
     *   - zlib inflate each PFSC block
     *   - Write every file inside the PFS to disk under
     *     `context.filesDir/games/<TITLE_ID>/`
     *
     * Only works for FPKG (DRM-free) packages. Retail NPDRM packages don't
     * have their PSN license keys bundled, so the RSA decryption will
     * produce garbage and PFS extraction will fail. For retail packages,
     * fall back to [installFromPkg] (metadata only).
     *
     * Progress events are emitted via [installProgress] as each file is
     * extracted. The UI can collect them to update a progress bar.
     *
     * IMPORTANT: This is a LONG operation. A 30GB game can take 5-15 minutes
     * on a fast phone. The caller MUST run this in a background scope and
     * MUST NOT block the UI thread.
     */
    suspend fun installFullFromPkg(context: Context, uri: Uri): PkgInstallResult =
        withContext(Dispatchers.IO) {
            // Phase 3 needs the FULL PKG on disk — no cap.
            _installProgress.tryEmit(InstallProgress.Preparing)

            val cachedPkg = copyUriToCache(context, uri, maxBytes = Long.MAX_VALUE)
                ?: return@withContext PkgInstallResult(
                    paramSfo = null, iconPath = null, pic1Path = null,
                    destDir = null, entryCount = 0, isDrmFree = false,
                    error = "Failed to copy PKG from SAF.",
                )

            try {
                _installProgress.tryEmit(InstallProgress.ParsingHeader)

                // Phase 3 needs a dest dir WITHOUT the /sce_sys suffix —
                // the native code will create sce_sys/ inside it AND write
                // PFS-extracted files alongside.
                val tmpDest = File(context.filesDir, "games/_pending_${System.currentTimeMillis()}")
                tmpDest.mkdirs()

                val result = try {
                    ShadPs4Native.nativeInstallPkgFull(
                        cachedPkg.absolutePath,
                        tmpDest.absolutePath,
                    ) { current, total, filename ->
                        _installProgress.tryEmit(
                            InstallProgress.Extracting(current, total, filename)
                        )
                    }
                } catch (e: UnsatisfiedLinkError) {
                    return@withContext PkgInstallResult(
                        paramSfo = null, iconPath = null, pic1Path = null,
                        destDir = null, entryCount = 0, isDrmFree = false,
                        error = "Native module not loaded: ${e.message}",
                    )
                }

                if (!result.isSuccess) {
                    tmpDest.deleteRecursively()
                    return@withContext result
                }

                // Rename to TITLE_ID.
                val titleId = result.paramSfo?.titleId?.takeIf { it.isNotBlank() }
                    ?: "IMPORTED_${System.currentTimeMillis()}"
                val finalDest = File(context.filesDir, "games/$titleId")
                if (finalDest.exists()) finalDest.deleteRecursively()
                tmpDest.renameTo(finalDest)

                val iconPath = result.iconPath?.let { path ->
                    val name = File(path).name
                    File(finalDest, "sce_sys/$name").takeIf { it.exists() }?.absolutePath
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

                _installProgress.tryEmit(InstallProgress.Done(result.entryCount))

                result.copy(iconPath = iconPath, destDir = finalDest.absolutePath)
            } finally {
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

    /**
     * Progress events emitted by [installFullFromPkg]. Collected by the UI
     * to update a progress bar.
     */
    sealed interface InstallProgress {
        data object Preparing : InstallProgress
        data object ParsingHeader : InstallProgress
        data class Extracting(val current: Int, val total: Int, val filename: String) : InstallProgress
        data class Done(val fileCount: Int) : InstallProgress
    }
}
