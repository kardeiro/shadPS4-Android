// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Android Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// JNI bridge: exposes the PSF (param.sfo) parser to Kotlin.
//
// Kotlin calls:
//     ShadPs4Native.readParamSfo("/sdcard/.../sce_sys/param.sfo")
//   → returns a `ParamSfo?` data class with title, titleId, version, etc.
//
// If the file does not exist or is not a valid PSF, returns null.

#include <jni.h>
#include <android/log.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "shadps4/psf.h"
#include "shadps4/pkg.h"

#define TAG "shadps4-jni"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)

namespace {

// Helper: convert std::optional<std::string_view> into a jstring (or nullptr).
jstring OptToJString(JNIEnv* env, std::optional<std::string_view> opt) {
    if (!opt.has_value() || opt->empty()) return nullptr;
    return env->NewStringUTF(std::string{*opt}.c_str());
}

jstring OptStringToJString(JNIEnv* env, const PSF& psf, std::string_view key) {
    auto v = psf.GetString(key);
    if (!v) return env->NewStringUTF("");
    return env->NewStringUTF(std::string{*v}.c_str());
}

jint OptIntToJInt(const PSF& psf, std::string_view key, jint fallback = 0) {
    auto v = psf.GetInteger(key);
    return v.value_or(fallback);
}

} // namespace

