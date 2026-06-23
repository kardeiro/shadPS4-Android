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

#include "shadps4/psf.h"

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
    return env->NewStringUTF("shadps4-android native v0.1.0 (psf parser)");
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
