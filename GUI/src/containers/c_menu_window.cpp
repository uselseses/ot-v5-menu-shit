// =============================================================================
// c_menu_window.cpp — REWRITTEN FROM DECOMPILED BINARY
// Source: decompiled/c_menu_window__render.c @ 0x140064460
// =============================================================================

#include <GUI/containers/c_menu_window.hpp>
#include <GUI/containers/c_tab_manager.hpp>

#include <GUI/style/colorscheme.hpp>
#include <GUI/style/element_sizes.hpp>
#include <GUI/style/rounding.hpp>
#include <GUI/animation/logo_animation.hpp>
#include <GUI/input/input_state.hpp>
#include <GUI/input/cursor.hpp>
#include <GUI/core/globals.hpp>

#include <Renderer/c_renderer_core.hpp>
#include <Renderer/font.hpp>

#include <algorithm>
#include <cstdint>

namespace GUI {

namespace {
// User 2026-05-31: min size == default size (800×600 scaled). The menu
// can't be shrunk below its default. Matches the binary behaviour
// where both were 800×600 (resize works one-way: only grow).
constexpr int k_menu_min_w_base = 800;
constexpr int k_menu_min_h_base = 600;

inline int min_w_scaled() {
    const float k = style::gui_scale();
    return static_cast<int>(k_menu_min_w_base * k);
}
inline int min_h_scaled() {
    const float k = style::gui_scale();
    return static_cast<int>(k_menu_min_h_base * k);
}
}

Renderer::rect c_menu_window::logo_rect() const
{
    const auto& sz = style::sizes();
    return {
        static_cast<float>(pos_x_),
        static_cast<float>(pos_y_),
        sz.tab_strip_w,
        sz.footer_margin
    };
}

c_menu_window::resize_edge c_menu_window::hit_test_resize(int mx, int my) const
{
    // User 2026-06-02: resize ONLY through the bottom-right grip
    // triangle — no edge bands at all. The triangle occupies a
    // 12 px square inside the bottom-right of the menu (see the
    // render block for grip_size + offsets). Cursor must be inside
    // that square AND on the wedge (below the hypotenuse) to grab.
    const int x = pos_x_, y = pos_y_, w = width_, h = height_;
    const int grip = 12;
    const int gx0 = x + w - grip - 1;
    const int gy0 = y + h - grip - 1;
    const int gx1 = x + w - 1;
    const int gy1 = y + h - 1;

    // Inside the 12 px square?
    if (mx < gx0 || mx > gx1 || my < gy0 || my > gy1)
        return resize_edge::none;

    // On the wedge? Triangle is (gx1, gy0) – (gx1, gy1) – (gx0, gy1).
    // The hypotenuse runs from (gx1, gy0) to (gx0, gy1) — slope -1.
    // Points BELOW the hypotenuse satisfy:
    //     (mx - gx0) + (my - gy0) >= grip
    if ((mx - gx0) + (my - gy0) < grip)
        return resize_edge::none;

    return resize_edge::bottom_right;
}

// BINARY: c_menu_window__update @ 0x140063E30
void c_menu_window::update(float dt)
{
    // Smooth open / close animation progress
    const float target_open = open_ ? 1.0f : 0.0f;
    const float anim_speed = 16.0f * style::anim_speed();
    open_anim_ += (target_open - open_anim_) * std::clamp(dt * anim_speed, 0.0f, 1.0f);
    if (std::abs(target_open - open_anim_) < 0.001f) {
        open_anim_ = target_open;
    }

    if (open_anim_ <= 0.001f) {
        dragging_ = false;
        resizing_ = false;
        has_focus_ = false;
        logo_hovered_ = false;
        return;
    }

    globals::g_current_layout_ctx = this;

    const auto& sz = style::sizes();
    auto& inp = input::current();
    auto& cur = input::cursor();

    const int mx = static_cast<int>(inp.mouse_pos.x);
    const int my = static_cast<int>(inp.mouse_pos.y);

    // Sync the resize-handler members to the DPI-scaled minimums so
    // every resize edge path enforces the same clamp.
    min_width_  = min_w_scaled();
    min_height_ = min_h_scaled();

    width_ = std::max(width_, min_w_scaled());
    height_ = std::max(height_, min_h_scaled());

    // Update bounds from pos/size
    bounds_.x = static_cast<float>(pos_x_);
    bounds_.y = static_cast<float>(pos_y_);
    bounds_.w = static_cast<float>(width_);
    bounds_.h = static_cast<float>(height_);

    // Binary offset +624: content_rect (title_rect in decompile)
    content_rect_.x = pos_x_ + static_cast<int>(sz.tab_strip_w);
    content_rect_.y = pos_y_;
    content_rect_.w = width_ - static_cast<int>(sz.tab_strip_w);
    content_rect_.h = height_;

    // Binary offset +724: footer_rect
    footer_rect_.x = pos_x_;
    footer_rect_.y = pos_y_ + height_ - static_cast<int>(sz.title_bar_h);
    footer_rect_.w = static_cast<int>(sz.tab_strip_w);
    footer_rect_.h = static_cast<int>(sz.title_bar_h);

    // Logo area (top-left, for dragging)
    logo_area_.x = pos_x_;
    logo_area_.y = pos_y_;
    logo_area_.w = static_cast<int>(sz.tab_strip_w);
    logo_area_.h = static_cast<int>(sz.footer_margin);

    // Logo hover — the rainbow tint eases in/out via logo_hover_anim_.
    auto lr = logo_rect();
    logo_hovered_ = lr.contains(inp.mouse_pos);
    globals::g_logo_anim_enabled = logo_hovered_;
    logo_hover_anim_ += ((logo_hovered_ ? 1.0f : 0.0f) - logo_hover_anim_)
                      * std::min(1.0f, dt * 12.0f);
    // Keep the rainbow animating while fading OUT too (not just while
    // hovered) so the colour recedes smoothly instead of snapping white.
    animation::logo_anim().set_hovered(logo_hover_anim_ > 0.001f);

    // Modal guard — when a popup owns modal or window is closing, block input
    const bool input_blocked = inp.input_blocked_by_other(this) || !open_;

    // Dragging is allowed ONLY from the top logo panel.
    bool in_drag_area = (mx >= logo_area_.x && mx < logo_area_.x + logo_area_.w &&
                         my >= logo_area_.y && my < logo_area_.y + logo_area_.h);

    if (in_drag_area && !dragging_ && !resizing_ && !input_blocked) {
        cur.set(input::cursor_type::hand);
        if (inp.mouse_pressed[input::mb_left]) {
            dragging_ = true;
            has_focus_ = true;
            drag_start_x_ = mx;
            drag_start_y_ = my;
            // Claim modal so peer widgets (tabs, checkboxes, sliders,
            // groupbox chevrons …) stop reacting to hover/click while
            // the cursor sweeps over them mid-drag (user 2026-06-02).
            inp.claim_modal(this, [this]{ dragging_ = false; resizing_ = false; });
        }
    }

    if (dragging_) {
        cur.set(input::cursor_type::size_all);
        if (inp.mouse_down[input::mb_left]) {
            pos_x_ += mx - drag_start_x_;
            pos_y_ += my - drag_start_y_;
            drag_start_x_ = mx;
            drag_start_y_ = my;
        } else {
            dragging_ = false;
            inp.release_modal(this);
        }
    }

    // Resize handling — also blocked while a popup owns the modal.
    if (!dragging_ && !input_blocked) {
        resize_hovered_ = hit_test_resize(mx, my);

        switch (resize_hovered_) {
            case resize_edge::left:
            case resize_edge::right:
                cur.set(input::cursor_type::size_we);
                break;
            case resize_edge::top:
            case resize_edge::bottom:
                cur.set(input::cursor_type::size_ns);
                break;
            case resize_edge::top_left:
            case resize_edge::bottom_right:
                cur.set(input::cursor_type::size_nwse);
                break;
            case resize_edge::top_right:
            case resize_edge::bottom_left:
                cur.set(input::cursor_type::size_nesw);
                break;
            default:
                break;
        }

        if (resize_hovered_ != resize_edge::none && inp.mouse_pressed[input::mb_left]) {
            resizing_ = true;
            active_resize_edge_ = resize_hovered_;
            // ABSOLUTE anchor: remember where inside the grip the
            // cursor grabbed relative to the bottom-right corner. Size
            // then FOLLOWS the cursor position each frame instead of
            // accumulating per-frame deltas — the menu cannot grow
            // until the cursor actually moves past the grabbed corner
            // (user 2026-06-09: dragging outward from inside the body
            // must not enlarge the window).
            resize_start_x_ = (pos_x_ + width_)  - mx;   // grab offset x
            resize_start_y_ = (pos_y_ + height_) - my;   // grab offset y
            inp.consume_mouse_press();
            // Claim modal so peers don't react while the cursor moves
            // through them during the resize drag.
            inp.claim_modal(this, [this]{ dragging_ = false; resizing_ = false; });
        }
    }

    if (resizing_) {
        // Only the bottom-right grip exists (hit_test_resize never
        // returns another edge) — the old per-edge delta switch was
        // dead code.
        width_  = std::max(min_width_,  (mx + resize_start_x_) - pos_x_);
        height_ = std::max(min_height_, (my + resize_start_y_) - pos_y_);

        if (!inp.mouse_down[input::mb_left]) {
            resizing_ = false;
            active_resize_edge_ = resize_edge::none;
            inp.release_modal(this);
        }
    }

    width_ = std::max(width_, min_w_scaled());
    height_ = std::max(height_, min_h_scaled());

    bounds_.x = static_cast<float>(pos_x_);
    bounds_.y = static_cast<float>(pos_y_);
    bounds_.w = static_cast<float>(width_);
    bounds_.h = static_cast<float>(height_);
    content_rect_.x = pos_x_ + static_cast<int>(sz.tab_strip_w);
    content_rect_.y = pos_y_;
    content_rect_.w = width_ - static_cast<int>(sz.tab_strip_w);
    content_rect_.h = height_;
    footer_rect_.x = pos_x_;
    footer_rect_.y = pos_y_ + height_ - static_cast<int>(sz.title_bar_h);
    footer_rect_.w = static_cast<int>(sz.tab_strip_w);
    footer_rect_.h = static_cast<int>(sz.title_bar_h);

    // Update children
    for (auto& child : children_) {
        if (child) child->set_bounds(rect_t{
            static_cast<float>(content_rect_.x),
            static_cast<float>(content_rect_.y),
            static_cast<float>(content_rect_.w),
            static_cast<float>(content_rect_.h),
        });
        if (child) child->update(dt);
    }
}

// BINARY: c_menu_window__render @ 0x140064460
// Exact render order from decompiled pseudocode:
//   1. render_filled_rect(bounds, 0xFF232328) - body fill
//   2. render_filled_rect({x,y,tab_strip_w,h}, 0xFF19191D) - tab strip
//   3. render_filled_rect(content_rect, 0xFF151519) - content area
//   4. render_textured_rect - gradient overlay (skipped, needs texture)
//   5. render_filled_rect_1px - horizontal sep at footer_margin (0xFF3E3E42)
//   6. render_filled_rect_1px - vertical sep at tab_strip_w (0xFF3E3E42)
//   7. render_filled_rect(footer_rect, 0xFF151519) - footer
//   8. render_rect_outline(footer_rect offset)
//   9. render_filled_rect_1px - footer top line (0xFF3E3E42)
//  10. render_textured_rect - logo icon (skipped, needs texture)
//  11. render_text - "William" (0xFFDEDEDE)
//  12. render_text - "01/01/3000" (accent 0xFFFAA618)
void c_menu_window::render(Renderer::c_renderer_core& r)
{
    if (open_anim_ <= 0.001f) return;

    const float t = std::clamp(open_anim_, 0.0f, 1.0f);
    const float eased = t * t * (3.0f - 2.0f * t);

    // Apply alpha modulation for entire menu window and all its children
    r.push_alpha(eased);

    // Snapshot draw cursor before rendering window and all children
    const auto draw_cur = r.capture_cursor();

    const auto& sz = style::sizes();
    const float menu_radius = style::rounding().of(style::rounding().menu_window);

    const float x = static_cast<float>(pos_x_);
    const float y = static_cast<float>(pos_y_);
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);

