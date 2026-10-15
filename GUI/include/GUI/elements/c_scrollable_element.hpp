#pragma once
// =============================================================================
// GUI::c_scrollable_element — scrollable container with gradient fade
//
// Binary reference: scrollbar render @ 0x140067EC0
// Gradient fade color: 0xFFD8D8DE (verified at vt+520)
//
// Binary layout:
//   +400       = scrollbar_visible flag
//   +404..420  = scrollbar_track_rect
//   +420..436  = scrollbar_thumb_rect
//   +436       = scroll_y (float)
//   +440       = target_scroll_y (float)
//   +444       = content_height (float)
//   +448       = viewport_height (float)
//
// Scrollbar sizing (from style struct):
//   scrollbar_w = style+0x1A0 (default 8)
//   scrollbar_min_h = style+0x1A8 (default 20)
//   scrollbar_pad = style+0x1B0 (default 2)
//
// Smooth scrolling params (from Settings sliders):
//   accelerate_threshold = style+0x470
//   accelerator = style+0x478
//   distance_ratio = style+0x480
//   smoothing = style+0x488 (default 14.0)
//
// Render order:
//   1. Scrollbar track fill (if visible)
//   2. Scrollbar thumb fill (accent or 0xFF606064)
//   3. Top gradient fade (0x00D8D8DE -> 0xFFD8D8DE, 16px)
//   4. Bottom gradient fade (0xFFD8D8DE -> 0x00D8D8DE, 16px)
// =============================================================================

#include <GUI/elements/c_base_element.hpp>

namespace GUI {

class c_scrollable_element : public c_base_element {
public:
    using c_base_element::c_base_element;

    std::string_view type_name() const override { return ", Scrollable element"; }
    bool             needs_hash() const override { return false; }

    float scroll_y()          const noexcept { return scroll_y_; }
    float target_scroll_y()   const noexcept { return target_scroll_y_; }
    float content_height()    const noexcept { return content_h_; }
    float viewport_height()   const noexcept { return viewport_h_; }

    void add_scroll(float dy)         { target_scroll_y_ += dy; }
    void set_content_height(float h)  { content_h_ = h; }
    void set_viewport_height(float h) { viewport_h_ = h; }

    bool scrollbar_visible() const noexcept { return scrollbar_visible_; }
    rect_t scrollbar_track() const noexcept { return scrollbar_track_; }
    rect_t scrollbar_thumb() const noexcept { return scrollbar_thumb_; }

protected:
    void tick_scroll(float dt);
    void clamp_scroll();
    void update_scrollbar();

    // Binary layout mirrors
    bool   scrollbar_visible_ = false;  // +400
    rect_t scrollbar_track_{};          // +404..+420
    rect_t scrollbar_thumb_{};          // +420..+436

    float scroll_y_         = 0.0f;     // +436
    float target_scroll_y_  = 0.0f;     // +440
    float content_h_        = 0.0f;     // +444
    float viewport_h_       = 0.0f;     // +448

    float smoothing_ = 14.0f;           // style+0x488 default
};

} // namespace GUI
