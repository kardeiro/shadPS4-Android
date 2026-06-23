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
