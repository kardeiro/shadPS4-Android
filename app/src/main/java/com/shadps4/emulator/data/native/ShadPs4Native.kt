package com.shadps4.emulator.data.native

import com.shadps4.emulator.data.model.ParamSfo
import com.shadps4.emulator.data.model.PkgInstallResult

/**
 * Thin Kotlin façade over the native (C++) module loaded via NDK.
 *
 * Phase 1: PSF (param.sfo) parser.
 * Phase 2: PKG installer (extracts `sce_sys/` plaintext entries only).
 *
 * As we port more subsystems from shadPS4 (ELF loader, shader_recompiler,
 * video_core), we'll add more `native*` declarations here.
 *
 * NOTE: `package com.shadps4.emulator.data.native` would clash with the
 * Kotlin `native` keyword if we used it as an identifier, but as a package
 * name it's fine. The JVM class name is what matters for JNI binding:
 *
 *   Java_com_shadps4_emulator_data_native_ShadPs4Native_nativeInstallPkg
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
     * Install a PS4 PKG file: extract `sce_sys/` plaintext entries
     * (param.sfo, icon0.png, pic1.png) into [destDir], parse param.sfo,
     * and return a structured result.
     *
     * @param pkgPath Absolute path to the .pkg file. Must be readable by the
     *                app — copy from a SAF Uri into cache first if needed.
     * @param destDir Absolute path of the destination directory. Will be
     *                created if missing. Should live under app-private
     *                storage (e.g. `context.filesDir`).
     * @return [PkgInstallResult]. Always non-null — check [PkgInstallResult.isSuccess].
     */
    @JvmStatic
    external fun nativeInstallPkg(pkgPath: String, destDir: String): PkgInstallResult

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

