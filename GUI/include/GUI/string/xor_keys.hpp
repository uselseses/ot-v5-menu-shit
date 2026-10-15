#pragma once
// =============================================================================
// xor_keys --- compile-time key material for the multi-key XOR string
// scheme. The original binary's changelog calls out:
//
//   Jul 1   : "String encryption"
//   Jul 6   : "Overall improvements to string encryption"
//   Aug ... : "stronger xor string encryption (multiple keys)"
//
// We use four 64-bit constants and rotate through them by byte index.
// These are placeholders --- swap per release build.
// =============================================================================

#include <cstddef>
#include <cstdint>

namespace GUI::string_ {

inline constexpr std::uint64_t k_xor_keys[4] = {
    0xA3F2C8D1E7B49056ull,
    0x5D7E1F03B92A48C7ull,
    0xCB6041DE2F873A95ull,
    0x91842EA70C3D5BF6ull,
};

// Mix a byte-index into a 1-byte key drawn from k_xor_keys.
constexpr std::uint8_t key_byte(std::size_t pos) noexcept {
    const std::uint64_t k = k_xor_keys[(pos >> 3) & 0x3u];
    return static_cast<std::uint8_t>((k >> ((pos & 7u) * 8u)) & 0xFFu);
}

} // namespace GUI::string_
