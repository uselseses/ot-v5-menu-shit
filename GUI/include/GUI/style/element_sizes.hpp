#pragma once
// =============================================================================
// GUI::style::element_sizes_t
//
// EXACT values from binary style struct at 0x140246000.
// Extracted via IDA decompile of initializer functions sub_140001010..sub_140005900.
// See HOW_IT_WORKS.md §5 for full offset table.
// =============================================================================

#include <functional>
#include <vector>

namespace GUI::style {

inline std::vector<std::function<void(float)>>& scale_callbacks_ref();

struct element_sizes_t {
    // --- Window metrics (0x098-0x0E0) ---
    float outer_padding        = 20.0f;   // +0x098
    float footer_margin        = 100.0f;  // +0x0B0 — logo area height
    float menu_window_w        = 800.0f;  // +0x0B8
    float menu_window_h        = 600.0f;  // +0x0C0
    float title_bar_h          = 40.0f;   // +0x0C8 — footer rect height
    float footer_text_margin   = 8.0f;    // +0x0D0
    float footer_icon_size     = 24.0f;   // +0x0D8
    float footer_icon_sz       = 16.0f;   // +0x0E0 (secondary)

    // --- Tab metrics (0x0E8-0x120) ---
    float tab_strip_w          = 160.0f;  // +0x0E8 — left panel width
    float tab_row_h            = 40.0f;   // +0x0F0 — binary default 40 (sub_140001170); was 29, tabs overlapped
    float subtab_strip_h       = 30.0f;   // +0x0F8 — binary default 30 (sub_140001190); was 19, 17px font overflowed rows
    float groupbox_padding_0   = 8.0f;    // +0x100 — outer groupbox padding
    float tab_inner_pad        = 8.0f;    // +0x100
    float tab_label_h          = 24.0f;   // +0x108 (computed)

    // --- Page metrics (0x118-0x138) ---
    float page_padding_x       = 20.0f;   // +0x118 — user-corrected from 40 (remake was 20 px right of original)
    float page_gap             = 20.0f;   // +0x120
    float groupbox_pad         = 16.0f;   // +0x128 — inner groupbox padding
    float groupbox_title_h     = 20.0f;   // +0x130
    float text_margin          = 20.0f;   // +0x138 — inset from parent edges

    // --- Control metrics (0x140-0x1A8) ---
    float control_h            = 16.0f;   // +0x140
    float control_gap          = 20.0f;   // +0x148 — vertical gap between controls
    float label_width          = 16.0f;   // +0x150
    float groupbox_inner_pad   = 7.0f;    // +0x158
    float element_padding      = 4.0f;    // +0x160
    float button_height        = 6.0f;    // +0x168 — extra height contribution
    float checkbox_height      = 20.0f;   // +0x170 — row height (also slider)
    float slider_height        = -1.0f;   // +0x178 — sentinel: use auto
    float checkbox_pad         = 1.0f;    // +0x180
    float dropdown_height      = 2.0f;    // +0x188 — extra padding
    float keybinder_width      = 12.0f;   // +0x190
    float colorpicker_size     = 12.0f;   // +0x198

    // --- Listbox/dropdown (0x1B0-0x1E0) ---
    float listbox_item_pad     = 6.0f;    // +0x1B0
    float color_bar_height     = 24.0f;   // +0x1B8
    float popup_x_offset       = 4.0f;    // +0x1C0
    float listbox_row_h        = 8.0f;    // +0x1D0
    float dropdown_extra_h     = 24.0f;   // +0x1E0 — popup extra height

    // --- Checkbox/label (0x208-0x228) ---
    float checkbox_size        = 12.0f;   // +0x208 — box size (also swatch size)
    float label_offset         = 8.0f;    // +0x210 — gap after checkbox
    float label_spacing        = 48.0f;   // +0x218
    float sibling_h            = 20.0f;   // +0x220
    float label_h              = 24.0f;   // +0x228

    // --- Slider (0x230-0x248) ---
    float slider_text_pad_x    = 6.0f;    // +0x230
    float slider_value_pad     = 6.0f;    // +0x238
    float slider_scrollbar_pad = 8.0f;    // +0x240
    float keybinder_height     = 25.0f;   // +0x248

    // --- Colorpicker panel (0x250-0x2F8) ---
    float colorpicker_panel_w  = 44.0f;   // +0x250
    float colorpicker_panel_h  = 24.0f;   // +0x258
    float colorpicker_hue_w    = 4.0f;    // +0x260
    float colorpicker_alpha_h  = 8.0f;    // +0x268
    float colorpicker_popup_w  = 184.0f;  // +0x298 — total popup width (user 2026-06-10: was too wide at 204)
    float colorpicker_margin   = 12.0f;   // +0x2A0 — left margin
    float colorpicker_top_margin = 12.0f; // +0x2A8
    float colorpicker_slider_gap = 8.0f;  // +0x2B0 — gap between columns
    float colorpicker_slider_h = 12.0f;   // +0x2B8 — bar height offset
    float colorpicker_popup_margin = 8.0f; // +0x2E8

