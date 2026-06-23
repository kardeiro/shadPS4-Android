package com.shadps4.emulator

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.shadps4.emulator.data.repository.SettingsRepository
import com.shadps4.emulator.ui.ShadPs4App
import com.shadps4.emulator.ui.theme.ShadPs4Theme
import com.shadps4.emulator.ui.theme.ThemeMode

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        val settingsRepository = SettingsRepository(applicationContext)

        setContent {
            val themeMode by settingsRepository.themeMode.collectAsStateWithLifecycle(initialValue = ThemeMode.SYSTEM)
            val dynamicColor by settingsRepository.dynamicColorEnabled.collectAsStateWithLifecycle(initialValue = true)

            ShadPs4Theme(
                themeMode = themeMode,
                dynamicColor = dynamicColor,
            ) {
                ShadPs4App()
            }
        }
    }
}
