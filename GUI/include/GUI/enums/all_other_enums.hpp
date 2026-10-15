#pragma once
// all_other_enums.hpp --- enums whose string sources are known but whose
// numbering has not yet been verified against the binary. Each block
// cites the strings recovered from .rdata. To be locked down during
// Phase 8 by following the dropdown initializer xrefs.
#include <cstdint>

namespace GUI {

// "Move condition" dropdown in the antiaim subtab.
enum class move_condition_e : std::uint32_t {
    stand     = 0, move      = 1, duck    = 2,
    air       = 3, slow_walk = 4, fakeduck = 5,
};

enum class lby_mode_e : std::uint32_t {
    normal = 0, opposite = 1, custom = 2,
};

enum class jitter_mode_e : std::uint32_t {
    center = 0, offset = 1, random = 2,
};

enum class smoothing_preset_e : std::uint32_t {
    slow = 0, normal = 1, fastest = 2,
};

enum class weapon_class_e : std::uint32_t {
    pistol = 0, heavy = 1, shotgun = 2, smg = 3,
    rifle  = 4, scout = 5, awp     = 6, auto_ = 7,
};

enum class wear_value_e : std::uint32_t {
    factory_new    = 0,
    minimal_wear   = 1,
    field_tested   = 2,
    well_worn      = 3,
    battle_scarred = 4,
};

enum class exploit_type_e : std::uint32_t {
    reliable   = 0,
    unreliable = 1,
    fast       = 2,
};

enum class trigger_type_e : std::uint32_t {
    aimbot     = 0,
    triggerbot = 1,
};

enum class log_color_e : std::uint32_t {
    console = 0,
    screen  = 1,
};

} // namespace GUI
