#pragma once
// =============================================================================
// GUI::style::rounding_t --- per-widget corner-rounding TOGGLES.
//
// Reversed from gui_build_menu.c:1374+ and HOW_IT_WORKS.md §"Rounding/shadow
// toggle flags": the binary's "Element rounding" group is an "Enable all"
// checkbox plus one CHECKBOX per widget, each bound to a thread_safe_t<bool>
// global (g_style_button_rounded_enabled @ +0x3F8 ... +0x528). All default ON.
//
// The actual corner radius is NOT stored per widget. c_menu_window__render
// (HOW_IT_WORKS.md §9) computes one shared radius:
//
//     float anim   = 25.0f * (style.anim_speed + 3);   // gui-scaling raw, 1 -> 100%
//     float scale  = anim * 0.01f;
//     int rounding = (int)floorf(scale * style.anim_scale);   // anim_scale = 2.75f
//
// i.e. R = floor(gui_scale * 2.75) -> 2 px at 100% scale. Widgets whose toggle
// is off pass radius 0. Some draws scale R (content area uses 2*R).
// =============================================================================

#include <GUI/style/element_sizes.hpp>

#include <cmath>

namespace GUI::style {

struct rounding_t {
    bool button         = true;
    bool button_grid    = true;
    bool checkbox       = true;
    bool colorpicker    = true;
    bool dialog_window  = true;
    bool context_menu   = true;
    bool groupbox       = true;
    bool dropdown       = true;
    bool table          = true;
    bool keybinder      = true;
    bool menu_window    = true;
    bool listbox        = true;
    bool text_container = true;
    bool page_manager   = true;
    bool slider         = true;
    bool scrollbar      = true;
    bool textbox        = true;
    bool tab_manager    = true;
    bool watermark      = true;
    bool spectator_list = true;
    bool keybind_list   = true;

    // style.anim_scale @ +0x3E8, default 2.75f — the base radius multiplier.
    float anim_scale = 2.75f;

    // Shared radius: floor(gui_scale * anim_scale). Integer-valued by design
    // (the binary truncates), which keeps corner arcs on pixel boundaries.
    float radius() const {
        return std::floor(gui_scale() * anim_scale);
    }

    // Radius for a widget gated by its toggle.
    float of(bool enabled) const {
        return enabled ? radius() : 0.0f;
    }

    // gui_push_clip_animated @ 0x1400910A0 (menu chrome border rings):
    //   radius = floor(scale * g_anim_factor) - floor(scale * 0.25f)
    // g_anim_factor IS anim_scale (style +0x3E8), i.e. the
    // Settings>Rounding "Factor" slider drives the menu border too —
    // exactly like the original (2 px at the 2.75 default, 100% scale).
    float chrome_radius() const {
        const float s = gui_scale();
        return std::max(0.0f, std::floor(s * anim_scale)
                            - std::floor(s * 0.25f));
    }

    void set_all(bool on) {
        button = button_grid = checkbox = colorpicker = dialog_window =
        context_menu = groupbox = dropdown = table = keybinder =
        menu_window = listbox = text_container = page_manager = slider =
        scrollbar = textbox = tab_manager = watermark = spectator_list =
        keybind_list = on;
    }
};

inline rounding_t& rounding() {
    static rounding_t r;
    return r;
}

} // namespace GUI::style