    // 2. Tab strip (left panel) - 0xFF19191D. Its LEFT corners follow the
    // window's rounded body.
    {
        const Renderer::color strip_col{0x19, 0x19, 0x1D, 0xFF};
        const float rr_body = style::rounding().menu_window
                                  ? style::rounding().chrome_radius()
                                  : 0.0f;
        const Renderer::rect strip{x, y, sz.tab_strip_w, h};
        if (rr_body > 0.0f) {
            const float radii[4] = {rr_body, 0.0f, 0.0f, rr_body};
            r.render_filled_rounded_rect_ex(strip, strip_col, radii);
        } else {
            r.render_filled_rect(strip, strip_col);
        }
    }

    // 3. Content area — 0xFF232328 (same as active tab fill), so the
    // active tab visually flows into the page content. RIGHT corners
    // follow the window curve.
    {
        const Renderer::color content_col{0x23, 0x23, 0x28, 0xFF};
        const float rr_body = style::rounding().menu_window
                                  ? style::rounding().chrome_radius()
                                  : 0.0f;
        const Renderer::rect cr{
            static_cast<float>(content_rect_.x),
            static_cast<float>(content_rect_.y),
            static_cast<float>(content_rect_.w),
            static_cast<float>(content_rect_.h)};
        if (rr_body > 0.0f) {
            const float radii[4] = {0.0f, rr_body, rr_body, 0.0f};
            r.render_filled_rounded_rect_ex(cr, content_col, radii);
        } else {
            r.render_filled_rect(cr, content_col);
        }
    }

