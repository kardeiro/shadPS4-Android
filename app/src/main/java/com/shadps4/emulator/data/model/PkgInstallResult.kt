package com.shadps4.emulator.data.model

import kotlinx.serialization.Serializable

/**
 * Result of installing a PKG file via [ShadPs4Native.nativeInstallPkg].
 *
 * Field order MUST match the JNI constructor signature in
 * `app/src/main/cpp/shadps4_jni.cpp` (search for `nativeInstallPkg`).
 *
 * @property paramSfo    Parsed metadata from `sce_sys/param.sfo`, or null if
 *                       the PKG had no param.sfo entry / parsing failed.
 * @property iconPath    Absolute filesystem path to `icon0.png` (the cover
 *                       icon), or null if the PKG had no icon0.png.
 * @property pic1Path    Absolute filesystem path to `pic1.png` (background
 *                       splash art), or null if not present.
 * @property destDir     Absolute path of the directory where `sce_sys/`
 *                       entries were extracted (always set on success).
 * @property entryCount  Number of plaintext sce_sys/ entries extracted.
 * @property isDrmFree   True if this is a DRM-free FPKG (pkg_type bit 31 set).
 *                       Retail NPDRM PKGs will return false here.
 * @property error       Empty string on success, otherwise a user-facing
 *                       error message describing what went wrong.
 */
@Serializable
data class PkgInstallResult(
    val paramSfo: ParamSfo?,
    val iconPath: String?,
    val pic1Path: String?,
    val destDir: String,
    val entryCount: Int,
    val isDrmFree: Boolean,
    val error: String,
) {
    val isSuccess: Boolean get() = error.isEmpty()
}
