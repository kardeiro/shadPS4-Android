# Add project specific ProGuard rules here.
# You can control the set of applied configuration files using the
# project options.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# Kotlin Serialization
-keepattributes *Annotation*, InnerClasses
-dontnote kotlinx.serialization.AnnotationsKt

-keepclassmembers class kotlinx.serialization.json.** {
    *** Companion;
}
-keepclasseswithmembers class kotlinx.serialization.json.** {
    kotlinx.serialization.KSerializer serializer(...);
}

# Compose
-dontwarn androidx.compose.**

# Coil
-dontwarn coil.**

# ─── JNI bridge ────────────────────────────────────────────────────────────
# Keep the native method declarations so R8 doesn't rename them and break
# the C++ `Java_com_shadps4_emulator_data_native_ShadPs4Native_*` linkage.
-keep class com.shadps4.emulator.data.native.ShadPs4Native {
    public static native <methods>;
}

# Keep the ParamSfo data class + constructor signature that JNI looks up by
# `(Ljava/lang/String;...)V` — R8 must not rename fields or reorder ctor args.
-keep class com.shadps4.emulator.data.model.ParamSfo {
    public <init>(...);
    <fields>;
}

# Keep PkgInstallResult for the same reason (Phase 2 JNI bridge).
-keep class com.shadps4.emulator.data.model.PkgInstallResult {
    public <init>(...);
    <fields>;
}
