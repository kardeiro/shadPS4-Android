// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PKG entry-name lookup table. Direct port of shadPS4's
// src/core/file_format/pkg_type.{h,cpp}, kept as a separate translation unit
// so we don't bloat pkg.cpp with 611 entries.
//
// The table is included as a .inc file to keep the C++ source readable.

#pragma once

#include <string_view>
#include "common.h"

/// Retrieves the PKG entry name from its type identifier.
/// Returns an empty string_view if the type is unknown.
std::string_view GetEntryNameByType(u32 type);
