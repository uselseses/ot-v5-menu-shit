#pragma once
// =============================================================================
// GUI::c_listbox — vertical list with persistent visible rows
//
// Vtable: 0x1401F6A18. Item vtable: 0x1401F6AA8
// Render: 0x14004D530 (thunk -> 0x14004F9D0)
//
// Binary layout (verified — HOW_IT_WORKS.md §21):
//   +68        = hidden flag (byte)
//   +80..96    = hit_rect
//   +188       = label (XOR-encoded)
//   +400       = hidden flag for header display
//   +496..512  = accent_strip_rect
//   +504       = strip_width (i32 — when >0, draw accent strip)
//
// Render order (verified @ 0x14004F9D0):
//   1. If hidden (+400) and has items:
//      Selected item text uses accent_color (runtime)
//      vt[+472] render_text(right-aligned, accent_color)
//   2. Calculate text width and adjust row bounds
//   3. For each visible item:
//      - Row height = style.checkbox_height
//      - If selected: draw left accent strip, accent-tinted bg
//      - Item text: 0xFFDEDEDE (selected) / 0xFFBEBEBE (normal)
//   4. Scrollbar if overflow
//   5. Gradient fade overlays at top/bottom
//
// Variants:
//   "Listbox (required, show_n_items == 4)"
//   "Listbox (searchable, show_n_items == 6)"
//   "Listbox (multi, show_n_items == 5)"
// =============================================================================

#include <string>
#include <vector>

#include <GUI/elements/c_base_control.hpp>

namespace Renderer { class font; class c_renderer_core; }

namespace GUI {

class c_listbox : public c_base_control<int, int> {
public:
    using state_t = wm::types::thread_safe_t<int, int>;

    c_listbox(std::string label,
              std::vector<std::string> options,
              int show_n_items = 5,
              bool required = true,
              std::shared_ptr<state_t> state = {})
        : c_base_control{std::move(label), std::move(state)},
          options_{std::move(options)},
          show_n_items_{show_n_items},
          required_{required}
    {
        if (required_ && state_->get() < 0 && !options_.empty()) state_->set(0);
    }

    std::string_view type_name() const override { return ", Listbox"; }

    void set_font(const Renderer::font* f) { font_ = f; }

    int  show_n_items() const noexcept { return show_n_items_; }
    bool required()    const noexcept { return required_; }

    const std::vector<std::string>& options() const noexcept { return options_; }

    // Replace the options at runtime — used by demo code to repopulate
    // configs / scripts listboxes after a directory rescan. Clamps
    // selection and resets scroll so the new contents read cleanly.
    void set_options(std::vector<std::string> opts) {
        options_ = std::move(opts);
        if (required_ && state_->get() < 0 && !options_.empty())
            state_->set(0);
        if (state_->get() >= static_cast<int>(options_.size()))
            state_->set(options_.empty() ? -1 : 0);
        scroll_y_ = target_scroll_y_ = 0.0f;
        hover_idx_ = -1;
    }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

    // Accent strip rect (+496..+512)
    rect_t accent_strip_rect() const noexcept { return accent_strip_rect_; }

private:
    std::vector<std::string> options_;
    int                      show_n_items_;
    bool                     required_;
    const Renderer::font*    font_ = nullptr;

    // Scroll state
    float scroll_y_        = 0.0f;
    float target_scroll_y_ = 0.0f;

    // Hover state
    int   hover_idx_ = -1;
    // Per-row hover fade 0..1 (reconstruction nicety matching the
    // dropdown popup rows).
    std::vector<float> row_anim_;

    // Binary layout (+496..+512)
    rect_t accent_strip_rect_{};
};

} // namespace GUI
