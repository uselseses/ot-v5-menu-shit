#pragma once
// =============================================================================
// GUI::c_separator — horizontal divider (two-line bevel)
//
// Address: 0x14005FF30. Vtable: 0x1401F7248
//
// IMPORTANT: Binary draws TWO stacked 1-pixel lines (highlight + shadow),
// NOT one line. This produces a subtle bevel effect.
//
// Render order (verified — HOW_IT_WORKS.md §25):
//   rect = this->bounds_xmm (16-byte rect)
//   rect.h = 1
//   rect.y = bounds.y + bounds.h / 2  (centered vertically)
//
//   // Line 1: the highlight (top), color 0xFF353538
//   vt[+248] render_filled_rect_1px({rect.x, rect.y, rect.w, 1}, 0xFF353538)
//
//   rect.y -= 1  // move up 1px
//
//   // Line 2: the shadow (bottom, drawn 1px above), color 0xFF3E3E42
//   vt[+248] render_filled_rect_1px({rect.x, rect.y, rect.w, 1}, 0xFF3E3E42)
//
// Colors:
//   0xFF353538 (4281677112) — slightly darker, top line
//   0xFF3E3E42 (4282269250) — standard border grey, bottom line
// =============================================================================

#include <GUI/elements/c_base_element.hpp>

namespace GUI {

class c_separator : public c_base_element {
public:
    c_separator() : c_base_element{""} {}

    std::string_view type_name() const override { return ", Separator"; }
    bool             needs_hash() const override { return false; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;
};

} // namespace GUI
