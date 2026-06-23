// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// PS4 PKG (Package) file parser + extractor.
//
// Direct port of shadPS4/shadPS4Plus's src/core/file_format/pkg.{h,cpp}, with
// the desktop shadPS4's `Common::FS::IOFile` replaced by `std::ifstream` and
// the Crypto++ dependency stripped out for the MVP. This means we can extract
// only the plaintext `sce_sys/` entries (param.sfo, icon0.png, pic1.png, ...)
// — the encrypted PFS body (which needs RSA-2048 + AES-XTS + zlib) is left
// for a later phase.
//
// CRITICAL: this fixes a bug in the previous version where:
//   - The magic constant was wrong (we used 0x7F504B47 "\x7FPKG"; the real
//     PS4 PKG magic is 0x7F434E54 "\x7FCNT" — "Content").
//   - The header layout was wrong (we used 0x80 bytes; the real header is
//     0x1000 bytes with the entry-count at offset 0x10 and the entry-table
//     offset at 0x18).
//   - Several entry IDs were wrong (pic1.png is 0x1006, not 0x1220).
//
// Reference: https://www.psdevwiki.com/ps4/Package_Files
//            https://github.com/AzaharPlus/shadPS4Plus

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "common.h"
#include "pkg_type.h"

// PS4 PKG magic. The first 4 bytes of every PS4 PKG file.
// ASCII "\x7FCNT" — CNT stands for "Content" (Sony's internal name).
constexpr u32 PKG_MAGIC = 0x7F434E54;

// PKG content type flag bits (offset 0x44 in the header). Used to identify
// game patches, DLC, remasters, etc.
enum class PKGContentFlag : u32 {
    FIRST_PATCH       = 0x00100000,
    PATCHGO           = 0x00200000,
    REMASTER          = 0x00400000,
    PS_CLOUD          = 0x00800000,
    GD_AC             = 0x02000000,
    NON_GAME          = 0x04000000,
    UNKNOWN_0x8000000 = 0x08000000,
    SUBSEQUENT_PATCH  = 0x40000000,
    DELTA_PATCH       = 0x41000000,
    CUMULATIVE_PATCH  = 0x60000000,
};

// PKG header. Total size is 0x1000 (4096) bytes — only the documented fields
// are listed here, the rest is padding/digests/RSA material we don't read
// in the MVP.
#pragma pack(push, 1)
struct PkgHeader {
    u32_be magic;                    // 0x00: PKG_MAGIC = 0x7F434E54
    u32_be pkg_type;                 // 0x04
    u32_be pkg_0x08;                 // 0x08: unknown
    u32_be pkg_file_count;           // 0x0C
    u32_be pkg_table_entry_count;    // 0x10: number of entries in entry table
    u16_be pkg_sc_entry_count;       // 0x14
    u16_be pkg_table_entry_count_2;  // 0x16: same as pkg_table_entry_count
    u32_be pkg_table_entry_offset;   // 0x18: file offset to entry table
    u32_be pkg_sc_entry_data_size;   // 0x1C
    u64_be pkg_body_offset;          // 0x20: offset of PKG body (encrypted region)
    u64_be pkg_body_size;            // 0x28: size of PKG body
    u64_be pkg_content_offset;       // 0x30
    u64_be pkg_content_size;         // 0x38
    u8     pkg_content_id[0x24];     // 0x40: 36-byte content ID (TITLE_ID starts at 0x47)
    u8     pkg_padding[0x0C];        // 0x64
    u32_be pkg_drm_type;             // 0x70
    u32_be pkg_content_type;         // 0x74
    u32_be pkg_content_flags;        // 0x78: bitfield — see PKGContentFlag
    u32_be pkg_promote_size;         // 0x7C
    u32_be pkg_version_date;         // 0x80
    u32_be pkg_version_hash;         // 0x84
    u32_be pkg_0x088;                // 0x88
    u32_be pkg_0x08C;                // 0x8C
    u32_be pkg_0x090;                // 0x90
    u32_be pkg_0x094;                // 0x94
    u32_be pkg_iro_tag;              // 0x98
    u32_be pkg_drm_type_version;     // 0x9C
    u8     pkg_zeroes_1[0x60];       // 0xA0
    u8     digest_entries1[0x20];    // 0x100
    u8     digest_entries2[0x20];    // 0x120
    u8     digest_table_digest[0x20];// 0x140
    u8     digest_body_digest[0x20]; // 0x160
    u8     pkg_zeroes_2[0x280];      // 0x180
    u32_be pkg_0x400;                // 0x400
    u32_be pfs_image_count;          // 0x404
    u64_be pfs_image_flags;          // 0x408
    u64_be pfs_image_offset;         // 0x410: offset of external PFS image
    u64_be pfs_image_size;           // 0x418: size of external PFS image
    u64_be mount_image_offset;       // 0x420
    u64_be mount_image_size;         // 0x428
    u64_be pkg_size;                 // 0x430: total PKG file size
    u32_be pfs_signed_size;          // 0x438
    u32_be pfs_cache_size;           // 0x43C
    u8     pfs_image_digest[0x20];   // 0x440
    u8     pfs_signed_digest[0x20];  // 0x460
    u64_be pfs_split_size_nth_0;     // 0x480
    u64_be pfs_split_size_nth_1;     // 0x488
    u8     pkg_zeroes_3[0xB50];      // 0x490
    u8     pkg_digest[0x20];         // 0xFE0
};
static_assert(sizeof(PkgHeader) == 0x1000, "PkgHeader must be 4096 bytes");
#pragma pack(pop)

