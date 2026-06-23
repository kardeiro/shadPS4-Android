package com.shadps4.emulator.data.repository

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import com.shadps4.emulator.data.model.GameInfo
import com.shadps4.emulator.data.model.ParamSfo
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

    private fun copyUriToCache(context: Context, uri: Uri): File? = try {
        val input = context.contentResolver.openInputStream(uri) ?: return null
        val outFile = File(context.cacheDir, "param_${System.currentTimeMillis()}.sfo")
        outFile.outputStream().use { output -> input.use { it.copyTo(output) } }
        outFile
    } catch (e: Exception) {
        null
    }
}