    // --- Column (0x270-0x290) ---
    float column_inner_gap     = 20.0f;   // +0x270
    float column_min_h         = 12.0f;   // +0x278
    float column_padding       = 16.0f;   // +0x280
    float column_width         = 328.0f;  // +0x288
    float page_content_h       = 60.0f;   // +0x290

    // --- Scrollbar (0x298-0x2B8) ---
    float scrollbar_w          = 204.0f;  // +0x298 — content width (not bar width)
    float scrollbar_pad        = 12.0f;   // +0x2A0
    float scrollbar_min_h      = 12.0f;   // +0x2A8
    float scrollbar_bar_w      = 4.0f;    // actual bar width (derived)

    // --- Tooltip (0x300-0x318) ---
    float tooltip_pad_x        = 4.0f;    // +0x300
    float tooltip_pad_y        = 2.0f;    // +0x308
    float tooltip_offset       = 8.0f;    // +0x310

    // --- Button (0x318-0x348) ---
    float button_text_offset   = 8.0f;    // +0x318
    float button_margin_y      = 4.0f;    // +0x320
    float button_h             = 24.0f;   // +0x330

    // --- Slider track (0x340-0x358) ---
    float slider_track_h       = 8.0f;    // +0x340 (user: 'a bit bigger' than the 6 px default)
    float slider_track_pad     = 20.0f;   // +0x348
    float context_menu_width   = 10.0f;   // +0x350
    float context_menu_item_h  = 10.0f;   // +0x358

