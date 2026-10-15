#pragma once
// =============================================================================
// GUI::c_subtab — second-level tab within a c_tab
//
// Layout: 0x140069350. Render: 0x140069880
//
// Holds a c_page_manager that switches between pages.
//
// Binary layout:
//   +24..32    = children begin/end (c_page_manager)
//   +188       = label (XOR-encoded, 64 bytes)
//   +320       = active flag (byte)
//   +340..356  = subtab_button_rect (for hover detection)
//
// Colors:
//   Active subtab text: 0xFFD2D2D2 (NOT same as active tab 0xFFDEDEDE)
//   Inactive subtab text: 0xFFAAAAAA
// =============================================================================

#include <GUI/elements/c_base_element.hpp>

namespace GUI {

class c_subtab : public c_base_element {
public:
    explicit c_subtab(std::string label) : c_base_element{std::move(label)} {}

    std::string_view type_name() const override { return ", Subtab"; }

    // Subtab button rect (for click detection)
    rect_t subtab_button_rect() const noexcept { return subtab_button_rect_; }

    float hover_anim() const noexcept { return hover_anim_; }
    void  set_hover_anim(float h) noexcept { hover_anim_ = h; }
    float active_anim() const noexcept { return active_anim_; }
    void  set_active_anim(float a) noexcept { active_anim_ = a; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

private:
    rect_t subtab_button_rect_{};  // +340..+356
    float  hover_anim_  = 0.0f;
    float  active_anim_ = 0.0f;
};

} // namespace GUI
