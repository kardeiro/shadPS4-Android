package com.shadps4.emulator.data.native

import com.shadps4.emulator.data.model.ParamSfo

/**
 * Thin Kotlin façade over the native (C++) module loaded via NDK.
 *
 * Phase 1: PSF (param.sfo) parser only. As we port more subsystems from
 * shadPS4 (ELF loader, shader_recompiler, video_core), we'll add more
 * `native*` declarations here.
 *
 * NOTE: `package com.shadps4.emulator.data.native` would clash with the
 * Kotlin `native` keyword if we used it as an identifier, but as a package
 * name it's fine. The JVM class name is what matters for JNI binding:
 *
 *   Java_com_shadps4_emulator_data_native_ShadPs4Native_nativeReadParamSfo
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
