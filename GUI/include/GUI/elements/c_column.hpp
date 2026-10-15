#pragma once
// GUI::c_column --- multi-column layout container. Children are
// distributed left-to-right into N columns of equal width with
// `column_gap` between them.

#include <unordered_map>

#include <GUI/elements/c_base_element.hpp>

namespace GUI {

class c_column : public c_base_element {
public:
    explicit c_column(int columns = 2) : c_base_element{""}, columns_{columns} {}

    std::string_view type_name() const override { return ", Column"; }
    bool             needs_hash() const override { return false; }

    int columns() const noexcept { return columns_; }

    // Pin a child to a specific column instead of the default
    // index-alternation. Used by selection-gated groupboxes (chams
    // "Configuring model" boxes) that must stack in one column while
    // only one of them is visible at a time.
    void set_child_column(const std::shared_ptr<c_base_element>& child, int col) {
        if (child) col_override_[child.get()] = col;
    }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

private:
    int columns_ = 2;
    std::unordered_map<const c_base_element*, int> col_override_;
};

} // namespace GUI
