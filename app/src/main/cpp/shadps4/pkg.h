// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Android Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 PKG (Package) file parser + extractor.
//
// Based on the public PS4 PKG format spec (PSDevWiki):
//   https://www.psdevwiki.com/ps4/Package_Files
//
// MVP scope: extract ONLY the plaintext `sce_sys/` entries from a PKG
// (param.sfo, icon0.png, pic1.png, etc.). These entries are stored
// unencrypted in the PKG body — identified by entry IDs 0x1000, 0x1200,
// 0x1220, ... — so no AES/RSA/HMAC is required to read them.
//
// Phase 3 (future) will add full PFS extraction with AES-XTS + crypto
// for FPKG/retail NPDRM, using mbedTLS or similar.

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "common.h"

// Magic number at offset 0x00 of every PS4 PKG file.
// ASCII "\x7FPKG" (4F 50 5B E7 in big-endian).
constexpr u32 PKG_MAGIC = 0x7F504B47;

// PKG type / flags bitfield at offset 0x04.
// Bit 0x80000000 = DRM type 1 (free, FPKG), bit 0x00000000 = DRM type 0 (retail NPDRM).
constexpr u32 PKG_DRM_TYPE_FREE = 0x80000000;

// Entry IDs that we care about for the MVP. These are all in the
// `sce_sys/` plaintext region of the PKG body.
enum PkgEntryId : u32 {
    // 0x1000 -sce_sys/param.sfo
    PARAM_SFO = 0x1000,
    // 0x1001 -sce_sys/param.sfo (duplicate, sometimes present)
    PARAM_SFO_DUP = 0x1001,
    // 0x1200 -sce_sys/icon0.png
    ICON0_PNG = 0x1200,
    // 0x1220 -sce_sys/pic1.png (background)
    PIC1_PNG = 0x1220,
    // 0x1240 -sce_sys/icon0_hi.png (high-res icon)
    ICON0_HI_PNG = 0x1240,
    // 0x1260 -sce_sys/pic0.png (small picture / screenshot)
    PIC0_PNG = 0x1260,
    // 0x1280 -sce_sys/snd0.at9 (background music)
    SND0_AT9 = 0x1280,
    // 0x1400 -sce_sys/atauthinfo (debug)
    AUTH_INFO = 0x1400,
};

// PKG header (0x1000 bytes total, only the first 0x80 are documented here —
// the rest is digests, padding and key material we don't need for MVP).
#pragma pack(push, 1)
struct PkgHeader {
    u32_be magic;             // 0x00: PKG_MAGIC
    u32_be pkg_type;          // 0x04: bitfield — bit 31 set = DRM-free (FPKG)
    u32_be pkg_0x08;          // 0x08: unknown / field_revision
    u32_be pkg_0x0c;          // 0x0c: unknown
    u32_be header_size;       // 0x10: total header size (always 0x1000)
    u32_be pkg_size_lo;       // 0x14: total file size (lower 32 bits)
    u32_be pkg_size_hi;       // 0x18: total file size (upper 32 bits) — for >4GB
    u32_be pkg_0x1c;          // 0x1c: unknown
    u32_be entry_count;       // 0x20: number of entries in the entry table
    u32_be entry_table_offset;// 0x24: offset in PKG to entry table
    u8     digest_main[8];    // 0x28: SHA-256 digest of body (8 bytes shown)
    u8     padding_0x30[0x20];// 0x30: digest misc
    u8     digest_body[8];    // 0x50: SHA-256 digest of header
    u8     padding_0x58[0x08];// 0x58
    u64_be body_offset;       // 0x60: where the encrypted/plaintext body starts
    u64_be body_size;         // 0x68: body length
    u8     padding_0x70[0x10];// 0x70
    // ...remaining 0xF80 bytes contain IVs, KEKs, RSA signatures, etc.
    // We don't read them in the MVP.
};
static_assert(sizeof(PkgHeader) == 0x80);
#pragma pack(pop)

// Entry table record (32 bytes per entry).
#pragma pack(push, 1)
struct PkgEntry {
    u32_be entry_id;          // 0x00: see PkgEntryId enum
    u32_be entry_offset;      // 0x04: offset relative to body_offset
    u32_be entry_size;        // 0x08: size of this entry (in bytes)
    u32_be entry_flags;       // 0x0c: bitfield (compression/encryption flags)
    u32_be entry_0x10;        // 0x10: reserved
    u32_be entry_0x14;        // 0x14: reserved
    u32_be entry_0x18;        // 0x18: reserved
    u32_be entry_0x1c;        // 0x1c: reserved
};
static_assert(sizeof(PkgEntry) == 0x20);
#pragma pack(pop)

// Bit 31 of entry_flags is set for encrypted entries. Plaintext entries
// (param.sfo, icon0.png) have this bit clear.
constexpr u32 PKG_ENTRY_FLAG_ENCRYPTED = 0x80000000u;

namespace Pkg {

// Result of opening a PKG file. Contains the parsed header + entry table.
struct PkgFile {
    u64 total_size{};
    u64 body_offset{};
    u64 body_size{};
    u32 entry_count{};
    u32 pkg_type_flags{};
    std::vector<PkgEntry> entries;
    bool is_drm_free{};  // True if `pkg_type_flags & PKG_DRM_TYPE_FREE`

    // Locate the first entry with the given ID, or nullptr if not present.
    const PkgEntry* FindEntry(u32 id) const {
        for (const auto& e : entries) {
            if (e.entry_id == id) return &e;
        }
        return nullptr;
    }
};

// Open a PKG file from disk and parse its header + entry table.
// Returns std::nullopt if the file does not exist or is not a valid PKG.
std::optional<PkgFile> Open(const std::filesystem::path& filepath);

// Read a single entry's bytes from an already-opened PKG file.
// Returns std::nullopt on I/O error or if the entry offset/size is out of range.
// Only works for entries that are NOT encrypted (entry_flags bit 31 clear).
std::optional<std::vector<u8>> ReadEntry(
    const std::filesystem::path& pkg_path,
    const PkgFile& pkg,
    const PkgEntry& entry);

// Convenience: extract a single entry by ID to a destination file.
// Returns true on success.
bool ExtractEntry(
    const std::filesystem::path& pkg_path,
    const PkgFile& pkg,
    u32 entry_id,
    const std::filesystem::path& dest_path);

// Convenience: extract all `sce_sys/` plaintext entries to a destination
// directory (created if missing). Returns the list of (entry_id, file_path)
// pairs that were successfully written. This is the high-level API used
// by the JNI installer.
struct ExtractedFile {
    u32 entry_id;
    std::string filename;  // e.g. "param.sfo", "icon0.png"
    std::filesystem::path absolute_path;
    u64 size;
};

std::vector<ExtractedFile> ExtractSceSys(
    const std::filesystem::path& pkg_path,
    const PkgFile& pkg,
    const std::filesystem::path& dest_dir);

}  // namespace Pkg
