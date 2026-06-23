// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Minimal common types for the Android port of shadPS4 file_format parsers.
// This is a standalone replacement for the desktop shadPS4 `common/` headers,
// keeping the same public API (u32, u16, s32, *_be, *_le) so the ported
// file_format/ sources compile unchanged.

#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace Common {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

// Little-endian / big-endian wrappers. On Android ARM64 + x86_64 the host is
// already little-endian, so the byteorder is identity. We keep the wrapper
// types so the ported struct definitions (PSFHeader, PSFRawEntry, ...) match
// the upstream layout bit-for-bit.

namespace detail {

template <typename T>
struct EndianStorage {
    std::array<std::uint8_t, sizeof(T)> raw{};
    T LoadLE() const noexcept {
        T v{};
        std::memcpy(&v, raw.data(), sizeof(T));
        return v;
    }
    T LoadBE() const noexcept {
        T v{};
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            v = static_cast<T>(v | (static_cast<T>(raw[i]) << (8 * (sizeof(T) - 1 - i))));
        }
        return v;
    }
    void StoreLE(T v) noexcept { std::memcpy(raw.data(), &v, sizeof(T)); }
    void StoreBE(T v) noexcept {
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            raw[i] = static_cast<std::uint8_t>(v >> (8 * (sizeof(T) - 1 - i)));
        }
    }
};

} // namespace detail

template <typename T>
struct LittleEndian {
    detail::EndianStorage<T> storage{};

    LittleEndian() = default;
    LittleEndian(T v) noexcept { storage.StoreLE(v); }

    operator T() const noexcept { return storage.LoadLE(); }
    LittleEndian& operator=(T v) noexcept {
        storage.StoreLE(v);
        return *this;
    }

    [[nodiscard]] T Raw() const noexcept { return storage.LoadLE(); }
    void FromRaw(T v) noexcept { storage.StoreLE(v); }
};

template <typename T>
struct BigEndian {
    detail::EndianStorage<T> storage{};

    BigEndian() = default;
    BigEndian(T v) noexcept { storage.StoreBE(v); }

    operator T() const noexcept { return storage.LoadBE(); }
    BigEndian& operator=(T v) noexcept {
        storage.StoreBE(v);
        return *this;
    }

    [[nodiscard]] T Raw() const noexcept { return storage.LoadBE(); }
    void FromRaw(T v) noexcept { storage.StoreBE(v); }
};

using u8_le = LittleEndian<u8>;
using u16_le = LittleEndian<u16>;
using u32_le = LittleEndian<u32>;
using u64_le = LittleEndian<u64>;
using s8_le = LittleEndian<s8>;
using s16_le = LittleEndian<s16>;
using s32_le = LittleEndian<s32>;
using s64_le = LittleEndian<s64>;

using u8_be = BigEndian<u8>;
using u16_be = BigEndian<u16>;
using u32_be = BigEndian<u32>;
using u64_be = BigEndian<u64>;
using s8_be = BigEndian<s8>;
using s16_be = BigEndian<s16>;
using s32_be = BigEndian<s32>;
using s64_be = BigEndian<s64>;

} // namespace Common

// Re-export the typedefs at global scope so ported headers (`u32_le`, `u16_be`
// etc.) compile without namespace qualification, matching upstream.
using Common::u8;
using Common::u16;
using Common::u32;
using Common::u64;
using Common::s8;
using Common::s16;
using Common::s32;
using Common::s64;

using Common::u16_le;
using Common::u16_be;
using Common::u32_le;
using Common::u32_be;
using Common::u64_le;
using Common::u64_be;

// Lightweight replacements for `common/assert.h` and `common/logging/log.h`.
// In the desktop build these route to fmt + spdlog; on Android we just forward
// to android/log.h via the JNI bridge. For the file_format parser we keep them
// no-op-ish (return false / abort on fatal) — the JNI layer reports the
// outcome up to Kotlin.
#include <cassert>
#define ASSERT_MSG(cond, ...) do { if (!(cond)) { assert(false && #cond); } } while (0)
#define UNREACHABLE_MSG(...) do { assert(false && "unreachable"); __builtin_unreachable(); } while (0)
