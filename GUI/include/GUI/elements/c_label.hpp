#pragma once
// =============================================================================
// GUI::c_label — static text with sibling adornment support
//
// Address: 0x1400621A0. Vtable: 0x1401F7298
//
// CRITICAL ROLE: Labels are how sibling adornments attach to a row.
// A label inspects element_flags at this+160 to detect which adornments
// live to its right (or left) and shrinks its text rect accordingly.
//
// Binary layout (verified — HOW_IT_WORKS.md §14):
//   +24..32  = children begin/end
//   +160     = element_flags (adornment bits)
//   +188     = label text (XOR-encoded)
//   +320     = has_checkbox_left flag (byte)
//   +321     = has_dropdown flag (byte)
//   +322     = has_keybinder flag (byte)
//   +324     = adornment_count (i32)
//   +328..344 = label_rect (16 bytes, cached from bounds)
//   +340     = row height (= style.checkbox_height)
//
// Sibling adornment bits (only these 4 are consumed by c_label__render):
//   0x08 = c_keybinder
//   0x10 = c_colorpicker
//   0x20 = c_dropdown
//   0x40 = c_checkbox (sits LEFT of label)
// =============================================================================

#include <GUI/elements/c_base_element.hpp>

namespace Renderer { class font; }

namespace GUI {

class c_label : public c_base_element {
public:
    explicit c_label(std::string text) : c_base_element{std::move(text)} {}

    std::string_view type_name() const override { return "c_label"; }
    bool             needs_hash() const override { return false; }

    void set_font(const Renderer::font* f) { font_ = f; }

    void set_visible(bool v) override {
        target_visible_ = v;
        if (v) visible_ = true;
    }

    float visibility_fraction() const noexcept override { return anim_visibility_; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

    // Adornment state (set during update from children's flags)
    bool has_checkbox_left() const { return has_checkbox_left_; }
    bool has_dropdown() const { return has_dropdown_; }
    bool has_keybinder() const { return has_keybinder_; }
    int  adornment_count() const { return adornment_count_; }

private:
    const Renderer::font* font_ = nullptr;

    // Animated reveal/hide state
    float anim_visibility_ = 1.0f;
    bool  target_visible_  = true;

    // Binary layout at +320..+324
    bool has_checkbox_left_ = false;  // +320 — checkbox sits LEFT
    bool has_dropdown_      = false;  // +321
    bool has_keybinder_     = false;  // +322
    int  adornment_count_   = 0;      // +324 — count of right-side adornments

    // Cached text rect at +328..+344 — exposed for checkbox hit_rect (binary Path B)
    rect_t label_rect() const { return label_rect_; }
    rect_t label_rect_{};
};

} // namespace GUI
