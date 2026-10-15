#pragma once
// =============================================================================
// GUI::c_dropdown — single-select dropdown
//
// Vtable: 0x1401F6628. Render: 0x140046520
//
// Sibling adornment: attaches to c_label row via element_flag 0x20.
//
// Binary layout (verified — HOW_IT_WORKS.md §18):
//   +68        = inline/sibling layout mode for this element, NOT popup-open
//   +80..96    = hit_rect (dropdown box for click detection)
//   +188       = label (XOR-encoded, 64 bytes)
//   +320       = embedded select/list state; popup-open is queried through
//                c_dropdown__update_0(this + 320)
//   +320       = selected_idx inside the embedded state in simple source model
//   +324       = animation_progress (float)
//   +340..356  = dropdown_box_rect
//   +356..372  = popup_rect (when open)
//   +372       = hover_idx (i32)
//
// Render order (verified @ 0x140046520):
//   1. Label text (if not hidden):
//      height = style.checkbox_height (NOT dropdown_height)
//      text_y = bounds.y + checkbox_height / 2
//      vt[+472] render_text(left-aligned, 0xFFBEBEBE)
//   2. Dropdown box body — SAME color as window body:
//      rounded_flag = style.dropdown_rounding ? 6 : 0
//      vt[+288] render_filled_rect(this+344, 0xFF232328, rounded_flag)
//   3. Debug outline (if g_state_textbox_focus_ptr):
//      vt[+264] push_clip(this+344, 1)
//   4. Build selected-items display string
//   5. Text width check: truncate if exceeds (this+352 - qword_1402461F8)
//   6. Chevron arrow on the right:
//      vt[+304] render_textured_rect(arrow_rect, g_texture_chevron, ...)
//   7. If popup open (overlay queue):
//      panel_h = item_count * row_h + style.dropdown_extra_h (24)
//      Each item row: hover → accent_color fill, else 0xFF232328
//      Item text: 0xFFBEBEBE, ALIGN_LEFT
//
// Note: Right-click clears the value (c_dropdown__on_click)
// =============================================================================

#include <string>
#include <vector>

#include <GUI/elements/c_base_control.hpp>

namespace Renderer { class font; class c_renderer_core; }

namespace GUI {

class c_dropdown : public c_base_control<int, int> {
public:
    using state_t = wm::types::thread_safe_t<int, int>;

    c_dropdown(std::string label, std::vector<std::string> options,
               std::shared_ptr<state_t> state = {})
        : c_base_control{std::move(label), std::move(state)},
          options_{std::move(options)}
    {
        set_element_flag(ef_dropdown);
    }

    std::string_view type_name() const override { return ", Dropdown"; }

    void set_font(const Renderer::font* f) { font_ = f; }

    // Multi-select mode (per user 2026-05-31: hitboxes etc. should be
    // multi). The bitset shadows the single-select `get()` value when
    // multi mode is on; render shows accent text for every set bit and
    // the box header shows the count.
    void set_multi_select(bool m) { multi_select_ = m; }
    bool multi_select() const noexcept { return multi_select_; }
    bool is_selected_multi(int i) const noexcept {
        if (i < 0 || i >= 64) return false;
        return (multi_mask_ >> i) & 1ULL;
    }
    void toggle_multi(int i) {
        if (i < 0 || i >= 64) return;
        multi_mask_ ^= (1ULL << i);
    }

    const std::vector<std::string>& options() const noexcept { return options_; }
    bool open() const noexcept { return open_; }

    // Rects computed during layout
    rect_t dropdown_box_rect() const noexcept { return dropdown_box_rect_; }
    rect_t popup_rect()        const noexcept { return popup_rect_; }
    int    hover_idx()         const noexcept { return hover_idx_; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

private:
    std::vector<std::string> options_;
    const Renderer::font*    font_ = nullptr;

    // Binary layout
    // Source-level popup state. In the binary the open/list state is embedded
    // at +320 and queried with c_dropdown__update_0; +68 is inline layout.
    bool  open_        = false;
    int   hover_idx_   = -1;     // +372
    float anim_prog_   = 0.0f;   // +324
    // Chevron spin (matches groupbox minimize): increments by 180° on
    // each open/close click; chevron_rotation_ tweens toward target.
    float chevron_rotation_ = 0.0f;
    float target_rotation_  = 0.0f;

    // Hover + open animation progress — mirrors binary
    // `anim_calc_progress_4ch` channels feeding `anim_blend_colors_multi`
    // in `c_dropdown__render.c:484-495`. The border colour is a 3-stop
    // lerp: idle → hover_accent_translucent → accent_solid.
    float hover_anim_ = 0.0f;   // 0 idle ↔ 1 hovered
    // open_lin_ = raw linear 0..1 progress (time-based, ~200 ms, matches
    // c_groupbox). open_anim_ = open_lin_ run through ease_in_out_cubic —
    // the eased value the reveal height + border blend actually use.
    float open_lin_   = 0.0f;
    float open_anim_  = 0.0f;   // 0 closed ↔ 1 open (eased)

    rect_t dropdown_box_rect_{}; // +340..+356
    rect_t popup_rect_{};        // +356..+372

    // Multi-select state — option indices set as bits.
    bool          multi_select_ = false;
    std::uint64_t multi_mask_   = 0;

    // Per-row hover anim 0..1 — smooth fade of the row-hover bg when
    // the cursor sweeps over popup items. Reset when the popup closes.
    std::vector<float> row_anim_{};

    // Popup list scrolling — at most k_max_visible_rows rows show at
    // once; longer lists wheel-scroll (user 2026-07-17).
    static constexpr int k_max_visible_rows = 5;
    float dd_scroll_y_        = 0.0f;
    float dd_target_scroll_y_ = 0.0f;
};

} // namespace GUI
