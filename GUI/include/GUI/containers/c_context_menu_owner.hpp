#pragma once
// =============================================================================
// GUI::c_context_menu_owner --- mixin for any widget that can spawn a
// right-click context menu. RTTI evidence:
//   .?AVc_context_menu_owner@GUI@@
// and the create_context_menu lambdas in c_dropdown / c_colorpicker /
// c_keybinder.
//
// Phase 4 just declares the slot; Phase 5 wires the spawn behavior
// (right-click in WndProc -> open_context_menu()).
// =============================================================================

#include <memory>

namespace GUI {

class c_context_menu;

class c_context_menu_owner {
public:
    virtual ~c_context_menu_owner() = default;

    bool has_context_menu_open() const noexcept { return !!menu_; }

    const std::shared_ptr<c_context_menu>& context_menu() const { return menu_; }

protected:
    // Subclass calls these from its create_context_menu() override.
    void install_context_menu(std::shared_ptr<c_context_menu> m) { menu_ = std::move(m); }
    void dismiss_context_menu() { menu_.reset(); }

    std::shared_ptr<c_context_menu> menu_;
};

} // namespace GUI
