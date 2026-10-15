#pragma once
// =============================================================================
// GUI::animation::logo_animation --- rainbow hue-cycling logo effect.
//
// Reversed from sub_14000D100:
// - Uses sin-based hue oscillation with custom easing
// - HSV to RGB conversion for smooth rainbow effect
// - Applies when logo is hovered (qword_140246698 flag)
// - Renders grayscale logo with colored tint when active
// =============================================================================

#include <cmath>
#include <cstdint>
#include <chrono>
#include <algorithm>

namespace GUI::animation {

struct logo_animation_state {
    bool enabled = false;      // set true when logo hovered
    bool was_hovered = false;  // previous frame hover state

    // Get the current animated color for the logo.
    // Returns 0xAARRGGBB format.
    std::uint32_t get_color() const {
        if (!enabled)
            return 0xFFFFFFFF; // white/grayscale when not hovered

        // Get time in 100-nanosecond units (matches Xtime_get_ticks)
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(now).count() * 10;

        // Animation formula from binary:
        // v2 = sinf((ticks / 10000 % 2000) * 0.0015707964f)
        // 0.0015707964 ≈ π/2000, so full cycle = 2000 units = 20ms * 2000 = 40 seconds
        float t = static_cast<float>((ticks / 10000) % 2000);
        float v2 = std::sin(t * 0.0015707964f);

        // Custom easing curve for smooth transitions
        float v3;
        if (v2 >= 0.5f) {
            float x = (v2 + v2) - 2.0f;
            v3 = std::pow(x, 2.0f) * (x * 3.5949094f + 2.5949094f) + 2.0f;
        } else {
            v3 = std::pow(v2 + v2, 2.0f) * (v2 * 7.1898189f - 2.5949094f);
        }

        float v4 = v3 * 0.5f;

        // HSV to RGB conversion
        // hue = (v4 * 0.1 + 0.5) wraps through spectrum
        float hue_norm = (v4 * 0.1f + 0.5f);
        float hue_frac = std::fmod(hue_norm * 256.0f * 6.0f, 256.0f * 6.0f);
        int sector = static_cast<int>(hue_frac) / 256;
        int frac = static_cast<int>(hue_frac) % 256;

        std::uint8_t r = 0, g = 0, b = 0;
        switch (sector % 6) {
            case 0: r = 255; g = static_cast<uint8_t>(frac); b = 0; break;
            case 1: r = static_cast<uint8_t>(255 - frac); g = 255; b = 0; break;
            case 2: r = 0; g = 255; b = static_cast<uint8_t>(frac); break;
            case 3: r = 0; g = static_cast<uint8_t>(255 - frac); b = 255; break;
            case 4: r = static_cast<uint8_t>(frac); g = 0; b = 255; break;
            case 5: r = 255; g = 0; b = static_cast<uint8_t>(255 - frac); break;
        }

        // Alpha based on animation intensity
        int alpha_i = static_cast<int>(v4 * 32.0f + 128.0f);
        alpha_i = std::clamp(alpha_i, 128, 255);
        std::uint8_t alpha = static_cast<std::uint8_t>(alpha_i);

        return (static_cast<uint32_t>(alpha) << 24) |
               (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8) |
               static_cast<uint32_t>(b);
    }

    // Called when logo hover state changes
    void set_hovered(bool hovered) {
        was_hovered = enabled;
        enabled = hovered;
    }
};

inline logo_animation_state& logo_anim() {
    static logo_animation_state s;
    return s;
}

// Color interpolation matching sub_14000E750
inline std::uint32_t lerp_color(std::uint32_t c1, std::uint32_t c2, float t) {
    if (t <= 0.0f) return c1;
    if (t >= 1.0f) return c2;
    if (c1 == c2) return c1;

    float inv = 1.0f - t;
    std::uint8_t r = static_cast<uint8_t>((c1 & 0xFF) * inv + (c2 & 0xFF) * t);
    std::uint8_t g = static_cast<uint8_t>(((c1 >> 8) & 0xFF) * inv + ((c2 >> 8) & 0xFF) * t);
    std::uint8_t b = static_cast<uint8_t>(((c1 >> 16) & 0xFF) * inv + ((c2 >> 16) & 0xFF) * t);
    std::uint8_t a = static_cast<uint8_t>(((c1 >> 24) & 0xFF) * inv + ((c2 >> 24) & 0xFF) * t);

    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(g) << 8) | r;
}

} // namespace GUI::animation