extern "C" JNIEXPORT jobject JNICALL
Java_com_shadps4_emulator_data_native_ShadPs4Native_nativeReadParamSfo(
    JNIEnv* env, jobject /* thiz */, jstring j_path) {

    if (j_path == nullptr) {
        LOGE("nativeReadParamSfo: null path");
        return nullptr;
    }

    const char* path_chars = env->GetStringUTFChars(j_path, nullptr);
    if (path_chars == nullptr) {
        return nullptr;
    }
    std::filesystem::path path{path_chars};
    env->ReleaseStringUTFChars(j_path, path_chars);

    if (!std::filesystem::exists(path)) {
        LOGI("nativeReadParamSfo: file does not exist: %s", path.string().c_str());
        return nullptr;
    }

    PSF psf;
    if (!psf.Open(path)) {
        LOGE("nativeReadParamSfo: failed to parse PSF at %s", path.string().c_str());
        return nullptr;
    }

    // Locate the Kotlin data class `ParamSfo` and its constructor.
    const char* kParamSfoClass =
        "com/shadps4/emulator/data/model/ParamSfo";
    jclass clazz = env->FindClass(kParamSfoClass);
    if (clazz == nullptr) {
        LOGE("nativeReadParamSfo: ParamSfo class not found");
        return nullptr;
    }

    // Constructor signature mirrors ParamSfo.kt data class field order.
    // (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;
    //  Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V
    const char* kCtorSig =
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
        "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
        "Ljava/lang/String;II)V";

    jmethodID ctor = env->GetMethodID(clazz, "<init>", kCtorSig);
    if (ctor == nullptr) {
        LOGE("nativeReadParamSfo: ParamSfo ctor not found — fields changed?");
        env->DeleteLocalRef(clazz);
        return nullptr;
    }

    // Extract fields we care about. These keys are the canonical PS4 param.sfo
    // entries (see PS4 SDK docs / scenps4).
    jstring title        = OptStringToJString(env, psf, "TITLE");
    jstring title_id     = OptStringToJString(env, psf, "TITLE_ID");
    jstring app_ver      = OptStringToJString(env, psf, "APP_VER");
    jstring category     = OptStringToJString(env, psf, "CATEGORY");
    jstring pub_tool_inf = OptStringToJString(env, psf, "PUBTOOLINFO");
    jstring content_id   = OptStringToJString(env, psf, "CONTENT_ID");
    jstring subtitle     = OptStringToJString(env, psf, "SUB_TITLE");

    jint system_ver = OptIntToJInt(psf, "SYSTEM_VER", 0);
    jint attribute  = OptIntToJInt(psf, "ATTRIBUTE", 0);

    jobject result = env->NewObject(
        clazz, ctor,
        title, title_id, app_ver, category,
        pub_tool_inf, content_id, subtitle,
        system_ver, attribute);

    env->DeleteLocalRef(title);
    env->DeleteLocalRef(title_id);
    env->DeleteLocalRef(app_ver);
    env->DeleteLocalRef(category);
    env->DeleteLocalRef(pub_tool_inf);
    env->DeleteLocalRef(content_id);
    env->DeleteLocalRef(subtitle);
    env->DeleteLocalRef(clazz);

    return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_shadps4_emulator_data_native_ShadPs4Native_nativeVersion(
    JNIEnv* env, jobject /* thiz */) {
    return env->NewStringUTF("shadps4-android native v0.3.0 (psf + pkg + pfs full extract)");
}

// ─── PKG installer ──────────────────────────────────────────────────────────
//
// Phase 2: extracts the plaintext `sce_sys/` entries (param.sfo, icon0.png,
// pic1.png, ...) from a PS4 PKG file into a destination directory inside
// app-private storage, then parses the param.sfo to populate the Kotlin
// `ParamSfo` data class.
//
// Returns a `PkgInstallResult` Java object containing:
//   - paramSfo:  the parsed metadata (or null if parsing failed)
//   - iconPath:  absolute path to icon0.png (or null if not extracted)
//   - pic1Path:  absolute path to pic1.png (or null)
//   - destDir:   the directory where sce_sys/ was extracted
//   - entryCount: number of entries extracted
//   - isDrmFree: true if this is an FPKG (DRM-free) PKG
//   - error:     empty string on success, error message otherwise

namespace {

jclass FindClassOrLog(JNIEnv* env, const char* name) {
    jclass c = env->FindClass(name);
    if (c == nullptr) {
        LOGE("JNI: class not found: %s", name);
    }
    return c;
}

} // namespace

extern "C" JNIEXPORT jobject JNICALL
Java_com_shadps4_emulator_data_native_ShadPs4Native_nativeInstallPkg(
    JNIEnv* env, jobject /* thiz */, jstring j_pkg_path, jstring j_dest_dir) {

    const char* pkg_chars = env->GetStringUTFChars(j_pkg_path, nullptr);
    const char* dest_chars = env->GetStringUTFChars(j_dest_dir, nullptr);
    std::filesystem::path pkg_path{pkg_chars};
    std::filesystem::path dest_dir{dest_chars};
    env->ReleaseStringUTFChars(j_pkg_path, pkg_chars);
    env->ReleaseStringUTFChars(j_dest_dir, dest_chars);

    LOGI("nativeInstallPkg: pkg='%s' dest='%s'",
         pkg_path.string().c_str(), dest_dir.string().c_str());

    // Locate the result class + ctor. Must match PkgInstallResult.kt field order.
    const char* kResultClass = "com/shadps4/emulator/data/model/PkgInstallResult";
    jclass result_clazz = FindClassOrLog(env, kResultClass);
    if (result_clazz == nullptr) return nullptr;

    // Ctor sig (note: all String args are nullable now — Kotlin data class
    // declares them as `String?`):
    // (Lcom/shadps4/emulator/data/model/ParamSfo;  // paramSfo
    //  Ljava/lang/String;                          // iconPath
    //  Ljava/lang/String;                          // pic1Path
    //  Ljava/lang/String;                          // destDir
    //  I                                           // entryCount
    //  Z                                           // isDrmFree
    //  Ljava/lang/String;)                         // error (non-null)
    // V
    const char* kCtorSig =
        "(Lcom/shadps4/emulator/data/model/ParamSfo;"
        "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
        "IZLjava/lang/String;)V";
    jmethodID ctor = env->GetMethodID(result_clazz, "<init>", kCtorSig);
    if (ctor == nullptr) {
        LOGE("PkgInstallResult ctor not found");
        env->DeleteLocalRef(result_clazz);
        return nullptr;
    }

    // Helper that builds a `PkgInstallResult` with the given error message
    // (everything else null/0/false). Note: destDir is now nullable on the
    // Kotlin side, so passing nullptr here is safe.
    auto make_error = [&](const std::string& msg) -> jobject {
        LOGE("nativeInstallPkg error: %s", msg.c_str());
        jstring j_msg = env->NewStringUTF(msg.c_str());
        jobject r = env->NewObject(
            result_clazz, ctor,
            /*paramSfo*/ nullptr,
            /*iconPath*/ nullptr,
            /*pic1Path*/ nullptr,
            /*destDir*/ nullptr,
            /*entryCount*/ 0,
            /*isDrmFree*/ JNI_FALSE,
            /*error*/ j_msg);
        env->DeleteLocalRef(j_msg);
        env->DeleteLocalRef(result_clazz);
        return r;
    };

    // ── Step 1: pre-flight diagnostics ───────────────────────────────────
    // Before invoking Pkg::Open, peek at the first 16 bytes so we can give
    // the user a meaningful error message when they pick a file that isn't
    // actually a PS4 PKG (e.g. a zip, an exe, an ELF, or a PS3 PKG which
    // shares the same magic but a different body layout).
    std::error_code ec;
    if (!std::filesystem::exists(pkg_path, ec)) {
        return make_error("PKG file does not exist: " + pkg_path.string());
    }
    const auto file_size = std::filesystem::file_size(pkg_path, ec);
    if (ec || file_size < sizeof(PkgHeader)) {
        return make_error("PKG file is too small (" +
                          std::to_string(file_size) +
                          " bytes) — expected at least 128 bytes for the header.");
    }

    // Read first 16 bytes for diagnostics.
    u8 first_bytes[16]{};
    {
        std::ifstream peek(pkg_path, std::ios::binary);
        peek.read(reinterpret_cast<char*>(first_bytes), sizeof(first_bytes));
    }

    // Log them as hex + ASCII for logcat debugging.
    char hex_buf[64]{};
    char ascii_buf[20]{};
    for (int i = 0; i < 16; i++) {
        std::snprintf(hex_buf + i * 3, 4, "%02X ", first_bytes[i]);
        ascii_buf[i] = (first_bytes[i] >= 0x20 && first_bytes[i] < 0x7F)
                       ? static_cast<char>(first_bytes[i]) : '.';
    }
    LOGI("nativeInstallPkg: first 16 bytes: %s | ASCII: %s | file_size=%llu",
         hex_buf, ascii_buf, static_cast<unsigned long long>(file_size));

    // Detect common non-PKG files by their magic bytes — give the user a
    // friendly error message instead of a generic "bad magic".
    //
    // Note: hex escape sequences in C++ string literals are greedy — they
    // consume as many hex digits as possible. So "\x7FELF" parses as
    // "\x7FE" + "LF" (wrong!) because 'E' is a valid hex digit. We avoid
    // this by using raw byte arrays instead of string literals.
    auto matches = [&](const u8* sig, std::size_t n) -> bool {
        return std::memcmp(first_bytes, sig, n) == 0;
    };
    static constexpr u8 ZIP_MAGIC[4]  = {0x50, 0x4B, 0x03, 0x04};        // "PK\x03\x04"
    static constexpr u8 RAR_MAGIC[4]  = {0x52, 0x61, 0x72, 0x21};        // "Rar!"
    static constexpr u8 SEVENZ_MAGIC[6] = {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C}; // "7z\xBC\xAF\x27\x1C"
    static constexpr u8 ELF_MAGIC[4]  = {0x7F, 0x45, 0x4C, 0x46};        // "\x7FELF"
    static constexpr u8 MSCF_MAGIC[4] = {0x4D, 0x53, 0x43, 0x46};        // "MSCF"
    static constexpr u8 GZIP_MAGIC[2] = {0x1F, 0x8B};                    // gzip
    static constexpr u8 RAR5_MAGIC[7] = {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01}; // "Rar!\x1A\x07\x01"

    if (matches(ZIP_MAGIC, 4)) {
        return make_error("Selected file is a ZIP archive, not a PKG. Unzip it first or pick the .pkg file directly.");
    }
    if (matches(RAR_MAGIC, 4) || matches(RAR5_MAGIC, 7)) {
        return make_error("Selected file is a RAR archive, not a PKG. Extract it first.");
    }
    if (matches(SEVENZ_MAGIC, 6)) {
        return make_error("Selected file is a 7-Zip archive, not a PKG. Extract it first.");
    }
    if (matches(ELF_MAGIC, 4)) {
        return make_error("Selected file is an ELF executable (likely eboot.bin), not a PKG.");
    }
    if (matches(MSCF_MAGIC, 4)) {
        return make_error("Selected file is a Microsoft Cabinet archive, not a PKG.");
    }
    if (matches(GZIP_MAGIC, 2)) {
        return make_error("Selected file is a gzip/tar.gz archive, not a PKG. Extract it first.");
    }

    // ── Step 2: parse the PKG header + entry table ──────────────────────
    auto pkg_opt = Pkg::Open(pkg_path);
    if (!pkg_opt) {
        // Build a precise error showing the actual magic value we read.
        u32 actual_magic = 0;
        std::memcpy(&actual_magic, first_bytes, sizeof(u32));
        char err_buf[256];
        std::snprintf(err_buf, sizeof(err_buf),
                      "Not a valid PS4 PKG file. Expected magic 0x7F434E54 (\"\\x7FCNT\"), "
                      "got 0x%08X. The file may be a PS3 PKG (different layout), "
                      "an ELF/eboot.bin, or simply not a PKG at all.",
                      actual_magic);
        return make_error(err_buf);
    }
    const auto& pkg = *pkg_opt;
    LOGI("nativeInstallPkg: opened PKG — title_id='%s' content_id='%s' entries=%zu "
         "pfs_offset=%llu pfs_size=%llu drm_free=%d flags=0x%08X (%s)",
         pkg.title_id.c_str(),
         pkg.content_id.c_str(),
         pkg.entries.size(),
         static_cast<unsigned long long>(pkg.header.pfs_image_offset),
         static_cast<unsigned long long>(pkg.header.pfs_image_size),
         pkg.is_drm_free ? 1 : 0,
         pkg.content_flags,
         Pkg::DescribeContentFlags(pkg.content_flags).c_str());

    // ── Step 3: extract `sce_sys/` plaintext entries into dest_dir ──────
    std::filesystem::create_directories(dest_dir, ec);
    auto extracted = Pkg::ExtractSceSys(pkg_path, pkg, dest_dir);
    if (extracted.empty()) {
        // Cleanup the partial dest_dir.
        std::error_code rec;
        std::filesystem::remove_all(dest_dir, rec);
        return make_error("PKG had no extractable sce_sys/ entries (file may be a PS3 PKG or a corrupted PS4 PKG).");
    }
    LOGI("nativeInstallPkg: extracted %zu sce_sys files into %s",
         extracted.size(), dest_dir.string().c_str());

    // ── Step 4: parse the extracted param.sfo (if present) ──────────────
    const std::filesystem::path sfo_path = dest_dir / "param.sfo";
    jobject j_psf = nullptr;
    if (std::filesystem::exists(sfo_path)) {
        PSF psf;
        if (psf.Open(sfo_path)) {
            const char* kParamSfoClass = "com/shadps4/emulator/data/model/ParamSfo";
            jclass psf_clazz = FindClassOrLog(env, kParamSfoClass);
            if (psf_clazz != nullptr) {
                const char* kPsfCtorSig =
                    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
                    "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
                    "Ljava/lang/String;II)V";
                jmethodID psf_ctor = env->GetMethodID(psf_clazz, "<init>", kPsfCtorSig);
                if (psf_ctor != nullptr) {
                    auto opt_str = [&](std::string_view k) -> jstring {
                        auto v = psf.GetString(k);
                        return v ? env->NewStringUTF(std::string{*v}.c_str())
                                 : env->NewStringUTF("");
                    };
                    auto opt_int = [&](std::string_view k, jint fb) -> jint {
                        auto v = psf.GetInteger(k);
                        return v.value_or(fb);
                    };
                    jstring j_title        = opt_str("TITLE");
                    jstring j_title_id     = opt_str("TITLE_ID");
                    jstring j_app_ver      = opt_str("APP_VER");
                    jstring j_category     = opt_str("CATEGORY");
                    jstring j_pub_tool_inf = opt_str("PUBTOOLINFO");
                    jstring j_content_id   = opt_str("CONTENT_ID");
                    jstring j_subtitle     = opt_str("SUB_TITLE");
                    jint    j_system_ver   = opt_int("SYSTEM_VER", 0);
                    jint    j_attribute    = opt_int("ATTRIBUTE", 0);

                    j_psf = env->NewObject(
                        psf_clazz, psf_ctor,
                        j_title, j_title_id, j_app_ver, j_category,
                        j_pub_tool_inf, j_content_id, j_subtitle,
                        j_system_ver, j_attribute);

                    env->DeleteLocalRef(j_title);
                    env->DeleteLocalRef(j_title_id);
                    env->DeleteLocalRef(j_app_ver);
                    env->DeleteLocalRef(j_category);
                    env->DeleteLocalRef(j_pub_tool_inf);
                    env->DeleteLocalRef(j_content_id);
                    env->DeleteLocalRef(j_subtitle);
                }
                env->DeleteLocalRef(psf_clazz);
            }
        } else {
            LOGW("nativeInstallPkg: param.sfo exists but failed to parse");
        }
    } else {
        LOGW("nativeInstallPkg: no param.sfo in extracted sce_sys/ entries");
    }

    // ── Step 5: build path strings for icon0.png and pic1.png ───────────
    jstring j_icon = nullptr;
    jstring j_pic1 = nullptr;
    jstring j_dest = env->NewStringUTF(dest_dir.string().c_str());
    for (const auto& e : extracted) {
        if (e.entry_id == ICON0_PNG) {
            j_icon = env->NewStringUTF(e.absolute_path.string().c_str());
        } else if (e.entry_id == PIC1_PNG) {
            j_pic1 = env->NewStringUTF(e.absolute_path.string().c_str());
        }
    }

    jobject result = env->NewObject(
        result_clazz, ctor,
        /*paramSfo*/ j_psf,
        /*iconPath*/ j_icon,
        /*pic1Path*/ j_pic1,
        /*destDir*/ j_dest,
        /*entryCount*/ static_cast<jint>(extracted.size()),
        /*isDrmFree*/ pkg.is_drm_free ? JNI_TRUE : JNI_FALSE,
        /*error*/ env->NewStringUTF(""));

    if (j_psf) env->DeleteLocalRef(j_psf);
    if (j_icon) env->DeleteLocalRef(j_icon);
    if (j_pic1) env->DeleteLocalRef(j_pic1);
    env->DeleteLocalRef(j_dest);
    env->DeleteLocalRef(result_clazz);

    return result;
}

// ─── Phase 3: full PKG install (RSA + AES-XTS + zlib) ──────────────────────
//
// Same return shape as nativeInstallPkg, but performs the full extraction
// pipeline via Pkg::ExtractFull(). The Java side passes a PkgProgressCallback
// SAM lambda that we invoke (via CallVoidMethod) once per extracted file.

extern "C" JNIEXPORT jobject JNICALL
Java_com_shadps4_emulator_data_native_ShadPs4Native_nativeInstallPkgFull(
    JNIEnv* env, jobject /* thiz */, jstring j_pkg_path, jstring j_dest_dir,
    jobject progress_callback) {

    const char* pkg_chars = env->GetStringUTFChars(j_pkg_path, nullptr);
    const char* dest_chars = env->GetStringUTFChars(j_dest_dir, nullptr);
    std::filesystem::path pkg_path{pkg_chars};
    std::filesystem::path dest_dir{dest_chars};
    env->ReleaseStringUTFChars(j_pkg_path, pkg_chars);
    env->ReleaseStringUTFChars(j_dest_dir, dest_chars);

    LOGI("nativeInstallPkgFull: pkg='%s' dest='%s' (NOT BUILT — see CMakeLists)",
         pkg_path.string().c_str(), dest_dir.string().c_str());

    // Look up the PkgInstallResult class + ctor (same signature as Phase 2).
    const char* kResultClass = "com/shadps4/emulator/data/model/PkgInstallResult";
    jclass result_clazz = FindClassOrLog(env, kResultClass);
    if (result_clazz == nullptr) return nullptr;

    const char* kCtorSig =
        "(Lcom/shadps4/emulator/data/model/ParamSfo;"
        "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
        "IZLjava/lang/String;)V";
    jmethodID ctor = env->GetMethodID(result_clazz, "<init>", kCtorSig);
    if (ctor == nullptr) {
        LOGE("PkgInstallResult ctor not found");
        env->DeleteLocalRef(result_clazz);
        return nullptr;
    }

    // Phase 3 full extraction is currently DISABLED at the C++ level
    // (see pkg.cpp #if 0 block — Crypto++ integration with Android NDK
    // has build issues that need a separate fix).
    //
    // We still want the JNI symbol to exist so the Kotlin side doesn't
    // crash with UnsatisfiedLinkError. Return a clear error message.
    (void)progress_callback;  // unused
    (void)pkg_path;
    (void)dest_dir;

    jstring j_msg = env->NewStringUTF(
        "Full PKG extraction is not available in this build. "
        "The Crypto++ library has integration issues with Android NDK "
        "that need to be resolved before PFS decryption can be enabled. "
        "Use 'Install PKG' (metadata only) instead — it works for any "
        "PKG including retail NPDRM."
    );
    jobject r = env->NewObject(
        result_clazz, ctor,
        nullptr, nullptr, nullptr, nullptr, 0, JNI_FALSE, j_msg);
    env->DeleteLocalRef(j_msg);
    env->DeleteLocalRef(result_clazz);
    return r;
}

// Standard JNI initialization hook. We just log — no global state to set up.
extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /* reserved */) {
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    LOGI("JNI_OnLoad: shadps4 native module loaded");
    return JNI_VERSION_1_6;
}
