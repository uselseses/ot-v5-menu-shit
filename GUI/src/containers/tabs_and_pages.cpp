// =============================================================================
// tabs_and_pages.cpp
// Faithful port of c_tab_manager__render @ 0x14006A540 and friends.
//
// Render layout exactly as the binary draws it:
//   - The left strip is already filled 0xFF19191D by c_menu_window__render.
//     c_tab_manager does NOT paint inactive tab backgrounds; inactive tabs
//     simply render their label over the strip fill.
//   - Active tab paints a 0xFF232328 (4280492840) rect across {ctx.x, y,
//     strip_w + 2, row_h}. This is what makes the active tab "pop" out of
//     the darker strip — there is no border outline, no accent ribbon, no
//     hover overlay; just a colour swap.
//   - Subtab rows under the active tab also paint 0xFF232328 across
//     {ctx.x, y, strip_w + 2, groupbox_padding + subtab_h}. The fill height
//     is (groupbox_padding + subtab_h) and the loop only advances by
//     subtab_h, so the last row overhangs by groupbox_padding — this is what
//     produces the small "tail" below the last subtab.
//   - Tab label text is centered on row, colour:
//       active   : 0xFFDEDEDE (-2171170)
//       inactive : lerped between 0xFFAAAAAA (-5592406) and 0xFFD2D2D2
//                  (-2960686) by anim_calc_progress_2ch on hover.
//   - Subtab label text is at the same vertical center as the row, colour:
//       active   : g_style_accent_color (0xFFFAA618)
//       inactive : lerped 0xFFAAAAAA <-> 0xFFD2D2D2 by hover.
// =============================================================================

#include <GUI/containers/c_tab_manager.hpp>
#include <GUI/containers/c_tab.hpp>
#include <GUI/containers/c_subtab.hpp>
#include <GUI/containers/c_page_manager.hpp>
#include <GUI/containers/c_page.hpp>

#include <GUI/input/input_state.hpp>
#include <GUI/input/cursor.hpp>
#include <GUI/style/colorscheme.hpp>
#include <GUI/style/element_sizes.hpp>
#include <GUI/style/rounding.hpp>
#include <GUI/core/globals.hpp>

#include <Renderer/c_renderer_core.hpp>
#include <Renderer/font.hpp>

