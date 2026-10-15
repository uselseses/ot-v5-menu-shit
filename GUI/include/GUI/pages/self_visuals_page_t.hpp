#pragma once
// =============================================================================
// GUI::self_visuals_page_t
//
// Binary ref: sub_1400857E0 @ 0x1400857E0 (allocator + ctor wrapper).
// Vtable @ 0x1401F5710, 7 vmethods.
// Mirrors enemy_visuals_page_t with label="Self", id="C".
// =============================================================================

#include <GUI/pages/abstract_visuals_page_t.hpp>

namespace GUI {

class self_visuals_page_t : public abstract_visuals_page_t {
public:
    explicit self_visuals_page_t(const Renderer::font* font);
    std::string_view type_name() const override { return ", SelfVisualsPage"; }
};

} // namespace GUI