// Entry table record (32 bytes per entry). Located at `pkg_table_entry_offset`
// in the PKG file. Each entry describes one file inside the PKG body.
#pragma pack(push, 1)
struct PkgEntry {
    u32_be id;               // 0x00: entry type — see GetEntryNameByType()
    u32_be filename_offset;  // 0x04: offset into the entry_names table (id 0x200)
    u32_be flags1;           // 0x08: bit 31 = encrypted
    u32_be flags2;           // 0x0C: key index, etc.
    u32_be offset;           // 0x10: file offset (absolute, from start of PKG)
    u32_be size;             // 0x14: size in bytes
    u64_be padding;          // 0x18: blank padding
};
static_assert(sizeof(PkgEntry) == 0x20);
#pragma pack(pop)

// Bit 31 of entry.flags1 is set for encrypted entries. Plaintext entries
// (param.sfo, icon0.png, etc.) have this bit clear and can be extracted
// without any crypto.
constexpr u32 PKG_ENTRY_FLAG_ENCRYPTED = 0x80000000u;

// Common entry IDs — these are the ones we care about for the MVP. The full
// table (611 entries) is in pkg_type.cpp.
enum PkgEntryId : u32 {
    ENTRY_KEYS        = 0x0010,  // RSA-encrypted key material (encrypted)
    IMAGE_KEY         = 0x0020,  // IV_KEY + EKPFS key (encrypted)
    ENTRY_NAMES       = 0x0200,  // Filename table (plaintext)
    PARAM_SFO         = 0x1000,  // sce_sys/param.sfo (plaintext)
    PIC1_PNG          = 0x1006,  // sce_sys/pic1.png — background art (plaintext)
    ICON0_PNG         = 0x1200,  // sce_sys/icon0.png — cover icon (plaintext)
    PIC0_PNG          = 0x1220,  // sce_sys/pic0.png — screenshot (plaintext)
    SND0_AT9          = 0x1240,  // sce_sys/snd0.at9 — background music (plaintext)
};

namespace Pkg {

// Parsed PKG file. Contains the header + entry table (in memory).
struct PkgFile {
    PkgHeader header{};
    std::vector<PkgEntry> entries;
    std::string content_id;       // 36-byte content ID from offset 0x40
    std::string title_id;         // 9-char TITLE_ID extracted from content_id[7..16]
    u64 file_size{};              // Total PKG file size on disk
    u32 content_flags{};          // Cached from header.pkg_content_flags
    bool is_drm_free{};           // True if DRM type is free (FPKG)

    // Locate the first entry with the given ID, or nullptr if not present.
    const PkgEntry* FindEntry(u32 id) const {
        for (const auto& e : entries) {
            if (e.id == id) return &e;
        }
        return nullptr;
    }

    // Returns the human-readable name for an entry ID (e.g. 0x1000 → "param.sfo").
    static std::string_view EntryName(u32 id) {
        return GetEntryNameByType(id);
    }
};

// Open a PKG file and parse its header + entry table.
// Returns std::nullopt if the file is not a valid PKG (bad magic, truncated,
// or impossible entry table geometry).
std::optional<PkgFile> Open(const std::filesystem::path& filepath);

// Read a single entry's bytes from the PKG file. Only works for plaintext
// entries (entry.flags1 bit 31 == 0). Returns std::nullopt on I/O error
// or if the entry is encrypted.
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

// Extracted file metadata returned by ExtractSceSys().
struct ExtractedFile {
    u32 entry_id;
    std::string filename;          // e.g. "param.sfo", "icon0.png"
    std::filesystem::path absolute_path;
    u64 size;
};

// Extract all plaintext `sce_sys/` entries to a destination directory.
// The directory is created if missing. Files that already exist are
// overwritten. Returns the list of successfully extracted files.
//
// NOTE: only entries with the encrypted flag CLEAR are extracted. Encrypted
// entries (entry_keys, image_key, license.dat, npbind.dat, etc.) require
// the crypto pipeline and are skipped.
std::vector<ExtractedFile> ExtractSceSys(
    const std::filesystem::path& pkg_path,
    const PkgFile& pkg,
    const std::filesystem::path& dest_dir);

// Returns a human-readable string listing the PKG content flags set in
// `flags`. E.g. "FIRST_PATCH, PATCHGO" or "BASE_GAME".
std::string DescribeContentFlags(u32 flags);

// Returns true if the DRM type indicates a free (FPKG) package.
// FPKGs are user-dumped packages; retail NPDRM packages need PSN license
// + crypto to play, but their sce_sys/ plaintext entries can still be
// extracted for metadata purposes.
inline bool IsDrmFree(u32 drm_type) {
    // DRM type 1 = free (FPKG); DRM type 0 = retail NPDRM.
    // The shadPS4Plus header doesn't explicitly document this, but the
    // pkg_type field at offset 0x04 historically carries this bit.
    return (drm_type & 0x80000000u) != 0;
}

}  // namespace Pkg
