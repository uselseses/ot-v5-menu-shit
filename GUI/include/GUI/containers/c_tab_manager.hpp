#pragma once
// =============================================================================
// GUI::c_tab_manager — horizontal tab strip with one active child
//
// Update: 0x14006A4F0. Layout: 0x14006A4C0. Render: 0x14006A540
//
// Binary layout (verified — HOW_IT_WORKS.md §10):
//   +24..32    = children begin/end (c_tab instances)
//   +188       = label (XOR-encoded)
//   +320       = active_tab_idx (i32)
//   +324       = animation_progress (float)
//   +340..356  = tab_row_rect (16 bytes)
//   +356..372  = subtab_strip_rect (16 bytes)
//   +372..388  = content_rect (16 bytes)
//
// Render order (verified @ 0x14006A540):
//   1. vt[+288] render_filled_rect(tab_row_rect, 0xFF232328, tab_rounding)
//   2. vt[+384] render_line(p1, p2, 0xFF3E3E42, 1.0f) — sep between rows
//   3. vt[+288] render_filled_rect(subtab_strip, 0xFF202024, subtab_rounding)
//   4. For each tab:
//      text_color = is_active ? 0xFFDEDEDE : 0xFFAAAAAA
//      vt[+472] render_text(tab_label, text_color, ALIGN_CENTER)
//      active-tab accent indicator (small accent bar on left)
//   5. For each subtab:
//      text_color = is_active_subtab ? 0xFFD2D2D2 : 0xFFAAAAAA
//      vt[+472] render_text(subtab_label, text_color)
//   6. vt[+288] render_filled_rect(subtab_area, 0xFF202024, R)
//   7. vt[+384] render_line(sep, 0xFF3E3E42, 1.0f)
//   8. vt[+288] render_filled_rect(content_bg, 0xFF151519, R)
//   9. vt[+288] render_filled_rect(alt_bg, 0xFF19191D, R)
//  10. children (active c_page_manager) render
//
// Style sizes:
//   tab_strip_w = style+0xE8 (160)
//   tab_row_h = style+0xF0 (40)
//   tab_inner_pad = style+0x100 (8)
//   subtab_strip_h = style+0xF8 (30)
//
// NOTE: c_tab_manager reads bounds from g_current_layout_ctx, NOT from
// constructor argument. Parent should NOT pre-set bounds.
// =============================================================================

#include <GUI/elements/c_base_element.hpp>

namespace Renderer { class font; }

namespace GUI {

class c_tab;

class c_tab_manager : public c_base_element {
public:
    enum orientation_e { horizontal, vertical };

    explicit c_tab_manager(orientation_e o = horizontal)
        : c_base_element{""}, orientation_{o} {}

    std::string_view type_name() const override { return ", Tab manager"; }
    bool             needs_hash() const override { return false; }

    void set_tab_font(const Renderer::font* f) { tab_font_ = f; }

    orientation_e orientation() const noexcept { return orientation_; }
    void set_orientation(orientation_e o) { orientation_ = o; }

    void set_parent_window_size(float w, float h) { parent_w_ = w; parent_h_ = h; }

    std::shared_ptr<c_tab> set_active_tab(std::shared_ptr<c_tab> t);
    std::shared_ptr<c_tab> active_tab() const { return active_; }
    int active_tab_idx() const noexcept { return active_tab_idx_; }

    // Computed rects
    rect_t tab_row_rect()     const noexcept { return tab_row_rect_; }
    rect_t subtab_strip_rect() const noexcept { return subtab_strip_rect_; }
    rect_t content_rect()     const noexcept { return content_rect_; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

private:
    std::shared_ptr<c_tab> active_;
    const Renderer::font*  tab_font_    = nullptr;
    orientation_e          orientation_ = horizontal;
    float                  parent_w_    = 800.0f;
    float                  parent_h_    = 600.0f;

    // Binary layout (+320..+388)
    int   active_tab_idx_      = 0;      // +320
    float animation_progress_  = 0.0f;   // +324

    rect_t tab_row_rect_{};              // +340..+356
    rect_t subtab_strip_rect_{};         // +356..+372
    rect_t content_rect_{};              // +372..+388

    // Smooth tab animation state (relative to tab_row_rect_.y)
    float anim_tab_rel_y_    = 0.0f;
    float anim_tab_h_        = 0.0f;
    bool  anim_tab_init_     = false;
    float page_anim_alpha_   = 1.0f;
    std::vector<float> tab_active_anims_;
    std::vector<float> tab_hover_anims_;
    std::vector<float> subtab_active_anims_;
    std::vector<float> subtab_hover_anims_;
};

} // namespace GUI
