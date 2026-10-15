#pragma once
// =============================================================================
// GUI::animation — exact animation functions from binary
//
// See HOW_IT_WORKS.md §28 for full animation system details.
//
// Key functions:
//   ease_in_out_cubic @ 0x14000E600
//   lerp_color @ 0x14008E9B0
//   anim_calc_progress_4ch @ 0x14008E690
//   anim_blend_colors_multi @ 0x14008EA70
// =============================================================================

#include <cstdint>
#include <cmath>

namespace GUI::animation {

// =============================================================================
// Easing — exact from binary @ 0x14000E600
// =============================================================================
// Cubic ease in-out: 4t^3 for t<0.5, 1-pow(2-2t,3)/2 for t>=0.5
inline float ease_in_out_cubic(float t) {
    if (t >= 0.5f)
        return 1.0f - (std::pow(2.0f - (t + t), 3.0f) * 0.5f);
    else
        return ((t * 4.0f) * t) * t;
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float clamp01(float t) {
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;
    return t;
}

// =============================================================================
// Color interpolation — from binary @ 0x14008E9B0
// =============================================================================
// Colors are packed 0xAARRGGBB
inline std::uint32_t lerp_color(std::uint32_t c1, std::uint32_t c2, float t) {
    if (t <= 0.0f) return c1;
    if (t >= 1.0f) return c2;

    std::uint8_t a1 = (c1 >> 24) & 0xFF, a2 = (c2 >> 24) & 0xFF;
    std::uint8_t r1 = (c1 >> 16) & 0xFF, r2 = (c2 >> 16) & 0xFF;
    std::uint8_t g1 = (c1 >>  8) & 0xFF, g2 = (c2 >>  8) & 0xFF;
    std::uint8_t b1 =  c1        & 0xFF, b2 =  c2        & 0xFF;

    auto l = [t](std::uint8_t x, std::uint8_t y) -> std::uint8_t {
        return static_cast<std::uint8_t>(x + (y - x) * t);
    };

    return (l(a1, a2) << 24) | (l(r1, r2) << 16) | (l(g1, g2) << 8) | l(b1, b2);
}

// =============================================================================
// Animation base formula — from per-frame loop @ 0x1400077C0
// =============================================================================
// base = 25.0 * (anim_speed + 3)  // default: 25*(1+3) = 100
// scaled = base * 0.01            // default: 1.0
//
// Per-subsystem multipliers (verified literals):
//   unicode_demo_y = floor(scaled * 175)  = 175 px
//   unicode_step   = floor(scaled * 20)   = 20 px
//   unicode_line   = floor(scaled * 12)   = 12 px
//   logo_x_offset  = floor(scaled * 10)   = 10 px
//   logo_width     = floor(scaled * 180)  = 180 px
//   logo_height    = floor(scaled * 40)   = 40 px

inline float calc_anim_base(int anim_speed = 1) {
    return 25.0f * (anim_speed + 3);
}

inline float calc_anim_scale(int anim_speed = 1) {
    return calc_anim_base(anim_speed) * 0.01f;
}

// Rounding calculation (from c_menu_window__render):
//   float anim = 25.0 * (style.anim_speed + 3);
//   float scale = anim * 0.01;
//   int rounding = (int)floor(scale * style.anim_scale);  // anim_scale default 2.75
//   int alt_r = (int)floor(scale * 0.5);
inline int calc_rounding(float anim_scale = 2.75f, int anim_speed = 1) {
    float scale = calc_anim_scale(anim_speed);
    return static_cast<int>(std::floor(scale * anim_scale));
}

// =============================================================================
// Animation progress (4-channel) — from binary @ 0x14008E690
// =============================================================================
// Used by checkbox, slider, button for multi-state color blending
// Returns 4 floats: [hover_in, hover_out, active_in, active_out]

struct anim_progress_4ch {
    float hover_in  = 0.0f;
    float hover_out = 0.0f;
    float active_in = 0.0f;
    float active_out = 0.0f;
};

inline anim_progress_4ch calc_progress_4ch(
    std::uint64_t current_time_ms,
    std::uint64_t hover_start,
    std::uint64_t hover_end,
    std::uint64_t active_start,
    std::uint64_t active_end,
    int duration_ms = 200
) {
    anim_progress_4ch result;
    if (duration_ms <= 0) return result;

    float d = static_cast<float>(duration_ms);

    // Hover in
    if (current_time_ms >= hover_start && current_time_ms < hover_start + duration_ms) {
        float t = static_cast<float>(current_time_ms - hover_start) / d;
        result.hover_in = ease_in_out_cubic(clamp01(t));
    } else if (current_time_ms >= hover_start + duration_ms) {
        result.hover_in = 1.0f;
    }

    // Hover out
    if (current_time_ms >= hover_end && current_time_ms < hover_end + duration_ms) {
        float t = static_cast<float>(current_time_ms - hover_end) / d;
        result.hover_out = ease_in_out_cubic(clamp01(t));
    } else if (current_time_ms >= hover_end + duration_ms) {
        result.hover_out = 1.0f;
    }

    // Active in
    if (current_time_ms >= active_start && current_time_ms < active_start + duration_ms) {
        float t = static_cast<float>(current_time_ms - active_start) / d;
        result.active_in = ease_in_out_cubic(clamp01(t));
    } else if (current_time_ms >= active_start + duration_ms) {
        result.active_in = 1.0f;
    }

    // Active out
    if (current_time_ms >= active_end && current_time_ms < active_end + duration_ms) {
        float t = static_cast<float>(current_time_ms - active_end) / d;
        result.active_out = ease_in_out_cubic(clamp01(t));
    } else if (current_time_ms >= active_end + duration_ms) {
        result.active_out = 1.0f;
    }

    return result;
}

// =============================================================================
// Multi-color blend — from binary @ 0x14008EA70
// =============================================================================
// Blends between 4 colors based on animation progress:
//   inactive, hover, active, active_bright
//
// Used by checkbox (border), slider (track), button (outline)

inline std::uint32_t blend_colors_multi(
    const anim_progress_4ch& prog,
    std::uint32_t inactive,     // e.g., 0xFF3E3E42
    std::uint32_t hover,        // e.g., 0xFF606064
    std::uint32_t active,       // e.g., accent
    std::uint32_t active_bright // e.g., accent | 0x20202020
) {
    // Start from inactive
    std::uint32_t result = inactive;

    // Blend toward hover based on hover_in (and back via hover_out)
    float hover_t = prog.hover_in * (1.0f - prog.hover_out);
    result = lerp_color(result, hover, hover_t);

    // Blend toward active based on active_in
    result = lerp_color(result, active, prog.active_in);

    // Blend toward bright variant on sustained active
    if (prog.active_in >= 1.0f && prog.active_out < 1.0f) {
        result = lerp_color(result, active_bright, 0.5f * (1.0f - prog.active_out));
    }

    return result;
}

} // namespace GUI::animation