    // 4. Gradient overlay - skipped (needs g_texture_gradient)

    // Tab strip separators — single 1-pixel lines per binary
    // c_menu_window__render @ 0x140064460 lines 145-158 (color 0xFF3E3E42 /
    // 4282269250).
    const Renderer::color sep{0x3E, 0x3E, 0x42, 0xFF};
    r.render_filled_rect(Renderer::rect{x, y + sz.footer_margin, sz.tab_strip_w, 1.0f}, sep);
    r.render_filled_rect(Renderer::rect{x + sz.tab_strip_w, y, 1.0f, h}, sep);

    // 7. Footer rect fill - 0xFF151519. Bottom-left corner follows the
    // window curve.
    const float fx = static_cast<float>(footer_rect_.x);
    const float fy = static_cast<float>(footer_rect_.y);
    const float fw = static_cast<float>(footer_rect_.w);
    const float fh = static_cast<float>(footer_rect_.h);
    {
        const Renderer::color footer_col{0x15, 0x15, 0x19, 0xFF};
        const float rr_body = style::rounding().menu_window
                                  ? style::rounding().chrome_radius()
                                  : 0.0f;
        const Renderer::rect fr{fx, fy, fw, fh};
        if (rr_body > 0.0f) {
            const float radii[4] = {0.0f, 0.0f, 0.0f, rr_body};
            r.render_filled_rounded_rect_ex(fr, footer_col, radii);
        } else {
            r.render_filled_rect(fr, footer_col);
        }
    }

