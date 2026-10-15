#pragma once
// =============================================================================
// GUI::c_checkbox --- bool-state checkbox.
//
// BINARY LAYOUT (from IDA @ 0x14003fbc0 c_checkbox__update):
// - Reads parent->next_y (+60) for Y position, NOT parent->bounds.y
// - Sets own bounds at +96
// - If not is_in_row, applies text_margin inset
// - Computes box position at +340, +344
// =============================================================================

#include <GUI/elements/c_base_control.hpp>

namespace Renderer { class font; }

namespace GUI {

class c_checkbox : public c_base_control<bool, bool> {
public:
    using state_t = wm::types::thread_safe_t<bool, bool>;

    c_checkbox(std::string label, std::shared_ptr<state_t> state = {})
        : c_base_control{std::move(label), std::move(state)}
    {
        set_element_flag(ef_checkbox);
    }

    std::string_view type_name() const override { return ", Checkbox"; }

    void set_font(const Renderer::font* f) { font_ = f; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

    float box_x() const { return box_x_; }
    float box_y() const { return box_y_; }

    // Force the hover animation on even when the cursor isn't over the box
    // itself — used by c_esp_row so hovering the label TEXT lights the
    // checkbox like the original (user 2026-06-11). Set every frame.
    void set_force_hover(bool h) { force_hover_ = h; }

private:
    const Renderer::font* font_ = nullptr;
    float box_x_ = 0;
    float box_y_ = 0;
    // Hover anim 0..1, ticks toward 1.0 when hovered, 0 otherwise.
    // Mirrors the binary's anim_blend_colors_multi progress at +120/+128/+136.
    float hover_anim_ = 0.0f;
    // Checked anim 0..1 — crossfades the accent fill in/out so
    // unchecking shows the accent fading from the border instead of
    // snapping to idle (user 2026-07-17).
    float check_anim_ = 0.0f;
    bool  check_anim_init_ = false;
    bool  force_hover_ = false;
};

} // namespace GUI
