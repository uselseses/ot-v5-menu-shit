#pragma once
// =============================================================================
// GUI::c_base_window --- root for everything that owns a screen rect:
// menu window, dialog window, context menu. Provides:
//   - background + outline
//   - title text (subclasses set this via label_)
//   - layout pass that positions children in the content area
// =============================================================================

#include <GUI/elements/c_base_element.hpp>

namespace GUI {

class c_base_window : public c_base_element {
public:
    std::string_view type_name() const override { return ", Base window"; }
    bool             needs_hash() const override { return false; }

    // Returns the rect available to children (bounds minus title bar).
    virtual c_base_element::rect_t content_rect() const;

    // Position direct children one under another inside content_rect().
    // Subclasses override to do specialized layout (e.g. tab manager).
    virtual void layout_children();

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

protected:
    using c_base_element::c_base_element;
};

} // namespace GUI
