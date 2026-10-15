#pragma once
// =============================================================================
// GUI::c_menu_window — main cheat menu shell
//
// Update: 0x140063E30. Render: 0x140064460
//
// Binary layout (verified — HOW_IT_WORKS.md §9):
//   +96..112   = bounds (x, y, w, h as i32)
//   +504       = modal_flag (qword — skip close button if set)
//   +536       = dialog_stack.begin (ptr)
//   +544       = dialog_stack.end (ptr)
//   +624..640  = right content rect (x + tab_strip_w, y, w - tab_strip_w, h)
//   +640       = has_focus / dragging flag (byte)
//   +696       = resizing / prevent_close flag (byte)
//   +724..740  = footer rect (left strip bottom band, x, y+h-title_bar_h, tab_strip_w, title_bar_h)
//
// Render order (verified @ 0x140064460):
//   1. sub_1400910A0(bounds, use_rounding) — shadow / animated prep
//   2. vt[+288] render_filled_rect(bounds, 0xFF232328, R) — body
//   3. vt[+288] render_filled_rect({x,y,tab_strip_w,h}, 0xFF19191D, R) — tab strip
//   4. vt[+288] render_filled_rect(title_rect[+624], 0xFF151519, 2*R) — content area
//   5. vt[+304] render_textured_rect({x,y}, g_texture_gradient, 0x40FFFFFF, scale/2)
//   6. if (!modal && !prevent_close && dialog_stack_empty):
//      vt[+304] render_textured_rect(close_btn, g_texture_close, tint)
//   7. vt[+248] render_filled_rect_1px({x,y+title_top,tab_strip_w,1}, 0xFF3E3E42) — h sep
//   8. vt[+248] render_filled_rect_1px({x+tab_strip_w,y,1,h}, 0xFF3E3E42) — v sep
//   9. vt[+288] render_filled_rect(content_rect[+624], 0xFF151519, 16*R) — inner content
//  10. vt[+296] render_rect_outline({content.x,content.y+1,content.w,content.h-1})
//  11. vt[+248] render_filled_rect_1px({content.x,content.y,content.w,1}, 0xFF3E3E42)
//  12. vt[+312] render_textured_rect_scaled(footer_icon_pos, g_texture_user_icon)
//  13. vt[+488] render_text_with_shadow(footer, "William", 0xFFDEDEDE)
//  14. vt[+488] render_text_with_shadow(footer, "01/01/3000", accent_color)
//
// Hardcoded strings:
//   "William" @ 0x140064C50 — username in footer
//   "01/01/3000" @ 0x140064CB5 — expiry date
// =============================================================================

#include <GUI/containers/c_base_window.hpp>
#include <Renderer/draw_list.hpp>
#include <Renderer/geometry.hpp>

namespace Renderer { class font; }

namespace GUI {

class c_menu_window : public c_base_window {
public:
    c_menu_window(std::string title, const Renderer::font* title_font = nullptr)
        : c_base_window{std::move(title)}, title_font_{title_font} {}

    std::string_view type_name() const override { return "c_menu_window"; }

    void set_title_font(const Renderer::font* f) { title_font_ = f; }
    void set_logo_texture(Renderer::texture_handle h) { logo_texture_ = h; }
    // Optional grayscale variant shown while NOT hovered (the colour
    // texture fades in on hover) — user 2026-06-10: idle logo is B&W.
    void set_logo_gray_texture(Renderer::texture_handle h) { logo_gray_texture_ = h; }
    void set_profile_texture(Renderer::texture_handle h) { profile_texture_ = h; }
    void set_bounds(rect_t r) {
        bounds_ = r;
        pos_x_ = static_cast<int>(r.x);
        pos_y_ = static_cast<int>(r.y);
        width_ = static_cast<int>(r.w);
        height_ = static_cast<int>(r.h);
    }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

    bool is_logo_hovered() const { return logo_hovered_; }
    bool is_dragging() const { return dragging_; }

    // Binary layout accessors
    rect_t content_rect() const { return rect_t{static_cast<float>(content_rect_.x), static_cast<float>(content_rect_.y), static_cast<float>(content_rect_.w), static_cast<float>(content_rect_.h)}; }
    rect_t footer_rect() const { return rect_t{static_cast<float>(footer_rect_.x), static_cast<float>(footer_rect_.y), static_cast<float>(footer_rect_.w), static_cast<float>(footer_rect_.h)}; }
    bool   has_focus() const { return has_focus_; }
    bool   modal() const { return modal_ != 0; }

    // Minimize/expand state — driven by the top-right minimize button.
    bool   is_minimized() const { return minimized_; }
    void   set_minimized(bool v) { minimized_ = v; }

    // Open/close animation state
    bool   is_open() const noexcept { return open_; }
    void   set_open(bool o) noexcept { open_ = o; }
    float  open_anim_progress() const noexcept { return open_anim_; }

private:
    const Renderer::font* title_font_ = nullptr;
    Renderer::texture_handle logo_texture_{};
    Renderer::texture_handle logo_gray_texture_{};
    Renderer::texture_handle profile_texture_{};
    // Logo hover fade 0..1
    float logo_hover_anim_ = 0.0f;

    // Window open/close animation
    bool  open_      = true;
    float open_anim_ = 1.0f;

    // Integer rect for binary compatibility
    struct rect_i { int x, y, w, h; };

    // Binary layout at +96..+112
    int pos_x_  = 100;
    int pos_y_  = 100;
    int width_  = 800;
    int height_ = 600;

    // Min size
    int min_width_  = 500;
    int min_height_ = 400;

    // Modal flag (+504)
    std::uint64_t modal_ = 0;

    // Content area (+624..+640)
    rect_i content_rect_{};

    // Has focus / dragging (+640)
    bool has_focus_ = false;
    bool dragging_  = false;
    int drag_start_x_ = 0;
    int drag_start_y_ = 0;

    // Resizing / prevent_close (+696)
    bool resizing_ = false;
    bool prevent_close_ = false;
    int resize_start_x_ = 0;
    int resize_start_y_ = 0;

    // Footer rect (+724..+740)
    rect_i footer_rect_{};

    // Logo area
    rect_i logo_area_{};
    bool logo_hovered_ = false;

    // Tab rect
    rect_i tab_rect_{};
    rect_i title_rect_{};

    // Top-right minimize button (rounded square, no shadow). When clicked,
    // toggles minimized_; widgets that observe is_minimized() may collapse.
    rect_i minimize_btn_{};
    bool   minimize_hovered_ = false;
    bool   minimized_        = false;

    // Resize edge detection
    enum class resize_edge {
        none, left, right, top, bottom,
        top_left, top_right, bottom_left, bottom_right
    };
    resize_edge resize_hovered_ = resize_edge::none;
    resize_edge active_resize_edge_ = resize_edge::none;

    // Helpers
    Renderer::rect logo_rect() const;
    resize_edge hit_test_resize(int mx, int my) const;

    static constexpr int resize_margin_ = 6;
};

} // namespace GUI
