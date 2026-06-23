package com.shadps4.emulator.data.model

import kotlinx.serialization.Serializable

/**
 * Compatibility status of a game in the emulator.
 */
@Serializable
enum class CompatibilityStatus(val label: String) {
    PLAYABLE("Playable"),
    INGAME("In-game"),
    MENU("Menus"),
    INTRO("Intro"),
    NOTHING("Nothing"),
    UNKNOWN("Unknown"),
}

@Serializable
data class GameInfo(
    val id: String,
    val title: String,
    val serial: String = "",
    val version: String = "1.00",
    val category: String = "Game",
    val firmware: String = "—",
    val sizeBytes: Long = 0L,
    val coverPath: String? = null,
    val iconPath: String? = null,
    val installDateMs: Long = 0L,
    val lastPlayedMs: Long = 0L,
    val playtimeMs: Long = 0L,
    val compatibility: CompatibilityStatus = CompatibilityStatus.UNKNOWN,
    val pkgPath: String? = null,
    val notes: String? = null,
) {
    val sizeFormatted: String
        get() = formatSize(sizeBytes)

    companion object {
        fun formatSize(bytes: Long): String {
            if (bytes <= 0) return "—"
            val units = arrayOf("B", "KB", "MB", "GB", "TB")
            var size = bytes.toDouble()
            var unitIndex = 0
            while (size >= 1024 && unitIndex < units.lastIndex) {
                size /= 1024.0
                unitIndex++
            }
            return "%.1f %s".format(size, units[unitIndex])
        }
    }
}
