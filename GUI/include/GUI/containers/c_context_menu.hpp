#pragma once
// =============================================================================
// GUI::c_context_menu — right-click popup menu
//
// Vtable: 0x1401F6FE0
// Update: 0x140053080. Layout: 0x140053700. Render: 0x140053A00
//
// Enrolled on overlay queue (always above siblings).
//
// Binary layout:
//   +68        = visible flag (byte)
//   +80..96    = menu_rect
//   +320       = item_count (i32)
//   +324..N    = items array
//
// Render order (verified — HOW_IT_WORKS.md §23):
//   1. Shadow (sub_1400910A0 prep)
//   2. vt[+288] render_filled_rect(menu_rect, 0xFF232328, context_menu_rounding)
//   3. vt[+296] render_rect_outline(menu_rect) — 0xFF3E3E42
//   4. For each item:
//      if (is_separator):
//          vt[+384] render_line(sep_p1, sep_p2, 0xFF3E3E42, 1.0f)
//      else:
//          if (hovered):
//              vt[+288] render_filled_rect(item_rect, accent_color, 0)
//              text_color = 0xFFDEDEDE
//          else:
//              text_color = 0xFFBEBEBE
//          vt[+472] render_text(item_pos, item_text, text_color, ALIGN_LEFT)
//
// Used by: c_dropdown, c_colorpicker, c_keybinder
// Sizes: context_menu_width (+0x350), context_menu_item_h (+0x358)
// =============================================================================

#include <GUI/containers/c_base_window.hpp>
#include <string>
#include <vector>
#include <functional>

namespace GUI {

struct context_menu_item {
    std::string label;
    std::function<void()> action;
    bool is_separator = false;
};

class c_context_menu : public c_base_window {
public:
    c_context_menu() : c_base_window{""} {}

    std::string_view type_name() const override { return ", Context menu"; }

    void add_item(std::string label, std::function<void()> action);
    void add_separator();
    void clear_items();

    const std::vector<context_menu_item>& items() const noexcept { return items_; }

    // Menu rect (computed during layout)
    rect_t menu_rect() const noexcept { return menu_rect_; }
    int    hover_idx() const noexcept { return hover_idx_; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

private:
    std::vector<context_menu_item> items_;
    rect_t menu_rect_{};
    int    hover_idx_ = -1;
};

} // namespace GUI
