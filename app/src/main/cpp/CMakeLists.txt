# SPDX-FileCopyrightText: Copyright 2025 shadPS4 Android Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Minimal CMake build for the shadPS4 Android native module.
#
# Phase 1: PSF parser.
# Phase 2: PKG sce_sys/ extraction (plaintext only).
# Phase 3 (in progress): full PKG extraction with Crypto++ + zlib.
#
# NOTE: Crypto++ integration with Android NDK has known issues with the
# `cryptopp-cmake` project — its CMakeLists passes flags as a single
# string instead of a list, breaking the NDK clang invocation. The full
# PFS extraction code (crypto.cpp, keys.h, pfs.h) is in place but not
# compiled into the .so yet. We ship the metadata-only installer as
# the working path; full install will be re-enabled in a follow-up
# commit using either a patched Crypto++ or a switch to mbedTLS.

cmake_minimum_required(VERSION 3.22.1)
project(shadps4_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_compile_options(
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-sign-compare
    -Wno-implicit-fallthrough
    -Wno-deprecated-declarations
)

# ─── zlib (system-provided in NDK) ─────────────────────────────────────────
# Android NDK ships zlib as a system library; just link `z`.
# (Kept for when we re-enable full PFS extraction.)

# ─── shadps4_native shared library ─────────────────────────────────────────
# Phase 3 note: crypto.cpp is excluded from the build for now. It's ported
# and ready, but depends on Crypto++ which has integration issues with
# Android NDK that need a separate patch. The pkg.cpp file has the
# ExtractFull() function which is gated behind the same flag — it will
# compile but is not callable from JNI yet (the JNI function
# nativeInstallPkgFull returns an error if called).
add_library(shadps4_native SHARED
    shadps4_jni.cpp
    shadps4/psf.cpp
    shadps4/pkg.cpp
    shadps4/pkg_type.cpp
)

find_library(LOG_LIB log REQUIRED)
find_library(ANDROID_LIB android REQUIRED)
find_library(Z_LIB z REQUIRED)

target_link_libraries(shadps4_native
    ${LOG_LIB}
    ${ANDROID_LIB}
    ${Z_LIB}
)

target_include_directories(shadps4_native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/shadps4
)
