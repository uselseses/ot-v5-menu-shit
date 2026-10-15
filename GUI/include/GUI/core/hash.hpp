#pragma once
// =============================================================================
// GUI::hash --- FNV-1a 64-bit, used to key each control's persisted
// state. The original uses element hashes for config lookup; the
// changelog confirms (Jul 3: "Fix element hash collision", Jul 29:
// "Reduce memory usage / init time by preventing hash calculations
// (and the storage of) for non-control elements").
//
// Non-control elements skip hashing by overriding c_base_element::
// needs_hash() to return false (label, separator, body, ...).
// =============================================================================

#include <cstdint>
#include <string_view>

namespace GUI {

using hash_t = std::uint64_t;

constexpr hash_t k_hash_offset = 0xcbf29ce484222325ull;
constexpr hash_t k_hash_prime  = 0x100000001b3ull;

constexpr hash_t fnv1a(std::string_view sv, hash_t seed = k_hash_offset) noexcept {
    hash_t h = seed;
    for (char c : sv) {
        h ^= static_cast<unsigned char>(c);
        h *= k_hash_prime;
    }
    return h;
}

// Combine a parent hash with a child name --- stable across runs as
// long as the menu tree shape doesn't change.
constexpr hash_t combine_hash(hash_t parent, std::string_view child) noexcept {
    return fnv1a(child, parent);
}

} // namespace GUI
