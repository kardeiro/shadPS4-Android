package com.shadps4.emulator.data.model

/**
 * Sample / placeholder game data, used only for the empty-state preview
 * screenshot. In production, the library starts empty and is populated
 * by the user installing PKG files via the "Install PKG" button.
 *
 * Kept here as a reference of what fields look like — but NOT used at
 * runtime (GameRepository now starts with an empty list).
 */
@Suppress("unused")
object SampleGames {

    val all: List<GameInfo> = emptyList()

    val recent: List<GameInfo>
        get() = emptyList()
}
