// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "pkg.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <zlib.h>

#include "crypto.h"
#include "pfs.h"

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

// ─── Phase 3: full extraction pipeline ────────────────────────────────────
//
// Direct port of shadPS4Plus's PKG::Extract() + PKG::ExtractFiles(), with
// `Common::FS::IOFile` replaced by `std::ifstream`/`std::ofstream` and a
// progress_callback added so the JNI bridge can push updates to Kotlin.

namespace {

// zlib inflate a single block. Returns true on success.
bool DecompressPFSC(std::span<const u8> compressed, std::span<u8> decompressed) {
    z_stream s{};
    s.zalloc = Z_NULL;
    s.zfree = Z_NULL;
    s.opaque = Z_NULL;

    if (inflateInit(&s) != Z_OK) return false;

    s.avail_in = static_cast<uInt>(compressed.size());
    s.next_in = const_cast<Bytef*>(compressed.data());
    s.avail_out = static_cast<uInt>(decompressed.size());
    s.next_out = decompressed.data();

    const int ret = inflate(&s, Z_FINISH);
    inflateEnd(&s);
    return ret == Z_STREAM_END || ret == Z_OK;
}

// Locate the PFSC magic (0x43534650 = "PFSC") inside the decrypted PFS image.
// Returns the absolute offset, or std::nullopt if not found.
std::optional<u64> FindPFSCOffset(std::span<const u8> pfs_image) {
    constexpr u32 PFSC_MAGIC = 0x43534650;
    // shadPS4Plus starts scanning at 0x20000 with 0x10000 stride.
    for (u64 i = 0x20000; i + sizeof(u32) <= pfs_image.size(); i += 0x10000) {
        u32 value = 0;
        std::memcpy(&value, pfs_image.data() + i, sizeof(u32));
        if (value == PFSC_MAGIC) return i;
    }
    return std::nullopt;
}

}  // namespace

