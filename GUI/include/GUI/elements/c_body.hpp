#pragma once
// GUI::c_body --- full-screen background. RTTI confirms an own
// vtable. Used as the bottom layer of the menu stack so dim/blur
// effects have something to draw against.

#include <GUI/elements/c_base_element.hpp>

namespace GUI {

class c_body : public c_base_element {
public:
    c_body() : c_base_element{""} {}

    std::string_view type_name() const override { return ", Body"; }
    bool             needs_hash() const override { return false; }

    void render(Renderer::c_renderer_core& r) override;
};

} // namespace GUI
