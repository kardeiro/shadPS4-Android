package com.shadps4.emulator.data.model

import kotlinx.serialization.Serializable

/**
 * Subset of fields parsed from a PS4 `param.sfo` file by the native module.
 *
 * Field order MUST match the JNI constructor signature in
 * `app/src/main/cpp/shadps4_jni.cpp` — if you reorder or rename fields
 * here, update the JNI `(Ljava/lang/String;...)` signature too.
 *
 * See: https://www.psdevwiki.com/ps4/Param.sfo
 */
@Serializable
data class ParamSfo(
    val title: String,
    val titleId: String,
    val appVer: String,
    val category: String,
    val pubToolInfo: String,
    val contentId: String,
    val subtitle: String,
    val systemVer: Int,
    val attribute: Int,
) {
    /**
     * Decode the SYSTEM_VER field. The PS4 packs the firmware version as
     * `(major << 16) | (minor << 8)`. E.g. `0x05050000` → "5.50".
     * Returns "—" if not present.
     */
    val systemVersionFormatted: String
        get() = if (systemVer == 0) "—"
        else "${(systemVer shr 24) and 0xFF}.${(systemVer shr 16) and 0xFF}"

    /**
     * Parse the SDK version from PUBTOOLINFO. That field is a key=value list
     * like `sdk_ver=0x05050080,...`. Returns "—" if not found.
     */
    val sdkVersion: String
        get() {
            val key = "sdk_ver="
            val idx = pubToolInfo.indexOf(key)
            if (idx < 0) return "—"
            val start = idx + key.length
            val end = pubToolInfo.indexOf(',', start).let { if (it < 0) pubToolInfo.length else it }
            val raw = pubToolInfo.substring(start, end).trim()
            // raw looks like "0x05050080" — convert to "5.50"
            return try {
                val v = raw.removePrefix("0x").toLong(16)
                "${(v shr 24) and 0xFF}.${(v shr 16) and 0xFF}"
            } catch (_: NumberFormatException) {
                raw
            }
        }

    /**
     * PS4 game category codes (from PSDevWiki):
     *   ac  = additional content
     *   bd  = Blu-ray disc game
     *   gp  = game patch
     *   gd  = digital game
     */
    val categoryDescription: String
        get() = when (category) {
            "ac" -> "Additional Content"
            "bd" -> "Blu-ray Disc Game"
            "gp" -> "Game Patch"
            "gd" -> "Digital Game"
            else -> category.ifEmpty { "—" }
        }
}
