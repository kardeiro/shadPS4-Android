package com.shadps4.emulator.data.repository

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import com.shadps4.emulator.ui.theme.ThemeMode
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.settingsDataStore: DataStore<Preferences> by preferencesDataStore(name = "shadps4_settings")

class SettingsRepository(private val context: Context) {

    private val store get() = context.settingsDataStore

    val themeMode: Flow<ThemeMode> = store.data.map { p ->
        when (p[KEY_THEME_MODE]) {
            1 -> ThemeMode.LIGHT
            2 -> ThemeMode.DARK
            else -> ThemeMode.SYSTEM
        }
    }

    val dynamicColorEnabled: Flow<Boolean> = store.data.map { p ->
        p[KEY_DYNAMIC_COLOR] ?: true
    }

    val resolutionScale: Flow<Int> = store.data.map { p ->
        p[KEY_RESOLUTION] ?: 0
    }

    val vsyncEnabled: Flow<Boolean> = store.data.map { p -> p[KEY_VSYNC] ?: true }
    val asyncShaders: Flow<Boolean> = store.data.map { p -> p[KEY_ASYNC_SHADERS] ?: true }
    val anisotropicFiltering: Flow<Int> = store.data.map { p -> p[KEY_ANISO] ?: 0 }

    val audioEnabled: Flow<Boolean> = store.data.map { p -> p[KEY_AUDIO_ENABLED] ?: true }
    val audioVolume: Flow<Int> = store.data.map { p -> p[KEY_AUDIO_VOLUME] ?: 100 }

    val touchControlsEnabled: Flow<Boolean> = store.data.map { p -> p[KEY_TOUCH] ?: true }
    val vibrationEnabled: Flow<Boolean> = store.data.map { p -> p[KEY_VIBRATION] ?: true }

    val cpuBackend: Flow<Int> = store.data.map { p -> p[KEY_CPU_BACKEND] ?: 0 }
    val logLevel: Flow<Int> = store.data.map { p -> p[KEY_LOG_LEVEL] ?: 2 }
    val dumpShaders: Flow<Boolean> = store.data.map { p -> p[KEY_DUMP_SHADERS] ?: false }

    val firmwareLoaded: Flow<Boolean> = store.data.map { p -> p[KEY_FIRMWARE_LOADED] ?: false }

    suspend fun setThemeMode(mode: ThemeMode) {
        store.edit { it[KEY_THEME_MODE] = when (mode) {
            ThemeMode.LIGHT -> 1
            ThemeMode.DARK -> 2
            ThemeMode.SYSTEM -> 0
        } }
    }

    suspend fun setDynamicColor(enabled: Boolean) {
        store.edit { it[KEY_DYNAMIC_COLOR] = enabled }
    }

    suspend fun setResolutionScale(value: Int) { store.edit { it[KEY_RESOLUTION] = value } }
    suspend fun setVsync(enabled: Boolean) { store.edit { it[KEY_VSYNC] = enabled } }
    suspend fun setAsyncShaders(enabled: Boolean) { store.edit { it[KEY_ASYNC_SHADERS] = enabled } }
    suspend fun setAnisotropicFiltering(value: Int) { store.edit { it[KEY_ANISO] = value } }
    suspend fun setAudioEnabled(enabled: Boolean) { store.edit { it[KEY_AUDIO_ENABLED] = enabled } }
    suspend fun setAudioVolume(value: Int) { store.edit { it[KEY_AUDIO_VOLUME] = value.coerceIn(0, 150) } }
    suspend fun setTouchControls(enabled: Boolean) { store.edit { it[KEY_TOUCH] = enabled } }
    suspend fun setVibration(enabled: Boolean) { store.edit { it[KEY_VIBRATION] = enabled } }
    suspend fun setCpuBackend(value: Int) { store.edit { it[KEY_CPU_BACKEND] = value } }
    suspend fun setLogLevel(value: Int) { store.edit { it[KEY_LOG_LEVEL] = value } }
    suspend fun setDumpShaders(enabled: Boolean) { store.edit { it[KEY_DUMP_SHADERS] = enabled } }
    suspend fun setFirmwareLoaded(loaded: Boolean) { store.edit { it[KEY_FIRMWARE_LOADED] = loaded } }

    companion object {
        private val KEY_THEME_MODE = intPreferencesKey("theme_mode")
        private val KEY_DYNAMIC_COLOR = booleanPreferencesKey("dynamic_color")
        private val KEY_RESOLUTION = intPreferencesKey("resolution_scale")
        private val KEY_VSYNC = booleanPreferencesKey("vsync")
        private val KEY_ASYNC_SHADERS = booleanPreferencesKey("async_shaders")
        private val KEY_ANISO = intPreferencesKey("anisotropic")
        private val KEY_AUDIO_ENABLED = booleanPreferencesKey("audio_enabled")
        private val KEY_AUDIO_VOLUME = intPreferencesKey("audio_volume")
        private val KEY_TOUCH = booleanPreferencesKey("touch_controls")
        private val KEY_VIBRATION = booleanPreferencesKey("vibration")
        private val KEY_CPU_BACKEND = intPreferencesKey("cpu_backend")
        private val KEY_LOG_LEVEL = intPreferencesKey("log_level")
        private val KEY_DUMP_SHADERS = booleanPreferencesKey("dump_shaders")
        private val KEY_FIRMWARE_LOADED = booleanPreferencesKey("firmware_loaded")
    }
}
