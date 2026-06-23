package com.shadps4.emulator.ui.navigation

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Collections
import androidx.compose.material.icons.outlined.Home
import androidx.compose.material.icons.outlined.Info
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material.icons.rounded.Collections
import androidx.compose.material.icons.rounded.Home
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.ui.graphics.vector.ImageVector

enum class TopLevelDestination(
    val route: String,
    val labelRes: Int,
    val selectedIcon: ImageVector,
    val unselectedIcon: ImageVector,
) {
    HOME(
        route = "home",
        labelRes = com.shadps4.emulator.R.string.nav_home,
        selectedIcon = Icons.Rounded.Home,
        unselectedIcon = Icons.Outlined.Home,
    ),
    LIBRARY(
        route = "library",
        labelRes = com.shadps4.emulator.R.string.nav_library,
        selectedIcon = Icons.Rounded.Collections,
        unselectedIcon = Icons.Outlined.Collections,
    ),
    COMPATIBILITY(
        route = "compatibility",
        labelRes = com.shadps4.emulator.R.string.nav_compatibility,
        selectedIcon = Icons.Rounded.Info,
        unselectedIcon = Icons.Outlined.Info,
    ),
    SETTINGS(
        route = "settings",
        labelRes = com.shadps4.emulator.R.string.nav_settings,
        selectedIcon = Icons.Rounded.Settings,
        unselectedIcon = Icons.Outlined.Settings,
    );

    companion object {
        val START: TopLevelDestination = HOME
    }
}
