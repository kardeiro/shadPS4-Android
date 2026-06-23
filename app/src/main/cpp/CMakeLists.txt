# SPDX-FileCopyrightText: Copyright 2025 shadPS4 Android Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Minimal CMake build for the shadPS4 Android native module.
# Phase 1 scope: just the PSF (param.sfo) parser. We'll grow this file as we
# port more subsystems (ELF loader, shader_recompiler, video_core).

cmake_minimum_required(VERSION 3.22.1)
project(shadps4_native LANGUAGES CXX)

# C++23 is what upstream uses; NDK r25+ supports it well enough for our subset.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Hide symbols by default — only JNI exports should be visible.
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Warnings as errors would be too strict for ported code; keep warnings only.
add_compile_options(
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-sign-compare
    -Wno-implicit-fallthrough
)

# Native module shared library, packaged into the APK by AGP.
add_library(shadps4_native SHARED
    shadps4_jni.cpp
    shadps4/psf.cpp
)

# Find the Android log library (android/log.h __android_log_print).
find_library(LOG_LIB log REQUIRED)
find_library(ANDROID_LIB android REQUIRED)

target_link_libraries(shadps4_native
    ${LOG_LIB}
    ${ANDROID_LIB}
)

# Our headers live under cpp/shadps4/ and cpp/. The JNI source uses
# `#include "shadps4/psf.h"` and psf.h uses `#include "common.h"`.
target_include_directories(shadps4_native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/shadps4
)
