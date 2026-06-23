package com.shadps4.emulator.viewmodel

import android.app.Application
import android.content.Context
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.shadps4.emulator.data.model.GameInfo
import com.shadps4.emulator.data.repository.GameRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.launch

/**
 * Shared ViewModel that owns the [GameRepository] and exposes its state to
 * the Compose screens.
 */
class LibraryViewModel(app: Application) : AndroidViewModel(app) {

    private val repository = GameRepository()

    val games: StateFlow<List<GameInfo>> = repository.games

    private val _importState = MutableStateFlow<ImportState>(ImportState.Idle)
    val importState: StateFlow<ImportState> = _importState.asStateFlow()

    fun getGame(id: String): GameInfo? = repository.getGameById(id)

    fun removeGame(id: String) = repository.removeGame(id)

    /**
     * Triggered by the "Install PKG / Import param.sfo" UI button. Resolves
     * the SAF-picked [uri] into a [GameInfo] entry via the native PSF parser.
     */
    fun importParamSfo(uri: Uri) {
        _importState.value = ImportState.Loading
        viewModelScope.launch {
            val ctx: Context = getApplication()
            val game = repository.importFromParamSfo(ctx, uri)
            _importState.value = if (game != null) {
                ImportState.Success(game)
            } else {
                ImportState.Error("Failed to parse param.sfo — is the file a real PS4 param.sfo?")
            }
        }
    }

    /**
     * Install a PS4 PKG file. The native module extracts the plaintext
     * `sce_sys/` entries (param.sfo + icon0.png + pic1.png) and adds the
     * game to the library. See [GameRepository.installFromPkg].
     */
    fun installPkg(uri: Uri) {
        _importState.value = ImportState.Loading
        viewModelScope.launch {
            val ctx: Context = getApplication()
            val result = repository.installFromPkg(ctx, uri)
            _importState.value = if (result.isSuccess && result.paramSfo != null) {
                val game = repository.getGameById(result.paramSfo.titleId)
                if (game != null) ImportState.Success(game)
                else ImportState.Error("PKG extracted but no GameInfo entry was created.")
            } else {
                ImportState.Error(result.error.ifEmpty { "Unknown PKG install error." })
            }
        }
    }

    /**
     * Phase 3: full PKG install (RSA + AES-XTS + zlib, FPKG only).
     * Use this for FPKG files where you want the complete game extracted
     * (not just metadata). Slower than [installPkg] — emits progress via
     * [repository.installProgress].
     */
    fun installPkgFull(uri: Uri) {
        _importState.value = ImportState.Loading
        viewModelScope.launch {
            val ctx: Context = getApplication()
            val result = repository.installFullFromPkg(ctx, uri)
            _importState.value = if (result.isSuccess && result.paramSfo != null) {
                val game = repository.getGameById(result.paramSfo.titleId)
                if (game != null) ImportState.Success(game)
                else ImportState.Error("PKG extracted but no GameInfo entry was created.")
            } else {
                ImportState.Error(result.error.ifEmpty { "Unknown PKG install error." })
            }
        }
    }

    fun resetImportState() { _importState.value = ImportState.Idle }

    sealed interface ImportState {
        data object Idle : ImportState
        data object Loading : ImportState
        data class Success(val game: GameInfo) : ImportState
        data class Error(val message: String) : ImportState
    }
}