    // --- Backward compatibility aliases ---
    float control_gap_y        = 20.0f;   // alias for control_gap
    float window_outline_px    = 1.0f;
    float groupbox_border_pad  = 20.0f;   // alias for text_margin
    float groupbox_rounding    = 0.0f;    // sharp corners in binary
    float groupbox_padding     = 8.0f;    // alias for groupbox_padding_0
    float page_padding_y       = 20.0f;   // alias for page_gap
    float tab_strip_h          = 40.0f;   // alias for tab_row_h
    float tab_label_padding_x  = 24.0f;   // +0x108, left text/icon reserve
    float checkbox_inner_pad   = 4.0f;    // alias for element_padding
    float small_inner_radius   = 6.0f;
    float slider_h             = 20.0f;   // alias for checkbox_height
    float sibling_w            = 12.0f;   // alias for checkbox_size
    float sibling_pad          = 8.0f;    // alias for label_offset
    float column_gap           = 20.0f;   // alias for page_gap
    float checkbox_padding     = 4.0f;    // alias for element_padding
    float control_pad_y        = 20.0f;   // alias for control_gap
    float textbox_height       = 22.0f;   // +0x1A0 — taller so 14px text isn't cramped (user 2026-06-10)
};

inline element_sizes_t& sizes() {
    static element_sizes_t s;
    return s;
}

// ============================================================================
// GUI scale factor — mirrors the binary's scalable_size_t registry
// (see [[dpi-and-resize]] memory note).
//
// Implementation: every time `set_gui_scale(x)` runs we apply a delta
// multiplier (x / prev_scale) to a SUBSET of the size_t members that
// drive on-screen layout. The base ("100%") values are snapshot the
// first time the scale changes, and every subsequent change scales from
// those snapshots so repeated set_gui_scale calls don't drift.
//
// The user-visible knob is Settings > GUI > "GUI scaling" (dropdown:
// 50% / 75% / 100% / 125% / 150% / 200%). The demo also seeds the
// initial scale from the host monitor's DPI.
// ============================================================================

inline float& gui_scale_ref() {
    static float v = 1.0f;
    return v;
}

inline float gui_scale() { return gui_scale_ref(); }

// Animation-speed multiplier. 1.0 = baseline (groupbox collapse,
// chevron spin, button hover, slider thumb grow). User-facing slider
// in Settings → GUI writes here. Range clamped to [0.1, 4.0].
inline float& anim_speed_ref() {
    static float v = 1.0f;
    return v;
}
inline float anim_speed() { return anim_speed_ref(); }
inline void  set_anim_speed(float v) {
    if (v < 0.1f) v = 0.1f;
    if (v > 4.0f) v = 4.0f;
    anim_speed_ref() = v;
}

inline element_sizes_t& base_sizes_ref() {
    // One-shot snapshot of element_sizes_t at the moment set_gui_scale is
    // first invoked. Used as the multiplier basis.
    static element_sizes_t s = sizes();
    return s;
}

inline void set_gui_scale(float new_scale)
{
    if (new_scale < 0.25f) new_scale = 0.25f;
    if (new_scale > 4.0f)  new_scale = 4.0f;

    // Capture the base once.
    static bool   captured = false;
    static element_sizes_t base;
    if (!captured) { base = sizes(); base_sizes_ref() = base; captured = true; }

    auto& s = sizes();
    const float k = new_scale;

    // Scale only the layout-critical members. Anything not listed here
    // intentionally stays at its base value (e.g. fixed-pixel paddings
    // that look wrong at non-integer scales).
    s.tab_strip_w        = base.tab_strip_w        * k;
    s.tab_row_h          = base.tab_row_h          * k;
    s.subtab_strip_h     = base.subtab_strip_h     * k;
    s.tab_label_padding_x= base.tab_label_padding_x* k;
    s.tab_strip_h        = base.tab_strip_h        * k;

    s.title_bar_h        = base.title_bar_h        * k;
    s.footer_margin      = base.footer_margin      * k;

    s.text_margin        = base.text_margin        * k;
    s.page_padding_x     = base.page_padding_x     * k;
    s.page_gap           = base.page_gap           * k;
    s.page_padding_y     = base.page_padding_y     * k;

    s.control_h          = base.control_h          * k;
    s.control_gap        = base.control_gap        * k;
    s.control_gap_y      = base.control_gap_y      * k;
    s.button_h           = base.button_h           * k;
    s.button_height      = base.button_height      * k;
    s.checkbox_height    = base.checkbox_height    * k;
    s.checkbox_size      = base.checkbox_size      * k;
    s.label_offset       = base.label_offset       * k;
    s.label_spacing      = base.label_spacing      * k;
    s.label_width        = base.label_width        * k;

    s.groupbox_padding   = base.groupbox_padding   * k;
    s.groupbox_padding_0 = base.groupbox_padding_0 * k;
    s.groupbox_pad       = base.groupbox_pad       * k;
    s.groupbox_title_h   = base.groupbox_title_h   * k;

    s.slider_track_h     = base.slider_track_h     * k;
    s.slider_track_pad   = base.slider_track_pad   * k;
    s.slider_text_pad_x  = base.slider_text_pad_x  * k;
    s.slider_value_pad   = base.slider_value_pad   * k;

    // Keybinder / colorpicker / sibling chip — user reported these
    // sizes don't track scaling.
    s.keybinder_height   = base.keybinder_height   * k;
    s.keybinder_width    = base.keybinder_width    * k;
    s.colorpicker_size   = base.colorpicker_size   * k;
    s.colorpicker_popup_w= base.colorpicker_popup_w* k;
    s.colorpicker_panel_w= base.colorpicker_panel_w* k;
    s.colorpicker_panel_h= base.colorpicker_panel_h* k;
    s.colorpicker_popup_margin = base.colorpicker_popup_margin * k;
    s.colorpicker_slider_gap   = base.colorpicker_slider_gap   * k;
    s.sibling_h          = base.sibling_h          * k;
    s.sibling_w          = base.sibling_w          * k;
    s.label_h            = base.label_h            * k;

    // Watermark / footer sizing (user explicitly called this out).
    s.footer_text_margin = base.footer_text_margin * k;
    s.footer_icon_size   = base.footer_icon_size   * k;
    s.footer_icon_sz     = base.footer_icon_sz     * k;

    // Listbox / dropdown / textbox row metrics.
    s.listbox_row_h      = base.listbox_row_h      * k;
    s.listbox_item_pad   = base.listbox_item_pad   * k;
    s.dropdown_extra_h   = base.dropdown_extra_h   * k;
    s.dropdown_height    = base.dropdown_height    * k;
    s.textbox_height     = base.textbox_height     * k;

    // Column + page extra metrics.
    s.column_gap         = base.column_gap         * k;
    s.column_padding     = base.column_padding     * k;
    s.column_width       = base.column_width       * k;

    s.menu_window_w      = base.menu_window_w      * k;
    s.menu_window_h      = base.menu_window_h      * k;

    gui_scale_ref() = new_scale;

    // User 2026-06-01: fire scale-change callbacks so the host can
    // rebake fonts at the new pixel height + resize the window. This
    // is the "reset initialization properly when changed" hook.
    for (auto& cb : scale_callbacks_ref()) if (cb) cb(new_scale);
}

inline std::vector<std::function<void(float)>>& scale_callbacks_ref() {
    static std::vector<std::function<void(float)>> v;
    return v;
}

inline void register_scale_callback(std::function<void(float)> cb) {
    scale_callbacks_ref().push_back(std::move(cb));
}

} // namespace GUI::style