namespace GUI {

namespace {
inline Renderer::rect to_rrect(c_base_element::rect_t r) {
    return Renderer::rect{r.x, r.y, r.w, r.h};
}

} // anon

// ============================================================================
// c_tab_manager
// ============================================================================

std::shared_ptr<c_tab> c_tab_manager::set_active_tab(std::shared_ptr<c_tab> t)
{
    if (active_ != t) {
        page_anim_alpha_ = 0.0f;
    }
    auto prev = active_;
    active_ = std::move(t);
    active_tab_idx_ = 0;
    for (auto& child : children_) {
        if (child == active_) break;
        ++active_tab_idx_;
    }
    return prev;
}

// BINARY: c_tab_manager::update @ 0x14006A4F0
//   a1[24] = ctx.x
//   a1[25] = footer_margin + ctx.y + 1                 (row Y start)
//   a1[26] = tab_strip_w + 1                           (row width)
//   a1[27] = ctx.h - tab_row_h - footer_margin - 1     (row block height)
// Then render() writes tab_row_rect at offsets 336..348 with slightly
// different math (strip_w + 2 instead of +1, h - title_bar_h instead of
// h - tab_row_h). We follow render()'s values.
void c_tab_manager::update(float dt)
{
    const auto& sz  = style::sizes();
    auto& in        = input::current();
    auto& cur       = input::cursor();

    if (globals::g_current_layout_ctx) {
        auto ctx = globals::g_current_layout_ctx;
        const float ctx_x = ctx->bounds().x;
        const float ctx_y = ctx->bounds().y;
        const float ctx_h = ctx->bounds().h;

        tab_row_rect_.x = ctx_x;
        tab_row_rect_.y = ctx_y + sz.footer_margin + 1.0f;
        tab_row_rect_.w = sz.tab_strip_w + 2.0f;
        tab_row_rect_.h = ctx_h - sz.title_bar_h - sz.footer_margin - 1.0f;

        content_rect_.x = ctx_x + sz.tab_strip_w;
        content_rect_.y = ctx_y;
        content_rect_.w = ctx->bounds().w - sz.tab_strip_w;
        content_rect_.h = ctx_h;

        bounds_ = tab_row_rect_;
    }

    // Per-frame hover + click for tabs and subtabs.
    // When a popup (dropdown / colorpicker / dialog) owns the modal
    // we suppress hover highlight + click on tabs/subtabs so the user
    // can't switch tabs through the open popup.
    const bool input_blocked = in.input_blocked_by_other(this);
    {
        const float base_x = tab_row_rect_.x;
        const float row_h  = sz.tab_row_h;
        const float sub_h  = sz.subtab_strip_h;
        const float strip_w = sz.tab_strip_w;
        const float groupbox_pad = sz.groupbox_padding;

        float y = tab_row_rect_.y;
        bool any_hover = false;
        int idx = 0;

        for (auto& child : children_) {
            if (!child) { ++idx; continue; }
            const bool is_active = (child == active_);

            Renderer::rect tab_rect{base_x, y, strip_w, row_h};
            const bool tab_hovered = !input_blocked &&
                                     tab_rect.contains(in.mouse_pos);
            child->set_hovered(tab_hovered);
            if (tab_hovered) {
                any_hover = true;
                if (in.mouse_pressed[input::mb_left]) {
                    if (auto t = std::dynamic_pointer_cast<c_tab>(child)) {
                        set_active_tab(t);
                        active_tab_idx_ = idx;
                        in.consume_mouse_press();
                    }
                }
            }

            y += row_h;

            if (is_active) {
                if (auto tab = std::dynamic_pointer_cast<c_tab>(child)) {
                    // The subtab fill rect starts (row_h - groupbox_pad) above
                    // its label baseline, but for hit testing use {y, sub_h}.
                    float sub_y = y - groupbox_pad;
                    for (auto& sub : tab->children()) {
                        if (!sub) continue;
                        Renderer::rect sr{base_x, sub_y, strip_w, sub_h};
                        const bool sub_hovered = !input_blocked &&
                                                 sr.contains(in.mouse_pos);
                        sub->set_hovered(sub_hovered);
                        if (sub_hovered) {
                            any_hover = true;
                            if (in.mouse_pressed[input::mb_left]) {
                                if (auto st = std::dynamic_pointer_cast<c_subtab>(sub)) {
                                    tab->set_active_subtab(st);
                                    in.consume_mouse_press();
                                }
                            }
                        }
                        sub_y += sub_h;
                    }
                    y = sub_y + groupbox_pad;
                }
            }
            ++idx;
        }

        if (any_hover) cur.set(input::cursor_type::hand);
    }

    // --- Smooth animation interpolation ---
    const int num_tabs = static_cast<int>(children_.size());
    if (static_cast<int>(tab_active_anims_.size()) != num_tabs)
        tab_active_anims_.assign(num_tabs, 0.0f);
    if (static_cast<int>(tab_hover_anims_.size()) != num_tabs)
        tab_hover_anims_.assign(num_tabs, 0.0f);

    page_anim_alpha_ += (1.0f - page_anim_alpha_) * std::clamp(dt * 18.0f, 0.0f, 1.0f);

    float target_tab_rel_y = 0.0f;
    float target_tab_h = sz.tab_row_h;
    float scan_rel_y = 0.0f;

    for (int i = 0; i < num_tabs; ++i) {
        auto& child = children_[i];
        if (!child) continue;
        const bool is_act = (child == active_);
        const float target_act = is_act ? 1.0f : 0.0f;
        tab_active_anims_[i] += (target_act - tab_active_anims_[i]) * std::clamp(dt * 14.0f * style::anim_speed(), 0.0f, 1.0f);
        const float target_hov = (child->hovered() && !is_act) ? 1.0f : 0.0f;
        tab_hover_anims_[i] += (target_hov - tab_hover_anims_[i]) * std::clamp(dt * 12.0f * style::anim_speed(), 0.0f, 1.0f);

        if (auto tab = std::dynamic_pointer_cast<c_tab>(child)) {
            auto active_sub = tab->active_subtab();
            for (auto& sub : tab->children()) {
                if (auto st = std::dynamic_pointer_cast<c_subtab>(sub)) {
                    const bool is_sub_act = is_act && (st == active_sub);
                    const float s_target_act = is_sub_act ? 1.0f : 0.0f;
                    st->set_active_anim(st->active_anim() + (s_target_act - st->active_anim()) * std::clamp(dt * 14.0f * style::anim_speed(), 0.0f, 1.0f));

                    const float s_target_hov = (st->hovered() && !is_sub_act) ? 1.0f : 0.0f;
                    st->set_hover_anim(st->hover_anim() + (s_target_hov - st->hover_anim()) * std::clamp(dt * 12.0f * style::anim_speed(), 0.0f, 1.0f));
                }
            }
        }

        if (is_act) {
            target_tab_rel_y = scan_rel_y;
            float block_h = sz.tab_row_h;
            if (auto tab = std::dynamic_pointer_cast<c_tab>(child)) {
                const auto& sub_kids = tab->children();
                if (!sub_kids.empty()) {
                    block_h = sz.tab_row_h + static_cast<float>(sub_kids.size()) * sz.subtab_strip_h;
                }
            }
            target_tab_h = block_h;
            scan_rel_y += block_h;
        } else {
            scan_rel_y += sz.tab_row_h;
        }
    }

    if (!anim_tab_init_) {
        anim_tab_rel_y_ = target_tab_rel_y;
        anim_tab_h_ = target_tab_h;
        anim_tab_init_ = true;
    } else {
        const float speed = std::clamp(dt * 18.0f * style::anim_speed(), 0.0f, 1.0f);
        anim_tab_rel_y_ += (target_tab_rel_y - anim_tab_rel_y_) * speed;
        anim_tab_h_ += (target_tab_h - anim_tab_h_) * speed;
    }

    if (active_) {
        active_->set_bounds(content_rect_);
        active_->update(dt);
    }
}

// BINARY: c_tab_manager::render @ 0x14006A540
void c_tab_manager::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    const auto& sz = style::sizes();

