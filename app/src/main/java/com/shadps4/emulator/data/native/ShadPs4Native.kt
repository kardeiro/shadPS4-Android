package com.shadps4.emulator.data.native

import com.shadps4.emulator.data.model.ParamSfo
import com.shadps4.emulator.data.model.PkgInstallResult

/**
 * Thin Kotlin façade over the native (C++) module loaded via NDK.
 *
 * Phase 1: PSF (param.sfo) parser.
 * Phase 2: PKG installer (sce_sys/ plaintext entries only).
 * Phase 3: Full PKG installer (RSA + AES-XTS + zlib, FPKG only).
 *
 * NOTE: `package com.shadps4.emulator.data.native` would clash with the
 * Kotlin `native` keyword if we used it as an identifier, but as a package
 * name it's fine. The JVM class name is what matters for JNI binding:
 *
 *   Java_com_shadps4_emulator_data_native_ShadPs4Native_nativeInstallPkgFull
 */
object ShadPs4Native {

    init {
        // The library name is set in CMakeLists.txt: `add_library(shadps4_native SHARED ...)`.
        System.loadLibrary("shadps4_native")
    }

    /**
     * Parse a PS4 `param.sfo` file from disk.
     *
     * @param absolutePath Absolute filesystem path (e.g. "/sdcard/.../sce_sys/param.sfo").
     *                     On Android 11+, you must have SAF access and resolve the
     *                     underlying path — or copy the file into app-private storage.
     * @return parsed [ParamSfo] or null if the file is missing / not a valid PSF.
     */
    @JvmStatic
    external fun nativeReadParamSfo(absolutePath: String): ParamSfo?

    /**
     * Phase 2: install a PS4 PKG file (metadata only).
     *
     * Extracts ONLY the plaintext `sce_sys/` entries: param.sfo, icon0.png,
     * pic1.png, pic0.png, snd0.at9. No crypto required — works for any PKG
     * including retail NPDRM.
     *
     * @param pkgPath Absolute path to the .pkg file on disk.
     * @param destDir Absolute path of the destination directory. Will be
     *                created if missing. Should be `<filesDir>/games/<TITLE_ID>/sce_sys/`.
     * @return [PkgInstallResult]. Always non-null — check [PkgInstallResult.isSuccess].
     */
    @JvmStatic
    external fun nativeInstallPkg(pkgPath: String, destDir: String): PkgInstallResult

    /**
     * Phase 3: full PKG install (RSA + AES-XTS + zlib, FPKG only).
     *
     * Extracts the COMPLETE PKG including the encrypted PFS body. Uses
     * Crypto++ for RSA-2048 + AES-XTS and zlib for PFSC decompression.
     * Only works for FPKG (DRM-free) packages — retail NPDRM keys aren't
     * bundled for legal reasons.
     *
     * [progressCallback] is invoked (approximately) once per extracted file
     * with (currentFile, totalFiles, currentFilename). The Kotlin side
     * typically forwards this to a SharedFlow for the UI to render a
     * progress bar.
     *
     * @param pkgPath Absolute path to the .pkg file on disk (full size,
     *                no cap — the native side reads it directly via fopen).
     * @param destDir Absolute path of the destination directory. Should be
     *                `<filesDir>/games/<TITLE_ID>/` (without the `/sce_sys`
     *                suffix — the native code creates sce_sys/ inside it AND
     *                writes PFS-extracted files alongside).
     * @return [PkgInstallResult]. Always non-null — check [PkgInstallResult.isSuccess].
     *         On failure, [PkgInstallResult.error] explains which step failed.
     */
    @JvmStatic
    external fun nativeInstallPkgFull(
        pkgPath: String,
        destDir: String,
        progressCallback: PkgProgressCallback,
    ): PkgInstallResult

    /** Returns a version string for the native module. Useful for diagnostics. */
    @JvmStatic
    external fun nativeVersion(): String

    // ─── Convenience wrappers ──────────────────────────────────────────────

    /** True if the native module loaded successfully. */
    val isLoaded: Boolean by lazy {
        try {
            nativeVersion()
            true
        } catch (_: UnsatisfiedLinkError) {
            false
        }
    }
}

/**
 * JNI callback interface for [ShadPs4Native.nativeInstallPkgFull] progress.
 *
 * Implemented as a SAM interface so Kotlin callers can pass a lambda:
 * ```
 * ShadPs4Native.nativeInstallPkgFull(path, dest) { cur, total, name ->
 *     println("Extracting $cur/$total: $name")
 * }
 * ```
 *
 * Note: this MUST be a `fun interface` (SAM) so the JNI side can call it
 * via `CallVoidMethod` on the lambda's synthesized class.
 */
fun interface PkgProgressCallback {
    fun onProgress(current: Int, total: Int, filename: String)
}


