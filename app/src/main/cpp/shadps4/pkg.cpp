// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "pkg.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
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

    // Read the full 0x1000-byte header.
    PkgFile pkg{};
    if (!ReadAt(f, 0, &pkg.header, sizeof(PkgHeader))) {
        return std::nullopt;
    }

    // Validate magic. PS4 PKG magic = 0x7F434E54 ("\x7FCNT").
    // PS3 PKGs use the same magic but a different body layout, so a mismatch
    // here means either (a) not a PKG at all, or (b) a PS3 PKG.
    if (pkg.header.magic != PKG_MAGIC) {
        return std::nullopt;
    }

    pkg.file_size = std::filesystem::file_size(filepath, ec);
    pkg.content_flags = pkg.header.pkg_content_flags;
    pkg.is_drm_free = IsDrmFree(pkg.header.pkg_type);

    // Extract the 36-byte content_id at offset 0x40.
    {
        char cid[sizeof(pkg.header.pkg_content_id) + 1] = {};
        std::memcpy(cid, pkg.header.pkg_content_id, sizeof(pkg.header.pkg_content_id));
        pkg.content_id = std::string(cid);
    }

    // The TITLE_ID is 9 chars starting at offset 7 of content_id (so PKG
    // offset 0x47). Format: "CUSA00207" etc.
    if (pkg.content_id.size() >= 16) {
        pkg.title_id = pkg.content_id.substr(7, 9);
    }

    // Read the entry table.
    const u32 entry_count = pkg.header.pkg_table_entry_count;
    if (entry_count == 0 || entry_count > 4096) {
        // 4096 is an arbitrary sanity upper bound.
        return std::nullopt;
    }

    const u64 table_offset = static_cast<u64>(pkg.header.pkg_table_entry_offset);
    const u64 table_size = static_cast<u64>(entry_count) * sizeof(PkgEntry);
    if (table_offset + table_size > pkg.file_size) {
        return std::nullopt;
    }

    pkg.entries.resize(entry_count);
    if (!ReadAt(f, table_offset, pkg.entries.data(), table_size)) {
        return std::nullopt;
    }

    return pkg;
}

std::optional<std::vector<u8>> ReadEntry(
    const std::filesystem::path& pkg_path,
    const PkgFile& pkg,
    const PkgEntry& entry) {

    // Refuse to read encrypted entries in MVP — they need the full crypto
    // pipeline (RSA + AES-XTS) which we don't have yet.
    if ((entry.flags1 & PKG_ENTRY_FLAG_ENCRYPTED) != 0) {
        return std::nullopt;
    }

    std::ifstream f;
    if (!OpenForRead(pkg_path, f)) {
        return std::nullopt;
    }

    // The entry.offset is ABSOLUTE (from the start of the PKG file), unlike
    // what some older documentation says. This matches the shadPS4Plus
    // implementation: `file.Seek(entry.offset)` after reading the table.
    const u64 absolute_offset = static_cast<u64>(entry.offset);
    const u64 size = static_cast<u64>(entry.size);

    if (absolute_offset + size > pkg.file_size) {
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

    // Create parent directory if missing.
    std::error_code ec;
    std::filesystem::create_directories(dest_path.parent_path(), ec);

    std::ofstream out(dest_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(bytes->data()),
              static_cast<std::streamsize>(bytes->size()));
    return static_cast<bool>(out);
}

std::vector<ExtractedFile> ExtractSceSys(
    const std::filesystem::path& pkg_path,
    const PkgFile& pkg,
    const std::filesystem::path& dest_dir) {

    std::vector<ExtractedFile> extracted;
    extracted.reserve(16);

    std::error_code ec;
    std::filesystem::create_directories(dest_dir, ec);

    for (const auto& entry : pkg.entries) {
        // Skip encrypted entries — they need the crypto pipeline.
        if ((entry.flags1 & PKG_ENTRY_FLAG_ENCRYPTED) != 0) continue;

        // Get the human-readable filename. Unknown entry IDs return an
        // empty string_view — skip those, they're internal bookkeeping
        // (digests, metas, etc.) we don't care about for the MVP.
        auto name_sv = GetEntryNameByType(entry.id);
        if (name_sv.empty()) continue;

        // Skip entries that are clearly not user-facing sce_sys files.
        // The pkg_type table includes "digests", "entry_keys", "metas",
        // "entry_names", etc. — these are internal PKG metadata, not
        // things we want to surface in the library.
        if (entry.id == 0x0001 ||   // digests
            entry.id == 0x0010 ||   // entry_keys (encrypted anyway)
            entry.id == 0x0020 ||   // image_key (encrypted anyway)
            entry.id == 0x0080 ||   // general_digests
            entry.id == 0x0100 ||   // metas
            entry.id == 0x0200) {   // entry_names
            continue;
        }

        std::string filename{name_sv};
        auto dest = dest_dir / filename;
        if (!ExtractEntry(pkg_path, pkg, entry.id, dest)) {
            continue;
        }

        std::error_code sec;
        const u64 size = std::filesystem::file_size(dest, sec);

        extracted.push_back(ExtractedFile{
            .entry_id = entry.id,
            .filename = filename,
            .absolute_path = dest,
            .size = size,
        });
    }

    return extracted;
}

std::string DescribeContentFlags(u32 flags) {
    // Match the flag table from shadPS4Plus's PKG::Open.
    struct FlagEntry { u32 bit; const char* name; };
    static constexpr FlagEntry flags_table[] = {
        {static_cast<u32>(PKGContentFlag::FIRST_PATCH),       "FIRST_PATCH"},
        {static_cast<u32>(PKGContentFlag::PATCHGO),           "PATCHGO"},
        {static_cast<u32>(PKGContentFlag::REMASTER),          "REMASTER"},
        {static_cast<u32>(PKGContentFlag::PS_CLOUD),          "PS_CLOUD"},
        {static_cast<u32>(PKGContentFlag::GD_AC),             "GD_AC"},
        {static_cast<u32>(PKGContentFlag::NON_GAME),          "NON_GAME"},
        {static_cast<u32>(PKGContentFlag::UNKNOWN_0x8000000), "UNKNOWN_0x8000000"},
        {static_cast<u32>(PKGContentFlag::SUBSEQUENT_PATCH),  "SUBSEQUENT_PATCH"},
        {static_cast<u32>(PKGContentFlag::DELTA_PATCH),       "DELTA_PATCH"},
        {static_cast<u32>(PKGContentFlag::CUMULATIVE_PATCH),  "CUMULATIVE_PATCH"},
    };

    std::string result;
    for (const auto& f : flags_table) {
        if ((flags & f.bit) != 0) {
            if (!result.empty()) result += ", ";
            result += f.name;
        }
    }
    if (result.empty()) {
        result = "BASE_GAME";
    }
    return result;
}

}  // namespace Pkg