    // Colours (decimal in decompile shown after each):
    const Renderer::color c_active_bg{0x23, 0x23, 0x28, 0xFF};     // 4280492840
    const Renderer::color c_text_active   {0xDE, 0xDE, 0xDE, 0xFF}; // -2171170
    const Renderer::color c_text_inactive {0xAA, 0xAA, 0xAA, 0xFF}; // -5592406
    const Renderer::color c_text_hover    {0xD2, 0xD2, 0xD2, 0xFF}; // -2960686
    const Renderer::color c_subtab_active = style::current().accent;  // live accent
    const Renderer::color c_border_dark {0x20, 0x20, 0x24, 0xFF};   // 4280295460
    const Renderer::color c_border_light{0x3E, 0x3E, 0x42, 0xFF};   // 4282269250

    const float base_x       = tab_row_rect_.x;                 // ctx.x
    const float strip_w      = sz.tab_strip_w;
    const float row_h        = sz.tab_row_h;
    const float sub_h        = sz.subtab_strip_h;
    const float groupbox_pad = sz.groupbox_padding;
    const float text_x       = base_x + sz.tab_label_padding_x; // ctx.x + 0x108
    const float sub_text_x   = text_x + sz.tab_inner_pad;       // + 0x100
    const float row_half_h   = row_h * 0.5f;
    const float sub_half_h   = sub_h * 0.5f;

    const float anim_tab_y = tab_row_rect_.y + anim_tab_rel_y_;

    // (1) Smooth active tab highlight background
    r.render_filled_rect({base_x, anim_tab_y, strip_w, anim_tab_h_}, c_active_bg);

    // (1b) Top border — smoothly sliding with active block
    const bool is_first_tab = (active_tab_idx_ == 0);
    if (is_first_tab && std::abs(anim_tab_rel_y_) < 2.0f) {
        r.render_filled_rect({base_x, anim_tab_y, strip_w, 1.0f}, c_border_dark);
    } else {
        r.render_filled_rect({base_x, anim_tab_y,        strip_w, 1.0f}, c_border_light);
        r.render_filled_rect({base_x, anim_tab_y + 1.0f, strip_w, 1.0f}, c_border_dark);
    }

