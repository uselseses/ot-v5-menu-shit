#pragma once
// =============================================================================
// GUI::c_page — vertically scrollable content area
//
// Holds groupboxes and columns. Scrolling via c_scrollable_element mixin.
//
// Binary layout:
//   +24..32    = children begin/end (c_column / c_groupbox)
//   +188       = label (XOR-encoded, 64 bytes)
//   +340..356  = content_rect (16 bytes)
//   +400       = scroll_y (float)
//   +404       = target_scroll_y (float)
//   +408       = content_height (float)
//
// Style sizes:
//   page_pad_x = style+0x118 (40)
//   page_gap = style+0x120 (20)
// =============================================================================

#include <GUI/elements/c_base_element.hpp>

namespace GUI {

class c_page : public c_base_element {
public:
    explicit c_page(std::string label) : c_base_element{std::move(label)} {}

    std::string_view type_name() const override { return ", Page"; }
    bool             needs_hash() const override { return false; }

    // Content rect (computed during layout)
    rect_t content_rect() const noexcept { return content_rect_; }

    // Pre-mutation outer bounds — c_page temporarily overwrites bounds_
    // with `scrolled` inside update() so c_groupbox::update reads inset
    // dimensions. Anything that needs the REAL outer page rect (e.g.
    // c_icon_tab_strip wanting to render flush to the menu border) can
    // call this instead and bypass the inset.
    rect_t outer_bounds() const noexcept { return outer_bounds_; }

    // Scroll state
    float scroll_y()        const noexcept { return scroll_y_; }
    float content_height()  const noexcept { return content_height_; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

private:
    rect_t outer_bounds_{};              // snapshot of bounds_ before update mutates it
    rect_t content_rect_{};              // +340..+356
    float  scroll_y_        = 0.0f;      // +400
    float  target_scroll_y_ = 0.0f;      // +404
    float  content_height_  = 0.0f;      // +408

    // Scrollbar interaction state (reconstruction-only — not in binary)
    rect_t scrollbar_thumb_{};
    bool   thumb_hovered_   = false;
    bool   thumb_pressed_   = false;
    float  thumb_drag_off_  = 0.0f;
    float  thumb_grow_      = 0.0f;      // 0..1 animated grow factor
};

} // namespace GUI
