#pragma once
// =============================================================================
// GUI::team_visuals_page_t
//
// Binary ref: sub_1400856B0 @ 0x1400856B0 (allocator + ctor wrapper).
// Vtable @ 0x1401F5730, 3 vmethods.
// Mirrors enemy_visuals_page_t with label="Team", id="B".
// =============================================================================

#include <GUI/pages/abstract_visuals_page_t.hpp>

namespace GUI {

class team_visuals_page_t : public abstract_visuals_page_t {
public:
    explicit team_visuals_page_t(const Renderer::font* font);
    std::string_view type_name() const override { return ", TeamVisualsPage"; }
};

} // namespace GUI