    // (1c) Bottom border — smoothly sliding with active block
    const float bottom_y = anim_tab_y + anim_tab_h_;
    r.render_filled_rect({base_x, bottom_y - 1.0f, strip_w, 1.0f}, c_border_dark);
    r.render_filled_rect({base_x, bottom_y,        strip_w, 1.0f}, c_border_light);

    // (2) Render tab labels and subtab rows
    float y = tab_row_rect_.y;
    int idx = 0;

    for (auto& child : children_) {
        if (!child) { ++idx; continue; }
        const bool is_active = (child == active_);

        const float act_w = (idx < static_cast<int>(tab_active_anims_.size())) ? tab_active_anims_[idx] : (is_active ? 1.0f : 0.0f);
        const float hov_w = (idx < static_cast<int>(tab_hover_anims_.size())) ? tab_hover_anims_[idx] : (child->hovered() ? 1.0f : 0.0f);

        // Smooth text color lerp: inactive -> hover -> active
        Renderer::color base_tc = Renderer::color::lerp(c_text_inactive, c_text_hover, hov_w);
        Renderer::color tc = Renderer::color::lerp(base_tc, c_text_active, act_w);

        if (tab_font_) {
            const float ty = y + row_half_h - tab_font_->pixel_height() * 0.5f;
            r.render_text(*tab_font_, {text_x, ty}, tc, child->label().c_str());
        }

        if (is_active) {
            if (auto tab = std::dynamic_pointer_cast<c_tab>(child)) {
                float sub_y = y + row_h - groupbox_pad;
                for (auto& sub : tab->children()) {
                    if (!sub) continue;
                    if (tab_font_) {
                        if (auto st = std::dynamic_pointer_cast<c_subtab>(sub)) {
                            const float s_hov = st->hover_anim();
                            const float s_act = st->active_anim();
                            Renderer::color base_sub_col = Renderer::color::lerp(c_text_inactive, c_text_hover, s_hov);
                            Renderer::color sub_tc = Renderer::color::lerp(base_sub_col, c_subtab_active, s_act);
                            const float ty = sub_y + sub_half_h - tab_font_->pixel_height() * 0.5f;
                            r.render_text(*tab_font_, {sub_text_x, ty}, sub_tc, sub->label().c_str());
                        }
                    }
                    sub_y += sub_h;
                }
                y = sub_y + groupbox_pad;
            } else {
                y += row_h;
            }
        } else {
            y += row_h;
        }
        ++idx;
    }

    // (3) Render active page with smooth fade-in
    if (active_) {
        r.push_alpha(page_anim_alpha_);
        active_->render(r);
        r.pop_alpha();
    }
}

// ============================================================================
// c_tab
// ============================================================================

std::shared_ptr<c_subtab> c_tab::set_active_subtab(std::shared_ptr<c_subtab> s)
{
    if (active_ != s) {
        subtab_anim_alpha_ = 0.0f;
    }
    auto prev = active_;
    active_ = std::move(s);
    active_subtab_idx_ = 0;
    for (auto& child : children_) {
        if (child == active_) break;
        ++active_subtab_idx_;
    }
    return prev;
}

void c_tab::update(float dt)
{
    subtab_anim_alpha_ += (1.0f - subtab_anim_alpha_) * std::clamp(dt * 18.0f, 0.0f, 1.0f);
    if (active_) {
        active_->set_bounds(bounds_);
        active_->update(dt);
    }
}

void c_tab::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    if (active_) {
        r.push_alpha(subtab_anim_alpha_);
        active_->render(r);
        r.pop_alpha();
    }
}

// ============================================================================
// c_subtab
// ============================================================================

void c_subtab::update(float dt)
{
    for (auto& c : children_) {
        if (!c) continue;
        c->set_bounds(bounds_);
        c->update(dt);
    }
}

void c_subtab::render(Renderer::c_renderer_core& r)
{
    c_base_element::render(r);
}

// ============================================================================
// c_page_manager
// ============================================================================

