package com.shadps4.emulator.ui.navigation

import kotlinx.serialization.Serializable

/**
 * Navigation graph for the shadPS4 Android app. Uses the type-safe navigation
 * API from androidx.navigation:navigation-compose 2.8+.
 */
@Serializable
sealed class Destination {

    @Serializable
    data object Main : Destination()

    @Serializable
    data class GameDetail(val gameId: String) : Destination()

    @Serializable
    data class Emulation(val gameId: String) : Destination()

    @Serializable
    data object About : Destination()
}
