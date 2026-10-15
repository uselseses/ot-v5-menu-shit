#pragma once
// =============================================================================
// GUI::c_colorpicker — RGBA picker with HSV popup
//
// Vtable: 0x1401F6548. Render: 0x140042FD0 (swatch), 0x140041370 (popup)
//
// Sibling adornment: attaches to c_label row via element_flag 0x10.
// Renders swatch on the right; click opens HSV/RGB picker popup.
//
// Binary layout (verified — HOW_IT_WORKS.md §19):
//   +68        = popup_open_flag (byte)
//   +80..96    = hit_rect (= swatch_rect for click detection)
//   +184       = sibling_margin
//   +312       = click_handler ptr
//   +320       = popup_state
//   +332..348  = swatch_rect (16 bytes)
//   +368       = color_preview_element ptr
//   +384       = alpha_preview_element ptr
//   +400       = sv_picker_element ptr
//   +416       = hue_slider_element ptr
//   +432..480  = RGB/HSV child sliders
//   +480       = has_alpha_channel (byte)
//   +488..492  = popup position (x, y)
//   +496       = popup width
//   +500       = popup height
//   +504..520  = SV picker rect
//   +520..536  = hue bar rect
//   +536..552  = alpha bar rect
//
// Swatch positioning (from slot4 @ 0x1400448A0):
//   swatch.x = bounds.x + bounds.w - checkbox_size - 1
//   swatch.y = bounds.y + bounds.h/2 - checkbox_size/2
//   swatch.w = checkbox_size (12)
//   swatch.h = checkbox_size (12)
// =============================================================================

#include <cstdint>
#include <string>

#include <GUI/elements/c_base_control.hpp>
#include <GUI/containers/c_context_menu_owner.hpp>

#include <Renderer/color.hpp>
#include <Renderer/geometry.hpp>

namespace Renderer { class font; class c_renderer_core; }

namespace GUI {

class c_colorpicker : public c_base_control<std::uint32_t, std::uint32_t>,
                      public c_context_menu_owner {
public:
    using state_t = wm::types::thread_safe_t<std::uint32_t, std::uint32_t>;

    c_colorpicker(std::string label, Renderer::color initial,
                  bool has_alpha = true,
                  std::shared_ptr<state_t> state = {})
        : c_base_control{std::move(label),
                         state ? std::move(state)
                               : std::make_shared<state_t>(initial.packed())},
          has_alpha_{has_alpha}
    {
        set_element_flag(ef_colorpicker);
    }

    std::string_view type_name() const override { return ", Colorpicker"; }

    void set_font(const Renderer::font* f) { font_ = f; }
    bool has_alpha() const noexcept { return has_alpha_; }

    Renderer::color color() const { return Renderer::color::from_packed(get()); }
    void            set_color(Renderer::color c) { set(c.packed()); }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

    // Popup state (+320)
    bool popup_open() const noexcept { return popup_open_; }

    // Swatch rect (set during layout, +332..+348). Externally
    // writeable so c_label's adornment pass can stamp the colorpicker
    // swatch into a row's right-side slot (binary: c_label__render
    // @ 0x1400621A0 writes the slot rect directly into *(cp+396)).
    rect_t swatch_rect() const noexcept { return swatch_rect_; }
    void   set_swatch_rect(const Renderer::rect& rc);
    bool   swatch_rect_external() const noexcept { return swatch_rect_external_; }

    // Popup rect (+488..+504)
    rect_t popup_rect() const noexcept { return popup_rect_; }

    // SV picker rect (+504..+520)
    rect_t sv_rect() const noexcept { return sv_rect_; }

    // Hue bar rect (+520..+536)
    rect_t hue_rect() const noexcept { return hue_rect_; }

    // Alpha bar rect (+536..+552)
    rect_t alpha_rect() const noexcept { return alpha_rect_; }

private:
    enum class drag_target_t {
        none,
        sv,
        hue,
        alpha,
    };

    // Which boxed field (if any) is being edited inline. The hex field and
    // the four A/R/G/B value boxes are real text inputs (user 2026-06-11:
    // "the r g b aren't inputs"). Lightweight inline editing — no embedded
    // c_textbox_new, which would need container plumbing the picker lacks.
    enum class edit_field_t { none, hex };

    // Recompute every popup sub-rect from the swatch position.
    void layout_popup(const Renderer::rect& swatch);

    // Commit the in-progress inline edit (parse edit_buf_ into the colour),
    // then clear edit_field_.
    void commit_edit();

    // Binary layout mirrors
    bool has_alpha_   = true;   // +480
    bool popup_open_  = false;  // +68

    // Rects computed during layout
    rect_t swatch_rect_{};      // +332..+348 — the small swatch box
    bool   swatch_rect_external_ = false;  // set by c_label adornment pass
    rect_t popup_rect_{};       // +488..+504 — popup window
    rect_t sv_rect_{};          // +504..+520 — saturation-value picker
    rect_t hue_rect_{};         // +520..+536 — hue vertical bar
    rect_t alpha_rect_{};       // +536..+552 — alpha vertical bar
    rect_t hex_rect_{};
    rect_t copy_rect_{};
    rect_t paste_rect_{};

    // HSV state — AUTHORITATIVE while the popup is interacting.
    // Resynced from the bound RGB only when it changes externally
    // (see update()); RGB->HSV is lossy at sat/val 0 and a per-frame
    // sync used to wipe the hue mid-drag.
    float hue_ = 0.0f;
    float sat_ = 0.0f;
    float val_ = 1.0f;
    Renderer::color last_synced_color_{0, 0, 0, 0};
    drag_target_t drag_target_ = drag_target_t::none;

    // SV cursor drag animation (expands selection circle while dragging)
    float sv_drag_anim_ = 0.0f;

    // Swatch hover/press animation — same scheme as checkbox/button so
    // the swatch fade-on-hover matches the rest of the menu.
    float hover_anim_ = 0.0f;
    float press_anim_ = 0.0f;
    bool  swatch_pressed_ = false;

    // Copy / Paste popup-button animation — hover fade + a press ramp
    // that raises the border accent alpha the LONGER the button is held
    float copy_hover_anim_  = 0.0f, copy_press_anim_  = 0.0f;
    float paste_hover_anim_ = 0.0f, paste_press_anim_ = 0.0f;

    // Hover fade for the hex input field
    float field_hover_anim_ = 0.0f;

    // Inline-edit state for the hex box.
    edit_field_t edit_field_ = edit_field_t::none;
    std::string  edit_buf_;
    int          edit_caret_ = 0;
    int          edit_sel_start_ = 0;
    int          edit_sel_end_ = 0;
    bool         hex_dragging_ = false;
    float        edit_blink_ = 0.0f;

    struct undo_state_t {
        std::string text;
        int caret = 0;
        int sel_start = 0;
        int sel_end = 0;
    };
    std::vector<undo_state_t> edit_undo_stack_;
    std::vector<undo_state_t> edit_redo_stack_;

    const Renderer::font* font_ = nullptr;
};

} // namespace GUI
