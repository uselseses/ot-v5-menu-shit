#pragma once
// =============================================================================
// GUI::style::colorscheme_t
//
// EXACT colors from GUI_64_DX11.exe — verified 2026-05-31 via byte audit of
// every widget render function. See HOW_IT_WORKS.md §6 for the full audit.
//
// Binary format: 0xAARRGGBB -> Renderer::color{R, G, B, A}
// =============================================================================

#include <Renderer/color.hpp>

namespace GUI::style {

struct colorscheme_t {
    // -------------------------------------------------------------------------
    // ACCENT — 0xFFFAA618 (A=255, R=250, G=166, B=24) — saturated ORANGE
    // Verified @ sub_140001C30 bytes: C7 06 18 A6 FA FF = mov [rsi], 0xFFFAA618
    // Prior docs claiming 0xFFFAA718 or "cyan-blue" were WRONG.
    // -------------------------------------------------------------------------
    // Binary-verified accent is orange 0xFFFAA618; the user prefers the
    // pink theme for now (2026-06-11: "i liked the pink color, add the
    // pink back for now").
    //Renderer::color accent{ 0xFA, 0xA6, 0x18, 0xFF };  // 0xFFFAA618
    Renderer::color accent{ 0xFF, 0x82, 0xD4, 0xFF };    // pink (user preference)

    // -------------------------------------------------------------------------
    // Window fills — from c_menu_window__render @ 0x140064460
    // -------------------------------------------------------------------------
    // 0xFF232328 @ 0x140064543 — main window body fill
    Renderer::color body_bg          { 0x23, 0x23, 0x28, 0xFF };  // 0xFF232328
    // 0xFF19191D @ 0x1400645E8 — tab strip (left panel), scrollbar track
    Renderer::color tab_strip_bg     { 0x19, 0x19, 0x1D, 0xFF };  // 0xFF19191D
    // 0xFF151519 @ 0x140064672 — content area inner fill, footer
    Renderer::color title_bar_bg     { 0x15, 0x15, 0x19, 0xFF };  // 0xFF151519

    // -------------------------------------------------------------------------
    // Separators & borders — from c_separator__render @ 0x14005FF30
    // Binary draws: dark (0xFF353538) at y, highlight (0xFF3E3E42) at y-1
    // -------------------------------------------------------------------------
    Renderer::color separator_dark   { 0x35, 0x35, 0x38, 0xFF };  // 0xFF353538 (top line)
    Renderer::color separator_light  { 0x3E, 0x3E, 0x42, 0xFF };  // 0xFF3E3E42 (highlight below)
    Renderer::color separator        { 0x35, 0x35, 0x38, 0xFF };  // alias for separator_dark

    // -------------------------------------------------------------------------
    // Text colors — verified across multiple widget renders
    // -------------------------------------------------------------------------
    Renderer::color title_text       { 0xDE, 0xDE, 0xDE, 0xFF };  // 0xFFDEDEDE — titles, active tab
    Renderer::color text             { 0xBE, 0xBE, 0xBE, 0xFF };  // 0xFFBEBEBE — body text, labels
    Renderer::color text_secondary   { 0xAA, 0xAA, 0xAA, 0xFF };  // 0xFFAAAAAA — inactive tab/subtab
    Renderer::color text_dim         { 0xAA, 0xAA, 0xAA, 0xFF };  // 0xFFAAAAAA — slider value
    Renderer::color text_disabled    { 0xBE, 0xBE, 0xBE, 0x80 };  // 0x80BEBEBE — disabled label (50% alpha)

    // -------------------------------------------------------------------------
    // Groupbox — from c_groupbox__render @ 0x140060000
    // -------------------------------------------------------------------------
    Renderer::color groupbox_bg      { 0x23, 0x23, 0x28, 0xFF };  // 0xFF232328 — body
    Renderer::color groupbox_border  { 0x3E, 0x3E, 0x42, 0xFF };  // 0xFF3E3E42 — border
    Renderer::color groupbox_title   { 0xDE, 0xDE, 0xDE, 0xFF };  // 0xFFDEDEDE — title text
    Renderer::color groupbox_header  { 0x20, 0x20, 0x24, 0xFF };  // 0xFF202024 — header bar

