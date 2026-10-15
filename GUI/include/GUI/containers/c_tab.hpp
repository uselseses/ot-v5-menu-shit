#pragma once
// =============================================================================
// GUI::c_tab — one tab in c_tab_manager
//
// Layout: 0x140069D60. Render: 0x14006A270
//
// Owns a horizontal strip of c_subtab children. The active subtab fills
// the content area below the strip.
//
// Binary layout:
//   +24..32    = children begin/end (c_subtab instances)
//   +188       = label (XOR-encoded, 64 bytes)
//   +320       = active_subtab_idx (i32)
//   +324       = animation_progress (float)
//   +340..356  = tab_button_rect (for hover detection)
//   +356..372  = accent_indicator_rect (small bar when active)
//
// Colors:
//   Active tab text: 0xFFDEDEDE
//   Inactive tab text: 0xFFAAAAAA
//   Accent indicator: runtime accent_color
// =============================================================================

#include <GUI/elements/c_base_element.hpp>

namespace Renderer { class font; }

namespace GUI {

class c_subtab;

class c_tab : public c_base_element {
public:
    explicit c_tab(std::string label) : c_base_element{std::move(label)} {}

    std::string_view type_name() const override { return ", Tab"; }

    void set_subtab_font(const Renderer::font* f) { subtab_font_ = f; }

    std::shared_ptr<c_subtab> set_active_subtab(std::shared_ptr<c_subtab> s);
    std::shared_ptr<c_subtab> active_subtab() const { return active_; }
    int active_subtab_idx() const noexcept { return active_subtab_idx_; }

    // Tab button rect (for click detection)
    rect_t tab_button_rect() const noexcept { return tab_button_rect_; }

    // Accent indicator rect (small bar when active)
    rect_t accent_rect() const noexcept { return accent_rect_; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

private:
    std::shared_ptr<c_subtab> active_;
    const Renderer::font*     subtab_font_ = nullptr;

    // Binary layout
    int   active_subtab_idx_     = 0;      // +320
    float animation_progress_    = 0.0f;   // +324

    rect_t tab_button_rect_{};             // +340..+356
    rect_t accent_rect_{};                 // +356..+372

    float subtab_anim_alpha_     = 1.0f;
};

} // namespace GUI
