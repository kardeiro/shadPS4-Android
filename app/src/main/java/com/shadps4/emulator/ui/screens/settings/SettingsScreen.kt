package com.shadps4.emulator.ui.screens.settings

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.windowInsetsTopHeight
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.KeyboardArrowRight
import androidx.compose.material.icons.rounded.ChevronRight
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.shadps4.emulator.R
import com.shadps4.emulator.data.repository.SettingsRepository
import com.shadps4.emulator.ui.theme.ThemeMode
import kotlinx.coroutines.launch

@Composable
fun SettingsScreen(
    onAboutClick: () -> Unit,
) {
    val context = LocalContext.current
    val settings = remember { SettingsRepository(context) }
    val scope = rememberCoroutineScope()

    val themeMode by settings.themeMode.collectAsState(initial = ThemeMode.SYSTEM)
    val dynamicColor by settings.dynamicColorEnabled.collectAsState(initial = true)
    val resolutionScale by settings.resolutionScale.collectAsState(initial = 0)
    val vsync by settings.vsyncEnabled.collectAsState(initial = true)
    val asyncShaders by settings.asyncShaders.collectAsState(initial = true)
    val anisotropic by settings.anisotropicFiltering.collectAsState(initial = 0)
    val audioEnabled by settings.audioEnabled.collectAsState(initial = true)
    val audioVolume by settings.audioVolume.collectAsState(initial = 100)
    val touchControls by settings.touchControlsEnabled.collectAsState(initial = true)
    val vibration by settings.vibrationEnabled.collectAsState(initial = true)
    val cpuBackend by settings.cpuBackend.collectAsState(initial = 0)
    val logLevel by settings.logLevel.collectAsState(initial = 2)
    val dumpShaders by settings.dumpShaders.collectAsState(initial = false)
    val firmwareLoaded by settings.firmwareLoaded.collectAsState(initial = false)

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState()),
    ) {
        Spacer(Modifier.windowInsetsTopHeight(WindowInsets.statusBars))

        Text(
            text = stringResource(R.string.nav_settings),
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp),
        )

        // General
        SectionCard(title = stringResource(R.string.settings_general)) {
            // Theme
            ThemeRow(
                current = themeMode,
                onChange = { mode -> scope.launch { settings.setThemeMode(mode) } },
            )
            SwitchRow(
                title = stringResource(R.string.settings_general_dynamic_color),
                subtitle = "Material You expressive colors",
                checked = dynamicColor,
                onChange = { v -> scope.launch { settings.setDynamicColor(v) } },
            )
            ClickRow(
                title = stringResource(R.string.settings_general_language),
                subtitle = "System default",
                onClick = { /* TODO */ },
            )
            ClickRow(
                title = stringResource(R.string.settings_general_check_updates),
                subtitle = null,
                onClick = { /* TODO */ },
            )
        }

        // Graphics
        SectionCard(title = stringResource(R.string.settings_graphics)) {
            SelectorRow(
                title = stringResource(R.string.settings_graphics_resolution),
                selected = resolutionScale,
                options = listOf(
                    stringResource(R.string.settings_graphics_resolution_native),
                    stringResource(R.string.settings_graphics_resolution_720),
                    stringResource(R.string.settings_graphics_resolution_1080),
                    stringResource(R.string.settings_graphics_resolution_1440),
                    stringResource(R.string.settings_graphics_resolution_2160),
                ),
                onSelect = { i -> scope.launch { settings.setResolutionScale(i) } },
            )
            SwitchRow(
                title = stringResource(R.string.settings_graphics_vsync),
                subtitle = null,
                checked = vsync,
                onChange = { v -> scope.launch { settings.setVsync(v) } },
            )
            SwitchRow(
                title = stringResource(R.string.settings_graphics_async_shaders),
                subtitle = "Reduces stutter but may show visual artifacts",
                checked = asyncShaders,
                onChange = { v -> scope.launch { settings.setAsyncShaders(v) } },
            )
            SelectorRow(
                title = stringResource(R.string.settings_graphics_anisotropic),
                selected = anisotropic,
                options = listOf("Off", "2x", "4x", "8x", "16x"),
                onSelect = { i -> scope.launch { settings.setAnisotropicFiltering(i) } },
            )
            ClickRow(
                title = stringResource(R.string.settings_graphics_renderer),
                subtitle = stringResource(R.string.settings_graphics_renderer_vulkan),
                onClick = { /* fixed */ },
            )
        }

        // Audio
        SectionCard(title = stringResource(R.string.settings_audio)) {
            SwitchRow(
                title = stringResource(R.string.settings_audio_enabled),
                subtitle = null,
                checked = audioEnabled,
                onChange = { v -> scope.launch { settings.setAudioEnabled(v) } },
            )
            SliderRow(
                title = stringResource(R.string.settings_audio_volume),
                value = audioVolume.toFloat(),
                valueRange = 0f..150f,
                onChange = { v -> scope.launch { settings.setAudioVolume(v.toInt()) } },
                valueLabel = { "$it%" },
            )
        }

        // Controls
        SectionCard(title = stringResource(R.string.settings_controls)) {
            SwitchRow(
                title = stringResource(R.string.settings_controls_touch),
                subtitle = null,
                checked = touchControls,
                onChange = { v -> scope.launch { settings.setTouchControls(v) } },
            )
            SwitchRow(
                title = stringResource(R.string.settings_controls_vibration),
                subtitle = null,
                checked = vibration,
                onChange = { v -> scope.launch { settings.setVibration(v) } },
            )
            ClickRow(
                title = stringResource(R.string.settings_controls_controller),
                subtitle = "None connected",
                onClick = { /* TODO */ },
            )
            ClickRow(
                title = stringResource(R.string.settings_controls_customize),
                subtitle = null,
                onClick = { /* TODO */ },
            )
        }

        // Emulation / Firmware
        SectionCard(title = stringResource(R.string.settings_emulation)) {
            ClickRow(
                title = stringResource(R.string.settings_emulation_load_firmware),
                subtitle = if (firmwareLoaded) "Loaded" else stringResource(R.string.settings_emulation_firmware_not_loaded),
                onClick = { /* TODO: file picker */ },
            )
            ClickRow(
                title = stringResource(R.string.settings_emulation_install_firmware),
                subtitle = null,
                onClick = { /* TODO */ },
            )
        }

        // Advanced
        SectionCard(title = stringResource(R.string.settings_advanced)) {
            SelectorRow(
                title = stringResource(R.string.settings_advanced_cpu),
                selected = cpuBackend,
                options = listOf(
                    stringResource(R.string.settings_advanced_cpu_dynarmic),
                    stringResource(R.string.settings_advanced_cpu_interp),
                ),
                onSelect = { i -> scope.launch { settings.setCpuBackend(i) } },
            )
            SelectorRow(
                title = stringResource(R.string.settings_advanced_log_level),
                selected = logLevel,
                options = listOf("Trace", "Debug", "Info", "Warning", "Error", "Critical"),
                onSelect = { i -> scope.launch { settings.setLogLevel(i) } },
            )
            SwitchRow(
                title = stringResource(R.string.settings_advanced_dump_shaders),
                subtitle = null,
                checked = dumpShaders,
                onChange = { v -> scope.launch { settings.setDumpShaders(v) } },
            )
            ClickRow(
                title = stringResource(R.string.settings_advanced_clear_cache),
                subtitle = null,
                onClick = { /* TODO */ },
            )
        }

        // About
        SectionCard(title = stringResource(R.string.about_title)) {
            ClickRow(
                title = stringResource(R.string.about_title),
                subtitle = "${stringResource(R.string.about_version)} 0.1.0",
                onClick = onAboutClick,
                trailing = {
                    Icon(Icons.Rounded.ChevronRight, contentDescription = null,
                        tint = MaterialTheme.colorScheme.onSurfaceVariant)
                },
            )
        }

        Spacer(Modifier.size(24.dp))
    }
}