    // -------------------------------------------------------------------------
    // Tabs — from c_tab_manager__render @ 0x14006A540
    // -------------------------------------------------------------------------
    Renderer::color tab_inactive     { 0xAA, 0xAA, 0xAA, 0xFF };  // 0xFFAAAAAA
    Renderer::color tab_active       { 0xDE, 0xDE, 0xDE, 0xFF };  // 0xFFDEDEDE
    Renderer::color subtab_inactive  { 0xAA, 0xAA, 0xAA, 0xFF };  // 0xFFAAAAAA
    Renderer::color subtab_active    { 0xD2, 0xD2, 0xD2, 0xFF };  // 0xFFD2D2D2
    Renderer::color tab_inset        { 0x15, 0x15, 0x19, 0xFF };  // 0xFF151519 — content bg
    Renderer::color page_bg          { 0x15, 0x15, 0x19, 0xFF };  // 0xFF151519

    // -------------------------------------------------------------------------
    // Scrollbar — from c_scrollable_element render
    // -------------------------------------------------------------------------
    Renderer::color scrollbar_bg     { 0x19, 0x19, 0x1D, 0xFF };  // 0xFF19191D — track
    Renderer::color scrollbar_thumb  { 0x3E, 0x3E, 0x42, 0xFF };  // 0xFF3E3E42 — thumb
    Renderer::color scrollbar_fade   { 0xD8, 0xD8, 0xDE, 0xFF };  // 0xFFD8D8DE — gradient fade

    // -------------------------------------------------------------------------
    // Checkbox — from c_checkbox__render_slot3 @ 0x140040000
    // Border colors via anim_blend_colors_multi:
    //   unchecked=0xFF3E3E42, hover=0xFF606064, checked=accent
    // Checkmark drawn via vt[+384] render_line with color 0xFF202024
    // -------------------------------------------------------------------------
    Renderer::color checkbox_bg      { 0x3E, 0x3E, 0x42, 0xFF };  // 0xFF3E3E42 — unchecked border
    Renderer::color checkbox_hover   { 0x60, 0x60, 0x64, 0xFF };  // 0xFF606064 — hover border
    Renderer::color checkbox_check   { 0x20, 0x20, 0x24, 0xFF };  // 0xFF202024 — checkmark stroke

    // -------------------------------------------------------------------------
    // Button — from c_button__render @ 0x14003E5B0
    // CORRECTED: fill is 0xFF232328, NOT 0xFF333338
    // -------------------------------------------------------------------------
    Renderer::color button_bg        { 0x23, 0x23, 0x28, 0xFF };  // 0xFF232328 — same as body
    Renderer::color button_hover     { 0x3E, 0x3E, 0x42, 0xFF };  // 0xFF3E3E42 — hover
    Renderer::color button_text      { 0xBE, 0xBE, 0xBE, 0xFF };  // 0xFFBEBEBE — text

    // -------------------------------------------------------------------------
    // Slider — from c_slider__render_0 @ 0x14004BB80
    // Track bg is 0xFF19191D (same as tab_strip_bg), NOT 0xFF1F1F1D
    // -------------------------------------------------------------------------
    Renderer::color slider_track     { 0x19, 0x19, 0x1D, 0xFF };  // 0xFF19191D — track bg

    // -------------------------------------------------------------------------
    // Menu window composite colors
    // -------------------------------------------------------------------------
    Renderer::color menu_fill        { 0x23, 0x23, 0x28, 0xFF };  // 0xFF232328 — main fill
    Renderer::color menu_inner       { 0x19, 0x19, 0x1D, 0xFF };  // 0xFF19191D — tab strip
    Renderer::color menu_outer       { 0x15, 0x15, 0x19, 0xFF };  // 0xFF151519 — content/footer
    Renderer::color border_highlight { 0x3E, 0x3E, 0x42, 0xFF };  // 0xFF3E3E42 — border lines

    // -------------------------------------------------------------------------
    // Misc
    // -------------------------------------------------------------------------
    Renderer::color window_bg        { 0x23, 0x23, 0x28, 0xFF };  // 0xFF232328
    Renderer::color window_highlight { 0x3E, 0x3E, 0x42, 0xFF };  // 0xFF3E3E42
    Renderer::color window_border    { 0x3E, 0x3E, 0x42, 0xFF };  // 0xFF3E3E42 — window border
    Renderer::color title_bg         { 0x15, 0x15, 0x19, 0xFF };  // 0xFF151519 — title bar
    Renderer::color outline          { 0x3E, 0x3E, 0x42, 0xFF };  // 0xFF3E3E42 — universal border
    Renderer::color hover_overlay    { 0xFF, 0xFF, 0xFF, 0x0E };  // 0x0EFFFFFF — subtle hover tint
    Renderer::color gradient_overlay { 0xFF, 0xFF, 0xFF, 0x40 };  // 0x40FFFFFF — title gradient
};

inline colorscheme_t& current() {
    static colorscheme_t cs;
    return cs;
}

} // namespace GUI::style
