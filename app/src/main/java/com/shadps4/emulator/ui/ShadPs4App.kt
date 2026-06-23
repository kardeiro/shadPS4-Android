package com.shadps4.emulator.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import androidx.navigation.NavType
import com.shadps4.emulator.R
import com.shadps4.emulator.ui.components.ShadNavigationBar
import com.shadps4.emulator.ui.navigation.TopLevelDestination
import com.shadps4.emulator.ui.screens.about.AboutScreen
import com.shadps4.emulator.ui.screens.compatibility.CompatibilityScreen
import com.shadps4.emulator.ui.screens.gamedetail.GameDetailScreen
import com.shadps4.emulator.ui.screens.home.HomeScreen
import com.shadps4.emulator.ui.screens.library.LibraryScreen
import com.shadps4.emulator.ui.screens.settings.SettingsScreen

@Composable
fun ShadPs4App() {
    val navController = rememberNavController()
    val snackbarHostState = remember { SnackbarHostState() }

    val backStackEntry = navController.currentBackStackEntryAsState().value
    val currentRoute = backStackEntry?.destination?.route

    val showBottomBar = currentRoute in TopLevelDestination.entries.map { it.route }

    Scaffold(
        snackbarHost = { SnackbarHost(hostState = snackbarHostState) },
        bottomBar = {
            if (showBottomBar) {
                ShadNavigationBar(
                    currentRoute = currentRoute,
                    onNavigate = { dest ->
                        if (currentRoute != dest.route) {
                            navController.navigate(dest.route) {
                                popUpTo(navController.graph.findStartDestination().id) {
                                    saveState = true
                                }
                                launchSingleTop = true
                                restoreState = true
                            }
                        }
                    },
                )
            }
        },
    ) { innerPadding ->
        NavHost(
            navController = navController,
            startDestination = TopLevelDestination.START.route,
            modifier = Modifier.fillMaxSize().padding(innerPadding),
        ) {
            composable(TopLevelDestination.HOME.route) {
                HomeScreen(
                    onGameClick = { id -> navController.navigate("game/$id") },
                    onSeeAll = { navController.navigate(TopLevelDestination.LIBRARY.route) },
                    onOpenGame = { /* TODO: file picker */ },
                    onInstallPkg = { /* TODO: file picker */ },
                    onScanLibrary = { navController.navigate(TopLevelDestination.LIBRARY.route) },
                )
            }
            composable(TopLevelDestination.LIBRARY.route) {
                LibraryScreen(
                    onGameClick = { id -> navController.navigate("game/$id") },
                    onAddFolder = { /* TODO */ },
                    onInstallPkg = { /* TODO */ },
                )
            }
            composable(TopLevelDestination.COMPATIBILITY.route) {
                CompatibilityScreen(
                    onGameClick = { id -> navController.navigate("game/$id") },
                )
            }
            composable(TopLevelDestination.SETTINGS.route) {
                SettingsScreen(
                    onAboutClick = { navController.navigate("about") },
                )
            }
            composable(
                route = "game/{gameId}",
                arguments = listOf(navArgument("gameId") { type = NavType.StringType }),
            ) { backStackEntry ->
                val gameId = backStackEntry.arguments?.getString("gameId").orEmpty()
                GameDetailScreen(
                    gameId = gameId,
                    onBack = { navController.popBackStack() },
                    onPlay = { /* navController.navigate("emulation/$gameId") */ },
                )
            }
            composable("about") {
                AboutScreen(onBack = { navController.popBackStack() })
            }
        }
    }
}
