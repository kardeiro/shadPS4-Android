# SPDX-FileCopyrightText: Copyright 2025 shadPS4 Android Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Minimal CMake build for the shadPS4 Android native module.
# Phase 1: PSF parser.
# Phase 2: PKG sce_sys/ extraction (plaintext only).
# Phase 3: full PKG extraction with Crypto++ + zlib (PFS decryption).

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

# ─── Crypto++ (FetchContent) ───────────────────────────────────────────────
# We use the official cryptopp CMakeLists; it produces a static `cryptopp`
# library that we link into our shared module.
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Pin Crypto++ to a stable release tag.
FetchContent_Declare(
    cryptopp
    GIT_REPOSITORY https://github.com/weidai11/cryptopp.git
    GIT_TAG        CRYPTOPP_8_9_0
    GIT_SHALLOW    TRUE
)

# Crypto++ CMake options: build only what we need (no tests, no examples).
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF  CACHE BOOL "" FORCE)
set(BUILD_STATIC ON   CACHE BOOL "" FORCE)
set(cryptopp_DISABLE_ASM OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(cryptopp)

# ─── shadps4_native shared library ─────────────────────────────────────────
add_library(shadps4_native SHARED
    shadps4_jni.cpp
    shadps4/psf.cpp
    shadps4/pkg.cpp
    shadps4/pkg_type.cpp
    shadps4/crypto.cpp
)

find_library(LOG_LIB log REQUIRED)
find_library(ANDROID_LIB android REQUIRED)
find_library(Z_LIB z REQUIRED)

target_link_libraries(shadps4_native
    ${LOG_LIB}
    ${ANDROID_LIB}
    ${Z_LIB}
    cryptopp
)

target_include_directories(shadps4_native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/shadps4
)