    // William footer border — 1-pixel SHARP outline on top, left, bottom only.
    // The right edge is the tab-strip vertical separator already drawn above,
    // so doubling up there gives a 2-pixel border. Use three sharp 1-px
    // lines instead of an outlined rect.
    const Renderer::color wm_border{0x3E, 0x3E, 0x42, 0xFF};
    r.render_filled_rect(Renderer::rect{fx,            fy,            fw,   1.0f}, wm_border); // top
    r.render_filled_rect(Renderer::rect{fx,            fy + fh - 1.f, fw,   1.0f}, wm_border); // bottom
    r.render_filled_rect(Renderer::rect{fx,            fy,            1.0f, fh},   wm_border); // left
    // No right edge — the existing vertical strip separator covers it.

    // 10. Top logo texture (qword_140245F20, 320x200) and footer profile
    // texture (qword_140245FA8, 48x48).
    if (logo_texture_.srv) {
        const float logo_w = sz.tab_strip_w;
        const float logo_h = sz.footer_margin;
        const Renderer::rect lr{x, y, logo_w, logo_h};
        // Idle = desaturated B&W skull; hover = the colour logo brightening
        // in. User 2026-06-10: animate the transition both on hover ON and
        // OFF (driven by logo_hover_anim_), NOT the rainbow tint.
        const auto& gray_tex =
            logo_gray_texture_.srv ? logo_gray_texture_ : logo_texture_;
        const float t = std::clamp(logo_hover_anim_, 0.0f, 1.0f);
        r.render_texture(lr, gray_tex, Renderer::color{0x70, 0x70, 0x70, 0xC0});
        if (t > 0.001f) {
            // Colour logo fades in over the grayscale base…
            r.render_texture(lr, logo_texture_,
                Renderer::color{0x70, 0x70, 0x70,
                                static_cast<std::uint8_t>(t * 0xC0)});
            // …plus the dumped 0x60FFFFFF brightening overlay.
            r.render_texture(lr, logo_texture_,
                Renderer::color{0xFF, 0xFF, 0xFF,
                                static_cast<std::uint8_t>(t * 0x60)});
        }
    }

