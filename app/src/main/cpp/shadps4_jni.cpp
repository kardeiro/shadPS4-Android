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
    return env->NewStringUTF("shadps4-android native v0.2.0 (psf + pkg parser)");
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

    // Locate the result class + ctor. Must match PkgInstallResult.kt field order.
    const char* kResultClass = "com/shadps4/emulator/data/model/PkgInstallResult";
    jclass result_clazz = FindClassOrLog(env, kResultClass);
    if (result_clazz == nullptr) return nullptr;

    // Ctor sig:
    // (Lcom/shadps4/emulator/data/model/ParamSfo;  // paramSfo
    //  Ljava/lang/String;                          // iconPath
    //  Ljava/lang/String;                          // pic1Path
    //  Ljava/lang/String;                          // destDir
    //  I                                           // entryCount
    //  Z                                           // isDrmFree
    //  Ljava/lang/String;)                         // error
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
    // (everything else null/0/false).
    auto make_error = [&](const char* msg) -> jobject {
        jstring j_msg = env->NewStringUTF(msg);
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

    // 1. Parse the PKG header + entry table.
    auto pkg_opt = Pkg::Open(pkg_path);
    if (!pkg_opt) {
        LOGE("nativeInstallPkg: not a valid PKG (or unreadable): %s", pkg_path.string().c_str());
        return make_error("Not a valid PKG file (bad magic or truncated).");
    }
    const auto& pkg = *pkg_opt;
    LOGI("nativeInstallPkg: opened PKG with %u entries, body_offset=%llu, drm_free=%d",
         pkg.entry_count,
         static_cast<unsigned long long>(pkg.body_offset),
         pkg.is_drm_free ? 1 : 0);

    // 2. Extract `sce_sys/` plaintext entries into dest_dir.
    std::error_code ec;
    std::filesystem::create_directories(dest_dir, ec);
    auto extracted = Pkg::ExtractSceSys(pkg_path, pkg, dest_dir);
    if (extracted.empty()) {
        return make_error("PKG had no extractable sce_sys/ entries.");
    }
    LOGI("nativeInstallPkg: extracted %zu sce_sys files into %s",
         extracted.size(), dest_dir.string().c_str());

    // 3. Parse the extracted param.sfo (if present).
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
        }
    }

    // 4. Build path strings for icon0.png and pic1.png (if extracted).
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

// Standard JNI initialization hook. We just log — no global state to set up.
extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /* reserved */) {
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    LOGI("JNI_OnLoad: shadps4 native module loaded");
    return JNI_VERSION_1_6;
}
