#pragma once
// GUI::c_button_grid --- N-per-row layout for c_button children. The
// demo strings include "odd_nth_occupy_full_width (click)" which
// implies a "fill remaining" mode --- Phase 5a uses uniform widths;
// the odd-row-spanning logic comes online when needed.

#include <GUI/elements/c_base_element.hpp>

namespace GUI {

class c_button_grid : public c_base_element {
public:
    explicit c_button_grid(int per_row = 2)
        : c_base_element{""}, per_row_{per_row} {}

    std::string_view type_name() const override { return ", Button grid"; }
    bool             needs_hash() const override { return false; }

    int per_row() const noexcept { return per_row_; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

private:
    int per_row_;
};

} // namespace GUI
