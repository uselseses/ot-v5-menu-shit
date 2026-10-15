#pragma once
// =============================================================================
// GUI::c_page_manager — page switcher inside a c_subtab
//
// Layout: 0x1400652E0. Update: 0x140065580. Render: 0x140065710
//
// Manages the page stack inside an active subtab. A page holds one or more
// c_column instances; each column holds c_groupbox instances.
//
// Binary layout:
//   +24..32    = children begin/end (c_page instances)
//   +320       = active_page_idx (i32)
//   +324       = animation_progress (float)
//   +340..356  = content_rect (16 bytes)
//
// Style sizes:
//   page_pad_x = style+0x118 (40)
//   page_gap = style+0x120 (20)
//
// Page switching uses fade animation controlled by style.anim_speed.
// =============================================================================

#include <GUI/elements/c_base_element.hpp>

namespace GUI {

class c_page;

class c_page_manager : public c_base_element {
public:
    c_page_manager() : c_base_element{""} {}

    std::string_view type_name() const override { return ", Page manager"; }
    bool             needs_hash() const override { return false; }

    std::shared_ptr<c_page> set_active_page(std::shared_ptr<c_page> p);
    std::shared_ptr<c_page> active_page() const { return active_; }
    int active_page_idx() const noexcept { return active_page_idx_; }

    // Content rect (computed during layout)
    rect_t content_rect() const noexcept { return content_rect_; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

private:
    std::shared_ptr<c_page> active_;

    // Binary layout
    int   active_page_idx_     = 0;      // +320
    float animation_progress_  = 0.0f;   // +324
    rect_t content_rect_{};              // +340..+356
};

} // namespace GUI