std::shared_ptr<c_page> c_page_manager::set_active_page(std::shared_ptr<c_page> p)
{
    if (active_ != p) {
        animation_progress_ = 0.0f;
    }
    auto prev = active_;
    active_ = std::move(p);
    return prev;
}

void c_page_manager::update(float dt)
{
    animation_progress_ += (1.0f - animation_progress_) * std::clamp(dt * 18.0f, 0.0f, 1.0f);
    content_rect_ = bounds_;
    if (active_) {
        active_->set_bounds(bounds_);
        active_->update(dt);
    }
}

void c_page_manager::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    if (active_) {
        r.push_alpha(animation_progress_);
        active_->render(r);
        r.pop_alpha();
    }
}

// ============================================================================
// c_page
// ============================================================================

void c_page::update(float dt)
{
    const auto& sz = style::sizes();
    auto& in = input::current();

    outer_bounds_ = bounds_;            // snapshot for outer_bounds()

    content_rect_.x = bounds_.x + sz.page_padding_x;
    content_rect_.y = bounds_.y + sz.page_gap;
    content_rect_.w = bounds_.w - 2.0f * sz.page_padding_x;
    content_rect_.h = bounds_.h - sz.page_gap;

    // The child layout rect is the content rect SHIFTED UP by scroll_y so
    // groupboxes/columns physically lay out off-screen above the page when
    // scrolled. The clip in render() hides the off-screen part.
    rect_t scrolled = content_rect_;
    scrolled.y -= scroll_y_;
    next_y_ = scrolled.y;

    float saved_x = bounds_.x;
    float saved_w = bounds_.w;
    bounds_.x = scrolled.x;
    bounds_.w = scrolled.w;

    // Update children FIRST so nested scrollable widgets (c_listbox, c_text_container, c_dropdown)
    // can process input and consume wheel_delta if they are hovered/scrolled!
    for (auto& c : children_) {
        if (!c || !c->visible()) continue;
        if (c->parent().expired()) c->set_parent(weak_from_this());
        c->set_bounds(scrolled);
        c->update(dt);
    }

    content_height_ = (next_y_ + scroll_y_) - content_rect_.y;

    bounds_.x = saved_x;
    bounds_.w = saved_w;

    // Modal guard — popups own scroll + thumb input. Page must NOT
    // consume the mouse wheel or hit-test the scrollbar thumb while
    // a dropdown/colorpicker/dialog is up.
    const bool input_blocked = in.input_blocked_by_other(this);

    // Mouse-wheel scrolling for the page — only if children did NOT consume wheel_delta!
    const float viewport_h = content_rect_.h;
    const float overflow = std::max(0.0f, content_height_ - viewport_h);
    if (overflow > 0.0f && !input_blocked) {
        Renderer::rect hit{bounds_.x, bounds_.y, bounds_.w, bounds_.h};
        if (hit.contains(in.mouse_pos) && in.wheel_delta != 0.0f) {
            target_scroll_y_ -= in.wheel_delta * 48.0f;   // ~48 px per notch
        }
    }

    // Scrollbar thumb hit-test + drag. We hit-test against an EXPANDED rect
    // (a few px on each side) so the thin pill is still grabbable.
    if (overflow > 0.0f && !input_blocked) {
        const Renderer::rect thumb_hit{
            scrollbar_thumb_.x - 4.0f, scrollbar_thumb_.y - 2.0f,
            scrollbar_thumb_.w + 8.0f, scrollbar_thumb_.h + 4.0f };
        thumb_hovered_ = thumb_hit.contains(in.mouse_pos);

        if (in.mouse_pressed[input::mb_left] && thumb_hovered_) {
            thumb_pressed_  = true;
            thumb_drag_off_ = in.mouse_pos.y - scrollbar_thumb_.y;
            in.consume_mouse_press();
        }
        if (!in.mouse_down[input::mb_left]) thumb_pressed_ = false;

        if (thumb_pressed_ && content_height_ > viewport_h) {
            const float track_y = content_rect_.y;
            const float track_h = viewport_h;
            const float ratio   = viewport_h / content_height_;
            const float thumb_h = std::max(20.0f, track_h * ratio);
            const float new_thumb_y = in.mouse_pos.y - thumb_drag_off_;
            float t = (new_thumb_y - track_y) / (track_h - thumb_h);
            if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
            target_scroll_y_ = t * overflow;
            scroll_y_        = target_scroll_y_;   // no smoothing while dragging
        }
    } else {
        thumb_hovered_ = thumb_pressed_ = false;
    }

    // Grow animation — thumb expands when hovered/held.
    const float grow_target = (thumb_hovered_ || thumb_pressed_) ? 1.0f : 0.0f;
    thumb_grow_ += (grow_target - thumb_grow_) * std::min(1.0f, dt * 14.0f);

    target_scroll_y_ = std::max(0.0f, std::min(target_scroll_y_, overflow));
    if (!thumb_pressed_)
        scroll_y_ += (target_scroll_y_ - scroll_y_) * std::min(1.0f, dt * 12.0f);
}