@Composable
private fun SectionCard(
    title: String,
    content: @Composable () -> Unit,
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp),
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerHigh,
        ),
    ) {
        Column(modifier = Modifier.padding(vertical = 8.dp)) {
            Text(
                text = title,
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.SemiBold,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
            )
            content()
        }
    }
}

@Composable
private fun ThemeRow(
    current: ThemeMode,
    onChange: (ThemeMode) -> Unit,
) {
    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp)) {
        Text(
            text = stringResource(R.string.settings_general_theme),
            style = MaterialTheme.typography.bodyLarge,
        )
        ThemeMode.entries.forEach { mode ->
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 4.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                RadioButton(
                    selected = mode == current,
                    onClick = { onChange(mode) },
                )
                val label = when (mode) {
                    ThemeMode.SYSTEM -> stringResource(R.string.settings_general_theme_system)
                    ThemeMode.LIGHT -> stringResource(R.string.settings_general_theme_light)
                    ThemeMode.DARK -> stringResource(R.string.settings_general_theme_dark)
                }
                Text(text = label, style = MaterialTheme.typography.bodyMedium)
            }
        }
    }
}

@Composable
private fun SwitchRow(
    title: String,
    subtitle: String?,
    checked: Boolean,
    onChange: (Boolean) -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(text = title, style = MaterialTheme.typography.bodyLarge)
            if (subtitle != null) {
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        Switch(checked = checked, onCheckedChange = onChange)
    }
}

@Composable
private fun SliderRow(
    title: String,
    value: Float,
    valueRange: ClosedFloatingPointRange<Float>,
    onChange: (Float) -> Unit,
    valueLabel: @Composable (Int) -> Unit,
) {
    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp)) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Text(text = title, style = MaterialTheme.typography.bodyLarge)
            valueLabel(value.toInt())
        }
        Slider(
            value = value,
            onValueChange = onChange,
            valueRange = valueRange,
        )
    }
}

@Composable
private fun SelectorRow(
    title: String,
    selected: Int,
    options: List<String>,
    onSelect: (Int) -> Unit,
) {
    Column(modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp)) {
        Text(text = title, style = MaterialTheme.typography.bodyLarge)
        options.forEachIndexed { index, label ->
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 4.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                RadioButton(
                    selected = index == selected,
                    onClick = { onSelect(index) },
                )
                Text(text = label, style = MaterialTheme.typography.bodyMedium)
            }
        }
    }
}

@Composable
private fun ClickRow(
    title: String,
    subtitle: String?,
    onClick: () -> Unit,
    trailing: @Composable (() -> Unit)? = null,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(text = title, style = MaterialTheme.typography.bodyLarge)
            if (subtitle != null) {
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        if (trailing != null) {
            trailing()
        } else {
            Icon(
                imageVector = Icons.AutoMirrored.Rounded.KeyboardArrowRight,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}
