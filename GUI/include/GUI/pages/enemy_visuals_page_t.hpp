#pragma once
// =============================================================================
// GUI::enemy_visuals_page_t
//
// Binary ref: sub_140085580 @ 0x140085580 (allocator + ctor wrapper).
// Vtable @ 0x1401F5720, 5 virtual methods.
//
// Constructor sequence (decompiled):
//   operator new(0x2D8u);                       // 728-byte object
//   *this = enemy_visuals_page_t::vftable;      // set vtable
//   abstract_visuals_page_t::ctor(this+2, parent, "Enemy", "A", mode=5, data)
//   // appends 4 colorpickers via sub_1400337B0:
//   //   ("Visible",     0xFFFAA618)  -- accent orange
//   //   ("Attachments", 0xFF1B1A26)  -- packed -15140186
//   //   ("Hidden",      0xFFFA3618)  -- packed -379624
//   //   ("History",     <truncated>)
// =============================================================================

#include <GUI/pages/abstract_visuals_page_t.hpp>

namespace GUI {

class enemy_visuals_page_t : public abstract_visuals_page_t {
public:
    explicit enemy_visuals_page_t(const Renderer::font* font);
    std::string_view type_name() const override { return ", EnemyVisualsPage"; }
};

} // namespace GUI
