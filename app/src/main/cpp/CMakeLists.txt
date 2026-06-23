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
# The main weidai11/cryptopp repo doesn't ship a CMakeLists.txt at the
# root — the CMake build lives in a separate repo: cryptopp-cmake.
# We clone both, drop the CMakeLists from cryptopp-cmake into the cryptopp
# source tree, then add_subdirectory().
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Clone the Crypto++ library source (headers + .cpp files).
FetchContent_Declare(
    cryptopp_src
    GIT_REPOSITORY https://github.com/weidai11/cryptopp.git
    GIT_TAG        CRYPTOPP_8_9_0
    GIT_SHALLOW    TRUE
)
FetchContent_Populate(cryptopp_src)

# Clone the CMake build files for Crypto++.
FetchContent_Declare(
    cryptopp_cmake
    GIT_REPOSITORY https://github.com/noloader/cryptopp-cmake.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_Populate(cryptopp_cmake)

# Copy the CMakeLists.txt and supporting .cmake files from cryptopp-cmake
# into the cryptopp source tree so add_subdirectory works.
# (file(COPY ...) doesn't have an OPTIONAL argument — guard with EXISTS.)
file(COPY ${cryptopp_cmake_SOURCE_DIR}/CMakeLists.txt DESTINATION ${cryptopp_src_SOURCE_DIR})
if(EXISTS "${cryptopp_cmake_SOURCE_DIR}/cryptopp-config.cmake")
    file(COPY ${cryptopp_cmake_SOURCE_DIR}/cryptopp-config.cmake DESTINATION ${cryptopp_src_SOURCE_DIR})
endif()

# Crypto++ CMake options: disable tests/examples, build static, no ASM
# (ASM on Android ARM64 needs setup we don't want to deal with yet).
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF  CACHE BOOL "" FORCE)
set(BUILD_STATIC ON   CACHE BOOL "" FORCE)
set(cryptopp_DISABLE_ASM ON CACHE BOOL "" FORCE)
set(CRYPTOPP_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(CRYPTOPP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(CRYPTOPP_BUILD_STATIC ON CACHE BOOL "" FORCE)

# Add Crypto++ as a subdirectory. This defines `cryptopp-static`.
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
