#pragma once
// GUI::c_button --- click button. Fires the on-change list when
// released over its rect. Hover/pressed visuals come from the
// colorscheme.

#include <functional>

#include <GUI/core/c_control_interface.hpp>

namespace Renderer { class font; }

namespace GUI {

class c_button : public c_control_interface {
public:
    using on_click_fn = std::function<void()>;

    c_button(std::string label, on_click_fn cb = {})
        : c_control_interface{std::move(label)}, on_click_{std::move(cb)} {}

    std::string_view type_name() const override { return ", Button"; }
    // Click buttons don't carry persistent state --- skip hash.
    bool             needs_hash() const override { return false; }

    void set_font(const Renderer::font* f) { font_ = f; }
    void set_on_click(on_click_fn cb)      { on_click_ = std::move(cb); }

    // When true, this button ignores the global modal-owner block.
    // Used by c_dialog_window for its own Dismiss/Confirm buttons:
    // the dialog claims modal globally to freeze every menu peer, but
    // its OWN buttons must still receive their clicks.
    void set_bypass_modal(bool v) { bypass_modal_ = v; }
    bool bypass_modal() const noexcept { return bypass_modal_; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

private:
    on_click_fn           on_click_;
    const Renderer::font* font_     = nullptr;
    bool                  pressed_  = false;   // mouse is held inside the rect
    float                 hover_anim_ = 0.0f;
    float                 press_anim_ = 0.0f;
    bool                  bypass_modal_ = false;
};

} // namespace GUI
