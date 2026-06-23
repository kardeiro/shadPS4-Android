// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "psf.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>

namespace {
// Log to stderr in debug builds — the JNI layer can also intercept this if
// needed. Kept as a no-op stub so the ported code compiles without pulling
// spdlog + fmt.
template <typename... Args>
void LogError [[maybe_unused]] (Args&&... args) {
    (void)std::initializer_list<int>{((void)args, 0)...};
}
} // namespace

bool PSF::Open(const std::filesystem::path& filepath) {
    using namespace std::chrono;
    if (std::filesystem::exists(filepath)) {
        const auto t = std::filesystem::last_write_time(filepath);
        const auto rel =
            duration_cast<seconds>(t - std::filesystem::file_time_type::clock::now()).count();
        const auto tp = system_clock::to_time_t(system_clock::now() + seconds{rel});
        last_write = system_clock::from_time_t(tp);
    }

    // Replace Common::FS::IOFile with std::ifstream.
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);

    std::vector<u8> psf_buffer(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(psf_buffer.data()), size)) {
        return false;
    }
    return Open(psf_buffer);
}

bool PSF::Open(const std::vector<u8>& psf_buffer) {
    if (psf_buffer.size() < sizeof(PSFHeader)) {
        return false;
    }
    const u8* psf_data = psf_buffer.data();

    entry_list.clear();
    map_binaries.clear();
    map_strings.clear();
    map_integers.clear();

    PSFHeader header{};
    std::memcpy(&header, psf_data, sizeof(header));

    if (header.magic != PSF_MAGIC) {
        return false;
    }
    if (header.version != PSF_VERSION_1_1 && header.version != PSF_VERSION_1_0) {
        return false;
    }

    const std::size_t entries = header.index_table_entries;
    if (psf_buffer.size() < sizeof(PSFHeader) + entries * sizeof(PSFRawEntry)) {
        return false;
    }

    for (u32 i = 0; i < entries; i++) {
        PSFRawEntry raw_entry{};
        std::memcpy(&raw_entry, psf_data + sizeof(PSFHeader) + i * sizeof(PSFRawEntry),
                    sizeof(raw_entry));

        Entry& entry = entry_list.emplace_back();
        const std::size_t key_off = header.key_table_offset + raw_entry.key_offset;
        if (key_off >= psf_buffer.size()) {
            return false;
        }
        entry.key = std::string{reinterpret_cast<const char*>(psf_data + key_off)};
        entry.param_fmt = static_cast<PSFEntryFmt>(raw_entry.param_fmt.Raw());
        entry.max_len = raw_entry.param_max_len;

        const std::size_t data_off = header.data_table_offset + raw_entry.data_offset;
        if (data_off + raw_entry.param_len > psf_buffer.size()) {
            return false;
        }
        const u8* data = psf_data + data_off;

        switch (entry.param_fmt) {
        case PSFEntryFmt::Binary: {
            std::vector<u8> value(raw_entry.param_len);
            std::memcpy(value.data(), data, raw_entry.param_len);
            map_binaries.emplace(i, std::move(value));
        } break;
        case PSFEntryFmt::Text: {
            std::string c_str{reinterpret_cast<const char*>(data)};
            map_strings.emplace(i, std::move(c_str));
        } break;
        case PSFEntryFmt::Integer: {
            if (raw_entry.param_len != sizeof(s32)) {
                return false;
            }
            s32 integer{};
            std::memcpy(&integer, data, sizeof(s32));
            map_integers.emplace(i, integer);
        } break;
        default:
            // Unknown format — skip rather than abort so a corrupt entry
            // doesn't kill the whole parse.
            entry_list.pop_back();
            continue;
        }
    }
    return true;
}

std::optional<std::span<const u8>> PSF::GetBinary(std::string_view key) const {
    const auto [it, index] = FindEntry(key);
    if (it == entry_list.end()) {
        return {};
    }
    if (it->param_fmt != PSFEntryFmt::Binary) {
        return {};
    }
    return std::span{map_binaries.at(index)};
}

std::optional<std::string_view> PSF::GetString(std::string_view key) const {
    const auto [it, index] = FindEntry(key);
    if (it == entry_list.end()) {
        return {};
    }
    if (it->param_fmt != PSFEntryFmt::Text) {
        return {};
    }
    return std::string_view{map_strings.at(index)};
}

std::optional<s32> PSF::GetInteger(std::string_view key) const {
    const auto [it, index] = FindEntry(key);
    if (it == entry_list.end()) {
        return {};
    }
    if (it->param_fmt != PSFEntryFmt::Integer) {
        return {};
    }
    return map_integers.at(index);
}

std::pair<std::vector<PSF::Entry>::const_iterator, std::size_t>
PSF::FindEntry(std::string_view key) const {
    const auto it = std::ranges::find_if(
        entry_list, [&](const auto& e) { return e.key == key; });
    return {it, static_cast<std::size_t>(std::distance(entry_list.begin(), it))};
}