void c_page::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    // User 2026-06-01: clip children to the INSET content_rect_ (not
    // the outer bounds). Outer bounds includes the page_gap above
    // content — scrolled checkboxes were rendering INTO that gap
    // because the clip used outer bounds. Inset clip stops content
    // from spilling above/below the visible viewport.
    //
    // 2026-06-02: the binary clips page content to the FULL menu
    // interior on BOTH axes — not to the inset content_rect_. The
    // page_padding_x / page_gap insets are purely for LAYOUT (where
    // child bounds get positioned); the CLIP must reach the menu
    // borders so:
    //   - first-row groupbox titles (drawn at bounds_.y - font_h/2)
    //     aren't sliced in half,
    //   - rounded corners of last-row widgets in a scrolled list
    //     render fully, not flat-clipped by a tight viewport edge,
    //   - edge-spanning widgets like c_icon_tab_strip (Visuals Players
    //     / Rage Weapons) can render their unselected backgrounds and
    //     bottom dividers from menu border to menu border without
    //     being chopped off at page_padding_x on each side.
    rect_t clip = bounds_;
    r.push_clip(to_rrect(clip));
    c_base_element::render(r);
    r.pop_clip();

    // Scrollbar — only when content overflows the viewport. Drawn
    // OUTSIDE the clip so it doesn't get cropped at the page edges.
    const float viewport_h = content_rect_.h;
    const float overflow = std::max(0.0f, content_height_ - viewport_h);
    if (overflow > 0.0f && viewport_h > 0.0f) {
        const auto& cs = style::current();
        // Pill grows from 4 -> 6 px wide on hover/press; track stays slim.
        const float bar_w     = 4.0f;
        const float thumb_w   = bar_w + 2.0f * thumb_grow_;   // 4..6
        const float bar_x     = bounds_.x + bounds_.w - bar_w - 4.0f;
        const float thumb_x   = bar_x + (bar_w - thumb_w) * 0.5f;
        const float track_y   = content_rect_.y;
        const float track_h   = viewport_h;

        // Track — full pill (rounded both ends)
        r.render_filled_rounded_rect({bar_x, track_y, bar_w, track_h},
                                     cs.scrollbar_bg, bar_w * 0.5f);

        // Thumb
        const float ratio    = viewport_h / content_height_;
        const float thumb_h  = std::max(20.0f, track_h * ratio);
        const float thumb_y  = track_y + (scroll_y_ / overflow) * (track_h - thumb_h);

        // Pressed = darker; hover = lighter; idle = base.
        Renderer::color thumb_color = cs.scrollbar_thumb;
        if (thumb_pressed_) {
            // Held — deeper / saturated grey
            thumb_color = Renderer::color{0x60, 0x60, 0x66, 0xFF};
        } else if (thumb_hovered_) {
            // Hover — slightly lighter than base
            thumb_color = Renderer::color{0x55, 0x55, 0x5A, 0xFF};
        }

        // Cache rect for next frame's hit-test
        scrollbar_thumb_ = rect_t{thumb_x, thumb_y, thumb_w, thumb_h};

        r.render_filled_rounded_rect({thumb_x, thumb_y, thumb_w, thumb_h},
                                     thumb_color, thumb_w * 0.5f);
    }
}

} // namespace GUI