    const float icon_x = x + sz.footer_text_margin;
    const float icon_y = y + h - sz.footer_text_margin - sz.footer_icon_size;
    if (profile_texture_.srv) {
        r.render_texture(Renderer::rect{icon_x, icon_y, sz.footer_icon_size, sz.footer_icon_size},
                         profile_texture_, Renderer::color{0xFF, 0xFF, 0xFF, 0xFF});
    } else {
        r.render_filled_rect(Renderer::rect{icon_x, icon_y, sz.footer_icon_size, sz.footer_icon_size},
                             Renderer::color{0xFF, 0xFF, 0xFF, 0x80});
    }
    // NO avatar outline — the original draws the profile texture bare
    // (user 2026-06-11: "border for the profile picture... shouldnt even
    // be there, its not on the original").

    // 11-12. Footer text — username pinned to the avatar's TOP, expiry to
    // the avatar's BOTTOM, so both lines stay within the avatar's vertical
    // span (user 2026-06-11: "name at top of the picture, expire date at
    // bottom — name doesn't go outside it but expire did").
    if (title_font_) {
        const float text_x = x + sz.footer_icon_size + 2.0f * sz.footer_text_margin - 2.0f;
        const float fh_line = title_font_->pixel_height();

        // Username — pinned to the avatar TOP. render_text places pos.y at
        // the EM-box top, which sits a few px below the visible cap, so we
        // lift it by the ascent slack to make the glyphs hug the avatar top
        // (user 2026-06-11: "username is still too low").
        const float top_slack = fh_line * 0.22f;
        r.render_text(*title_font_, {text_x, icon_y - top_slack},
                      Renderer::color{0xDE, 0xDE, 0xDE, 0xFF}, "anonteam.net");

        // Expiry date — pinned to the avatar BOTTOM.
        const float date_y = icon_y + sz.footer_icon_size - fh_line + top_slack;
        r.render_text(*title_font_, {text_x, date_y},
                      style::current().accent, "01/01/2077");
    }

    // 13. Render children (tab_manager, etc.)
    for (auto& child : children_) {
        if (child) child->render(r);
    }

    // Re-STROKE the two chrome rings after children — child fills (tab
    // strip, logo, pages) overdraw the rounded frame, which left the
    // border partially missing when rounded (user 2026-06-11 "borders
    // dont render all when rounded"). Same two colours as the filled
    // rings above — still exactly a 2-px border, no third ring.
    {
        const float rr = style::rounding().menu_window
                             ? style::rounding().chrome_radius()
                             : 0.0f;
        r.render_rounded_rect(Renderer::rect{x - 2.0f, y - 2.0f, w + 4.0f, h + 4.0f},
                              Renderer::color{0x20, 0x20, 0x24, 0xFF},
                              rr + 2.0f, 1.0f);
        r.render_rounded_rect(Renderer::rect{x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f},
                              Renderer::color{0x60, 0x60, 0x64, 0xFF},
                              rr + 1.0f, 1.0f);
    }
    (void)menu_radius;

    // Resize grip — user 2026-06-10: the filled wedge looked wrong; the
    // original uses the conventional corner "gripper" of a few short
    // diagonal hatch lines stepping in from the bottom-right corner.
    const Renderer::color grip_col{0x6A, 0x6A, 0x6E, 0xFF};
    const float gx1 = x + w - 3.0f;   // just inside the outer border
    const float gy1 = y + h - 3.0f;
    // Three parallel 45° lines of increasing length toward the corner.
    for (int i = 0; i < 3; ++i) {
        const float off = 3.0f + i * 4.0f;   // 3, 7, 11 px from corner
        r.render_line({gx1 - off, gy1}, {gx1, gy1 - off}, grip_col, 1.0f);
    }

    r.pop_alpha();

    // If animating, slide the ENTIRE recorded menu (background, tabs, groupboxes, text, sliders)
    // together in lockstep as a single unified entity
    if (open_anim_ < 0.999f) {
        const float slide_y = (1.0f - eased) * -16.0f;
        const Renderer::vec2 center{x + w * 0.5f, y + h * 0.5f};
        r.transform_recorded(draw_cur, center, 1.0f, {0.0f, slide_y});
    }
}

} // namespace GUI
