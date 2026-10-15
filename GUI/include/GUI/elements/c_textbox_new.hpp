#pragma once
// =============================================================================
// GUI::c_textbox_new — single-line text input with XOR-encrypted storage
//
// Vtable: 0x1401F6D40. Render: 0x14005A820 (thunk -> slot+40)
//
// Binary layout (verified — HOW_IT_WORKS.md §22):
//   +32..+288  = value (XOR-encrypted, 256 bytes max)
//   +289..+292 = XOR key (3 bytes, at +32+257+(i%3))
//   +68        = focused flag
//   +80..96    = hit_rect
//   +188       = label (XOR-encoded, 64 bytes)
//
// XOR scheme for textbox values (distinct from 64-byte label scheme):
//   - Encrypted buffer at this+32, 256 bytes max
//   - Key bytes: this[32 + 257 + (i % 3)] (3-byte cycling key)
//   - Decoded into heap std::string each frame
//
// Render order (verified @ 0x14005A820):
//   1. vt[+288] render_filled_rect(bounds, 0xFF232328, textbox_rounding)
//   2. vt[+296] render_rect_outline(bounds) — 0xFF3E3E42 unfocused, accent focused
//   3. vt[+264] push_clip(text_bounds)
//   4. if (has_selection):
//      vt[+288] render_filled_rect(sel_rect, alpha_blend(accent, 0x60), 0)
//   5. vt[+472] render_text(text_pos, decoded_value,
//                           0xFFBEBEBE, ALIGN_LEFT, ALIGN_CENTER)
//   6. if (focused && cursor_visible):
//      cursor_alpha = blink_fade(GetTickCount64())
//      vt[+288] render_filled_rect(cursor_rect, alpha_blend(accent, cursor_alpha))
// =============================================================================

#include <string>

#include <GUI/elements/c_base_element.hpp>

namespace Renderer { class font; class c_renderer_core; }

namespace GUI {

class c_textbox_new : public c_base_element {
public:
    explicit c_textbox_new(std::string label, std::string placeholder = {})
        : c_base_element{std::move(label)}, placeholder_{std::move(placeholder)} {}

    std::string_view type_name() const override { return ", Textbox"; }
    bool             needs_hash() const override { return true; }

    void set_font(const Renderer::font* f) { font_ = f; }

    const std::string& value() const noexcept { return value_; }
    void               set_value(std::string v) { value_ = std::move(v); clamp_caret(); }

    bool focused() const noexcept { return focused_; }

    // Vertical gap to the NEXT flow element. Unset = the style's 6 px
    // control gap. Negative values overlap — the cloud Search box uses
    // -1 so its bottom border and the list's top border coincide as ONE
    // line (og2-cloud capture; two stacked borders read doubled).
    void set_flow_gap(float g) { flow_gap_ = g; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

    // Text rect (inner bounds for text rendering)
    rect_t text_rect() const noexcept { return text_rect_; }

    // Selection state
    int selection_start() const noexcept { return sel_start_; }
    int selection_end()   const noexcept { return sel_end_; }
    bool has_selection()  const noexcept { return sel_start_ != sel_end_; }

private:
    void clamp_caret() {
        if (caret_ > static_cast<int>(value_.size())) caret_ = static_cast<int>(value_.size());
        if (caret_ < 0) caret_ = 0;
    }

    std::string           value_;
    std::string           placeholder_;
    const Renderer::font* font_ = nullptr;

    // Binary layout mirrors
    bool  focused_    = false;   // +68
    bool  pressed_    = false;
    int   caret_      = 0;
    int   sel_start_  = 0;
    int   sel_end_    = 0;
    float blink_      = 0.0f;    // 0..2*pi cycle for cursor blink
    float scroll_x_   = 0.0f;    // horizontal scroll for long text
    float hover_anim_ = 0.0f;
    float press_anim_ = 0.0f;
    float flow_gap_   = -1.0e9f; // sentinel = style default (see set_flow_gap)

    // Computed during layout
    rect_t text_rect_{};
    rect_t box_rect_{};

    // Undo / Redo history
    struct undo_state_t {
        std::string text;
        int caret = 0;
        int sel_start = 0;
        int sel_end = 0;
    };
    std::vector<undo_state_t> undo_stack_;
    std::vector<undo_state_t> redo_stack_;
};

} // namespace GUI
