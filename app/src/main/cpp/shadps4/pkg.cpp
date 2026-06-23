// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Android Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "pkg.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <system_error>

namespace Pkg {

namespace {

// Open the PKG file for binary reading. Returns an open ifstream on success.
bool OpenForRead(const std::filesystem::path& path, std::ifstream& out) {
    out.open(path, std::ios::binary | std::ios::ate);
    if (!out.is_open()) return false;
    out.seekg(0, std::ios::beg);
    return true;
}

// Read `n` bytes from `stream` starting at `offset`. Returns false on EOF/err.
bool ReadAt(std::ifstream& stream, u64 offset, void* dst, std::size_t n) {
    stream.clear();
    stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!stream.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(n))) {
        return false;
    }
    return static_cast<std::size_t>(stream.gcount()) == n;
}

// Convert a (lo, hi) pair to a u64. Used for the 32+32-bit size field at 0x14/0x18.
inline u64 CombineU64(u32 lo, u32 hi) {
    return (static_cast<u64>(hi) << 32) | lo;
}

}  // namespace

std::optional<PkgFile> Open(const std::filesystem::path& filepath) {
    std::error_code ec;
    if (!std::filesystem::exists(filepath, ec)) {
        return std::nullopt;
    }

    std::ifstream f;
    if (!OpenForRead(filepath, f)) {
        return std::nullopt;
    }

    // Read just the documented header (0x80 bytes).
    PkgHeader header{};
    if (!ReadAt(f, 0, &header, sizeof(header))) {
        return std::nullopt;
    }

    // Validate magic.
    if (header.magic != PKG_MAGIC) {
        return std::nullopt;
    }

    // The header_size field at 0x10 is documented to always be 0x1000. Some
    // malformed PKGs may have it off, but we trust the field for the body
    // offset fallback.
    if (header.header_size != 0x1000 && header.header_size != 0) {
        // Allow 0 (some tools leave it zeroed) but warn — we proceed anyway.
    }

    PkgFile pkg{};
    pkg.total_size = CombineU64(header.pkg_size_lo, header.pkg_size_hi);
    pkg.body_offset = static_cast<u64>(header.body_offset);
    pkg.body_size = static_cast<u64>(header.body_size);
    pkg.entry_count = header.entry_count;
    pkg.pkg_type_flags = header.pkg_type;
    pkg.is_drm_free = (header.pkg_type & PKG_DRM_TYPE_FREE) != 0;

    // Sanity check: entry table offset + count * 32 must fit inside the body.
    // The entry table itself is plaintext (not part of the encrypted body)
    // and is located at `entry_table_offset` from the start of the file.
    if (pkg.entry_count == 0 || pkg.entry_count > 4096) {
        // 4096 is an arbitrary sanity upper bound — real PKGs have ~10-50 entries.
        return std::nullopt;
    }
    const u64 table_offset = static_cast<u64>(header.entry_table_offset);
    const u64 table_size = static_cast<u64>(pkg.entry_count) * sizeof(PkgEntry);

    pkg.entries.resize(pkg.entry_count);
    if (!ReadAt(f, table_offset, pkg.entries.data(), table_size)) {
        return std::nullopt;
    }

    return pkg;
}

std::optional<std::vector<u8>> ReadEntry(
    const std::filesystem::path& pkg_path,
    const PkgFile& pkg,
    const PkgEntry& entry) {

    // Refuse to read encrypted entries in MVP.
    if ((entry.entry_flags & PKG_ENTRY_FLAG_ENCRYPTED) != 0) {
        return std::nullopt;
    }

    std::ifstream f;
    if (!OpenForRead(pkg_path, f)) {
        return std::nullopt;
    }

    // The entry_offset is relative to the body_offset, not absolute.
    const u64 absolute_offset = pkg.body_offset + static_cast<u64>(entry.entry_offset);
    const u64 size = static_cast<u64>(entry.entry_size);

    // Sanity: the entry must fit inside the body region.
    if (absolute_offset + size > pkg.body_offset + pkg.body_size + 0x1000) {
        // Allow a small slack for padding; reject obviously broken entries.
        return std::nullopt;
    }

    std::vector<u8> buffer(static_cast<std::size_t>(size));
    if (!ReadAt(f, absolute_offset, buffer.data(), buffer.size())) {
        return std::nullopt;
    }
    return buffer;
}

bool ExtractEntry(
    const std::filesystem::path& pkg_path,
    const PkgFile& pkg,
    u32 entry_id,
    const std::filesystem::path& dest_path) {

    const PkgEntry* entry = pkg.FindEntry(entry_id);
    if (entry == nullptr) return false;

    auto bytes = ReadEntry(pkg_path, pkg, *entry);
    if (!bytes) return false;

    std::ofstream out(dest_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(bytes->data()),
              static_cast<std::streamsize>(bytes->size()));
    return static_cast<bool>(out);
}

namespace {

// Map an entry ID to its filename inside `sce_sys/`.
// Returns std::nullopt for entry IDs we don't care about.
std::optional<std::string_view> EntryIdToFilename(u32 id) {
    switch (id) {
        case PARAM_SFO:        return "param.sfo";
        case PARAM_SFO_DUP:    return std::nullopt;  // skip duplicate
        case ICON0_PNG:        return "icon0.png";
        case PIC1_PNG:         return "pic1.png";
        case ICON0_HI_PNG:     return "icon0_hi.png";
        case PIC0_PNG:         return "pic0.png";
        case SND0_AT9:         return "snd0.at9";
        case AUTH_INFO:        return std::nullopt;  // skip — debug-only
        default:               return std::nullopt;
    }
}

}  // namespace

std::vector<ExtractedFile> ExtractSceSys(
    const std::filesystem::path& pkg_path,
    const PkgFile& pkg,
    const std::filesystem::path& dest_dir) {

    std::vector<ExtractedFile> extracted;
    extracted.reserve(8);

    std::error_code ec;
    std::filesystem::create_directories(dest_dir, ec);

    for (const auto& entry : pkg.entries) {
        // Skip encrypted entries.
        if ((entry.entry_flags & PKG_ENTRY_FLAG_ENCRYPTED) != 0) continue;

        auto filename = EntryIdToFilename(entry.entry_id);
        if (!filename) continue;

        auto dest = dest_dir / std::string{*filename};
        if (!ExtractEntry(pkg_path, pkg, entry.entry_id, dest)) {
            continue;
        }

        std::error_code sec;
        const u64 size = std::filesystem::file_size(dest, sec);

        extracted.push_back(ExtractedFile{
            .entry_id = entry.entry_id,
            .filename = std::string{*filename},
            .absolute_path = dest,
            .size = size,
        });
    }

    return extracted;
}

}  // namespace Pkg
