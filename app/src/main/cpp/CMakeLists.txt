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

# ─── Crypto++ (FetchContent + add_subdirectory) ────────────────────────────
# We use FetchContent to clone Crypto++ source, then add_subdirectory() on
# it so its full CMake project (which exports the `cryptopp` target with
# include paths) is built as part of our project. This is more reliable
# than FetchContent_MakeAvailable for cross-compilation to Android.
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    cryptopp_src
    GIT_REPOSITORY https://github.com/weidai11/cryptopp.git
    GIT_TAG        CRYPTOPP_8_9_0
    GIT_SHALLOW    TRUE
)
FetchContent_Populate(cryptopp_src)

# Crypto++ CMake options: disable tests/examples, build static, no ASM
# (ASM on Android ARM64 needs setup we don't want to deal with yet).
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF  CACHE BOOL "" FORCE)
set(BUILD_STATIC ON   CACHE BOOL "" FORCE)
set(cryptopp_DISABLE_ASM ON CACHE BOOL "" FORCE)
set(CRYPTOPP_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(CRYPTOPP_BUILD_SHARED OFF CACHE BOOL "" FORCE)

# Add the Crypto++ subdirectory. This defines the `cryptopp-static` target
# (NOT `cryptopp` — that's the shared lib variant which we disabled).
add_subdirectory(${cryptopp_src_SOURCE_DIR} ${cryptopp_src_BINARY_DIR} EXCLUDE_FROM_ALL)

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

# Link Crypto++ static lib + zlib + android log.
# `cryptopp-static` is the target name defined by Crypto++'s CMakeLists
# when BUILD_STATIC=ON.
target_link_libraries(shadps4_native
    ${LOG_LIB}
    ${ANDROID_LIB}
    ${Z_LIB}
    cryptopp-static
)

# Crypto++ headers live at the root of the cloned source tree.
target_include_directories(shadps4_native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/shadps4
    ${cryptopp_src_SOURCE_DIR}
)