std::vector<ExtractedFile> ExtractFull(
    const std::filesystem::path& pkg_path,
    const PkgFile& pkg,
    const std::filesystem::path& dest_dir,
    std::string* fail_reason,
    std::function<void(int, int, std::string_view)> progress_callback) {

    std::vector<ExtractedFile> extracted;

    // First pass: extract all the plaintext sce_sys/ entries (same as
    // ExtractSceSys — we want icon0.png, param.sfo etc. to land in
    // dest_dir/sce_sys/ regardless of whether the PFS extraction succeeds).
    extracted = ExtractSceSys(pkg_path, pkg, dest_dir / "sce_sys");
    if (extracted.empty()) {
        if (fail_reason) *fail_reason = "PKG had no extractable sce_sys/ entries";
        return {};
    }

    // Locate the three encrypted entries we need for the crypto pipeline.
    const PkgEntry* entry_keys_entry = nullptr;
    const PkgEntry* image_key_entry = nullptr;
    for (const auto& e : pkg.entries) {
        if (e.id == 0x0010) entry_keys_entry = &e;
        else if (e.id == 0x0020) image_key_entry = &e;
    }

    if (entry_keys_entry == nullptr || image_key_entry == nullptr) {
        if (fail_reason) *fail_reason = "PKG missing entry_keys (0x10) or image_key (0x20) — required for PFS decryption";
        return extracted;  // Return what we have (sce_sys/ files)
    }

    // Open the PKG file once and keep it open for the duration.
    std::ifstream file(pkg_path, std::ios::binary);
    if (!file.is_open()) {
        if (fail_reason) *fail_reason = "Failed to open PKG file for extraction";
        return extracted;
    }

    auto read_at = [&](u64 offset, void* dst, std::size_t n) -> bool {
        file.clear();
        file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(n))) return false;
        return static_cast<std::size_t>(file.gcount()) == n;
    };

    // ── Step 1: read entry_keys (0x10) ──────────────────────────────────
    // Entry layout: seed_digest (32 bytes) + 7 × digest (32 bytes) + 7 × key (256 bytes)
    std::array<u8, 32> seed_digest{};
    std::array<std::array<u8, 32>, 7> digest1{};
    std::array<std::array<u8, 256>, 7> key1{};

    if (!read_at(entry_keys_entry->offset, seed_digest.data(), 32)) {
        if (fail_reason) *fail_reason = "Failed to read seed_digest from entry_keys";
        return extracted;
    }
    for (int i = 0; i < 7; i++) {
        if (!read_at(entry_keys_entry->offset + 32 + i * 32, digest1[i].data(), 32)) {
            if (fail_reason) *fail_reason = "Failed to read digest1 from entry_keys";
            return extracted;
        }
    }
    for (int i = 0; i < 7; i++) {
        if (!read_at(entry_keys_entry->offset + 32 + 7 * 32 + i * 256, key1[i].data(), 256)) {
            if (fail_reason) *fail_reason = "Failed to read key1 from entry_keys";
            return extracted;
        }
    }

    // ── Step 2: RSA-2048 decrypt key1[3] → dk3 ──────────────────────────
    Crypto crypto;
    std::array<u8, 32> dk3{};
    crypto.RSA2048Decrypt(dk3, key1[3], /*is_dk3=*/true);

    // ── Step 3: read image_key (0x20) and derive ivKey + imgKey + ekpfsKey
    std::array<u8, 256> imgkeydata{};
    if (!read_at(image_key_entry->offset, imgkeydata.data(), 256)) {
        if (fail_reason) *fail_reason = "Failed to read image_key";
        return extracted;
    }

    // ivKey = SHA256(PkgEntry || dk3). The "PkgEntry" here is the raw 32-byte
    // PkgEntry struct for the image_key entry, concatenated with dk3.
    std::array<u8, 64> concat_ivkey_dk3{};
    std::memcpy(concat_ivkey_dk3.data(), image_key_entry, sizeof(PkgEntry));
    std::memcpy(concat_ivkey_dk3.data() + sizeof(PkgEntry), dk3.data(), dk3.size());

    std::array<u8, 32> ivKey{};
    crypto.ivKeyHASH256(concat_ivkey_dk3, ivKey);

    // imgKey = AES-CBC-CFB-128 decrypt imgkeydata with ivKey.
    std::array<u8, 256> imgKey{};
    crypto.aesCbcCfb128Decrypt(ivKey, imgkeydata, imgKey);

    // ekpfsKey = RSA-2048 decrypt imgKey (using FakeKeyset, not DK3 keyset).
    std::array<u8, 32> ekpfsKey{};
    crypto.RSA2048Decrypt(ekpfsKey, imgKey, /*is_dk3=*/false);

    // ── Step 4: read seed at pfs_image_offset + 0x370 ───────────────────
    std::array<u8, 16> seed{};
    const u64 pfs_off = pkg.header.pfs_image_offset;
    if (!read_at(pfs_off + 0x370, seed.data(), 16)) {
        if (fail_reason) *fail_reason = "Failed to read PFS seed";
        return extracted;
    }

    // ── Step 5: derive dataKey + tweakKey from ekpfsKey + seed ──────────
    std::array<u8, 16> dataKey{};
    std::array<u8, 16> tweakKey{};
    crypto.PfsGenCryptoKey(ekpfsKey, seed, dataKey, tweakKey);

    // ── Step 6: decrypt the PFS image (only the cache-sized prefix) ─────
    // shadPS4Plus uses `pkgheader.pfs_cache_size * 0x2` as the decryption
    // length. This contains the superblock + inode table + first dirents.
    const u32 length = pkg.header.pfs_cache_size * 2;
    if (length == 0) {
        if (fail_reason) *fail_reason = "pfs_cache_size is 0";
        return extracted;
    }

    std::vector<u8> pfs_encrypted(length);
    if (!read_at(pfs_off, pfs_encrypted.data(), length)) {
        if (fail_reason) *fail_reason = "Failed to read PFS image";
        return extracted;
    }

    std::vector<u8> pfs_decrypted(length);
    crypto.decryptPFS(dataKey, tweakKey, pfs_encrypted, pfs_decrypted, /*sector=*/0);

    // ── Step 7: find PFSC inside the decrypted PFS image ───────────────
    auto pfsc_offset_opt = FindPFSCOffset(pfs_decrypted);
    if (!pfsc_offset_opt.has_value()) {
        if (fail_reason) *fail_reason = "PFSC magic not found in decrypted PFS image";
        return extracted;
    }
    const u64 pfsc_offset = *pfsc_offset_opt;

    // Parse the PFSCHdr at the start of the PFSC block.
    PFSCHdr pfsc_hdr{};
    if (pfsc_offset + sizeof(PFSCHdr) > pfs_decrypted.size()) {
        if (fail_reason) *fail_reason = "PFSC header out of bounds";
        return extracted;
    }
    std::memcpy(&pfsc_hdr, pfs_decrypted.data() + pfsc_offset, sizeof(PFSCHdr));

    const int num_blocks = static_cast<int>(pfsc_hdr.data_length / pfsc_hdr.block_sz2);
    if (num_blocks <= 0) {
        if (fail_reason) *fail_reason = "PFSC has zero blocks";
        return extracted;
    }

    // Build the sector map (offsets of each compressed block within the PFSC region).
    std::vector<u64> sector_map(num_blocks + 1);
    for (int i = 0; i < num_blocks + 1; i++) {
        const u64 read_off = pfsc_offset + pfsc_hdr.block_offsets + static_cast<u64>(i) * 8;
        if (read_off + 8 > pfs_decrypted.size()) {
            if (fail_reason) *fail_reason = "PFSC sector map out of bounds";
            return extracted;
        }
        std::memcpy(&sector_map[i], pfs_decrypted.data() + read_off, 8);
    }

    // ── Step 8: scan PFSC blocks to extract inode table + dirent table ─
    std::vector<Inode> inode_buf;
    std::unordered_map<int, std::filesystem::path> extract_paths;
    u32 ndinode = 0;
    int ndinode_counter = 0;
    bool dinode_reached = false;
    bool uroot_reached = false;
    int ent_size = 0;

    std::vector<u8> decompressed_block(0x10000);

    // Compute how many blocks the inode table occupies.
    // (We need to do a first pass through block 0 to know ndinode.)
    for (int blk = 0; blk < num_blocks; blk++) {
        const u64 sector_offset = sector_map[blk];
        const u64 sector_size = sector_map[blk + 1] - sector_offset;
        if (sector_size > 0x10000) continue;  // safety

        std::vector<u8> compressed(sector_size);
        if (pfsc_offset + sector_offset + sector_size > pfs_decrypted.size()) break;
        std::memcpy(compressed.data(),
                    pfs_decrypted.data() + pfsc_offset + sector_offset,
                    sector_size);

        if (sector_size == 0x10000) {
            std::memcpy(decompressed_block.data(), compressed.data(), 0x10000);
        } else if (sector_size < 0x10000) {
            if (!DecompressPFSC(compressed, decompressed_block)) continue;
        } else {
            continue;
        }

        if (blk == 0) {
            std::memcpy(&ndinode, decompressed_block.data() + 0x30, 4);
        }

        int occupied_blocks = static_cast<int>((ndinode * 0xA8) / 0x10000);
        if ((ndinode * 0xA8) % 0x10000 != 0) occupied_blocks += 1;

        if (blk >= 1 && blk <= occupied_blocks) {
            for (int p = 0; p < 0x10000; p += 0xA8) {
                Inode node;
                std::memcpy(&node, decompressed_block.data() + p, sizeof(node));
                if (node.Mode == 0) break;
                inode_buf.push_back(node);
            }
        }

        // Look for "flat_path_table" → uroot region.
        const std::string_view flat_path_table(
            reinterpret_cast<const char*>(decompressed_block.data() + 0x10), 15);
        if (flat_path_table == "flat_path_table") {
            uroot_reached = true;
        }

        if (uroot_reached) {
            for (int i = 0; i < 0x10000; i += ent_size) {
                Dirent dirent;
                std::memcpy(&dirent, decompressed_block.data() + i, sizeof(dirent));
                ent_size = dirent.entsize;
                if (dirent.ino != 0) {
                    ndinode_counter++;
                } else {
                    auto parent_path = dest_dir.parent_path();
                    auto title_id = std::string_view(
                        reinterpret_cast<const char*>(pkg.header.pkg_content_id + 7), 9);
                    if (parent_path.filename() != std::filesystem::path(title_id) &&
                        !dest_dir.string().ends_with("-patch")) {
                        extract_paths[ndinode_counter] = parent_path / std::string(title_id);
                    } else {
                        extract_paths[ndinode_counter] = dest_dir;
                    }
                    uroot_reached = false;
                    break;
                }
            }
        }

        const char dot = static_cast<char>(decompressed_block[0x10]);
        const std::string_view dotdot(
            reinterpret_cast<const char*>(decompressed_block.data() + 0x28), 2);
        if (dot == '.' && dotdot == "..") {
            dinode_reached = true;
        }

        bool end_reached = false;
        std::filesystem::path current_dir;
        if (dinode_reached) {
            for (int j = 0; j < 0x10000; j += ent_size) {
                Dirent dirent;
                std::memcpy(&dirent, decompressed_block.data() + j, sizeof(dirent));
                if (dirent.ino == 0) break;
                ent_size = dirent.entsize;

                std::string name(dirent.name, dirent.namelen);
                if (dirent.type == PFS_CURRENT_DIR) {
                    current_dir = extract_paths[dirent.ino];
                }
                extract_paths[dirent.ino] = current_dir / name;

                if (dirent.type == PFS_FILE || dirent.type == PFS_DIR) {
                    if (dirent.type == PFS_DIR) {
                        std::error_code ec;
                        std::filesystem::create_directory(extract_paths[dirent.ino], ec);
                    }
                    ndinode_counter++;
                    if (ndinode_counter + 1 == static_cast<int>(ndinode)) {
                        end_reached = true;
                    }
                }
            }
            if (end_reached) break;
        }
    }

    // ── Step 9: extract individual files from PFSC ─────────────────────
    // For each PFS_FILE inode, decrypt + decompress each of its blocks and
    // write to disk. This is the expensive part for big games.
    int file_index = 0;
    int total_files = 0;
    for (const auto& [ino, path] : extract_paths) {
        if (std::filesystem::is_directory(path)) continue;
        total_files++;
    }

    for (const auto& [ino, path] : extract_paths) {
        if (ino < 0 || static_cast<size_t>(ino) >= inode_buf.size()) continue;
        const auto& node = inode_buf[ino];
        if (node.Mode == 0) continue;

        // Only regular files (InodeMode::file = 0x8000).
        if ((node.Mode & 0xF000) != 0x8000) continue;

        const int sector_loc = node.loc;
        const int nblocks = node.Blocks;
        const int bsize = static_cast<int>(node.Size);
        if (nblocks <= 0) continue;

        if (progress_callback) {
            progress_callback(file_index + 1, total_files, path.filename().string());
        }
        file_index++;

        std::ofstream out_file(path, std::ios::binary | std::ios::trunc);
        if (!out_file.is_open()) continue;

        // Buffer for one decrypted+decompressed 0x10000 block.
        std::vector<u8> pfsc_buf(0x11000);
        std::vector<u8> pfs_dec_buf(0x11000);
        std::vector<u8> decompressed(0x10000);
        int size_decompressed = 0;

        for (int j = 0; j < nblocks; j++) {
            const u64 sector_offset = sector_map[sector_loc + j];
            const u64 sector_size = sector_map[sector_loc + j + 1] - sector_offset;
            const u64 file_offset = pfs_off + pfsc_offset + sector_offset;
            const u64 current_sector = (pfsc_offset + sector_offset) / 0x1000;

            const int sector_offset_mask = static_cast<int>((sector_offset + pfsc_offset) & 0xFFFFF000);
            const int previous_data = static_cast<int>((sector_offset + pfsc_offset) - sector_offset_mask);

            // Read 0x11000 bytes starting just before the target sector, so
            // we can decrypt on a 0x1000 boundary.
            const u64 read_off = (file_offset > static_cast<u64>(previous_data))
                                 ? file_offset - previous_data : 0;
            if (!read_at(read_off, pfsc_buf.data(), pfsc_buf.size())) break;

            crypto.decryptPFS(dataKey, tweakKey, pfsc_buf, pfs_dec_buf, current_sector);

            std::vector<u8> compressed(sector_size);
            std::memcpy(compressed.data(), pfs_dec_buf.data() + previous_data, sector_size);

            if (sector_size == 0x10000) {
                std::memcpy(decompressed.data(), compressed.data(), 0x10000);
            } else if (sector_size < 0x10000) {
                if (!DecompressPFSC(compressed, decompressed)) break;
            } else {
                break;
            }

            size_decompressed += 0x10000;
            if (j < nblocks - 1) {
                out_file.write(reinterpret_cast<const char*>(decompressed.data()), 0x10000);
            } else {
                const int write_size = 0x10000 - (size_decompressed - bsize);
                if (write_size > 0) {
                    out_file.write(reinterpret_cast<const char*>(decompressed.data()), write_size);
                }
            }
        }

        std::error_code sec;
        const u64 sz = std::filesystem::file_size(path, sec);
        extracted.push_back(ExtractedFile{
            .entry_id = 0xFFFFFFFF,  // PFS-extracted file, no PKG entry ID
            .filename = path.filename().string(),
            .absolute_path = path,
            .size = sz,
        });
    }

    if (progress_callback) {
        progress_callback(total_files, total_files, "");
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
