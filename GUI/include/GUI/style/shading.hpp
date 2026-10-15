#pragma once
// =============================================================================
// GUI::style::shading_t — shadow and gradient parameters
//
// Binary uses sub_1400910A0 for shadow prep and vt[+304]/vt[+520] for gradients.
// See HOW_IT_WORKS.md §9, §26, §29 for details.
// =============================================================================

#include <cstdint>

namespace GUI::style {

struct shading_t {
    // Shadow settings (used by c_menu_window, c_dialog_window, c_context_menu)
    bool   shadows_enabled    = true;   // style +0x498 default ON
    float  shadow_blur        = 8.0f;
    float  shadow_offset_x    = 0.0f;
    float  shadow_offset_y    = 4.0f;
    std::uint32_t shadow_color = 0x40000000;  // 25% black

    // Gradient overlay (title bar accent tint)
    // Binary uses 0x40FFFFFF (25% white) at vt[+304]
    std::uint32_t gradient_overlay_color = 0x40FFFFFF;
    float  gradient_scale     = 0.5f;   // scale / 2 in binary

    // Scrollbar fade gradient (at vt[+520])
    // CORRECTED: 0xFFD8D8DE (solid light grey), NOT alpha-blended white
    std::uint32_t scrollbar_fade_color = 0xFFD8D8DE;

    // Texture slots (from binary — see HOW_IT_WORKS.md §5)
    // qword_140245F90 = chevron (84x48)
    // qword_140245FA0 = accent gradient (320x200)
    // qword_140245FA8 = user icon (48x48)
    // qword_140245F20 = logo (320x200 grayscale, hue-cycles)
};

inline shading_t& shading() {
    static shading_t s;
    return s;
}

// Helper: brighten a color by adding to RGB channels
inline std::uint32_t brighten_color(std::uint32_t color, int amount = 32) {
    auto clamp = [](int v) -> std::uint8_t {
        return static_cast<std::uint8_t>(v > 255 ? 255 : (v < 0 ? 0 : v));
    };

    std::uint8_t a = (color >> 24) & 0xFF;
    std::uint8_t r = clamp(((color >> 16) & 0xFF) + amount);
    std::uint8_t g = clamp(((color >>  8) & 0xFF) + amount);
    std::uint8_t b = clamp(( color        & 0xFF) + amount);

    return (a << 24) | (r << 16) | (g << 8) | b;
}

// Helper: set alpha on a color
inline std::uint32_t set_alpha(std::uint32_t color, std::uint8_t alpha) {
    return (static_cast<std::uint32_t>(alpha) << 24) | (color & 0x00FFFFFF);
}

} // namespace GUI::style
