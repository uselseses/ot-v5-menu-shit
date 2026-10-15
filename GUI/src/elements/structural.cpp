// =============================================================================
// structural.cpp — REWRITTEN FROM DECOMPILED BINARY
// Source: decompiled/c_groupbox__render.c @ 0x140060700
// =============================================================================

#include <algorithm>

#include <GUI/elements/c_groupbox.hpp>
#include <GUI/elements/c_column.hpp>
#include <GUI/elements/c_scrollable_element.hpp>
#include <GUI/elements/c_body.hpp>

#include <GUI/core/layout_context.hpp>
#include <GUI/input/cursor.hpp>
#include <GUI/input/input_state.hpp>
#include <GUI/style/colorscheme.hpp>
#include <GUI/style/element_sizes.hpp>
#include <GUI/style/rounding.hpp>
#include <GUI/style/textures.hpp>
#include <GUI/core/globals.hpp>

#include <Renderer/c_renderer_core.hpp>
#include <Renderer/font.hpp>

#include <algorithm>
#include <cmath>

namespace GUI {

namespace {
inline Renderer::rect to_rrect(c_base_element::rect_t r) {
    return Renderer::rect{r.x, r.y, r.w, r.h};
}
} // anon

// ============================================================================
// c_groupbox
// ============================================================================

// BINARY: c_groupbox__update @ 0x140060290
//
// Animation per [[groupbox-chevron-anim]] memory note:
//   - body height tween uses ease_in_out_cubic with ~200 ms duration
//   - chevron rotation INCREMENTS by 180 on every click and tweens
//     toward target so it keeps spinning clockwise across toggles
void c_groupbox::update(float dt)
{
    const auto& sz = style::sizes();
    auto& in = input::current();

    // BINARY: c_groupbox__update.c:152-206 — the height tween is a
    // TIME-BASED fraction of ~200 ms run through ease_in_out_cubic,
    // then height = total_h * t (expanding) / total_h - total_h * t
    // (collapsing). The previous exponential lerp never terminated,
    // so a "closed" box kept re-measuring its reflowing children for
    // many frames — the visible close→dip→bounce glitch.
    // style::anim_speed() still scales the duration (GUI slider).
    const float k_collapse_dur = 0.2f;   // seconds at anim_speed 1
    const float anim_rate = 14.0f * style::anim_speed();   // chevron tween
    {
        const float dir  = collapsed_ ? -1.0f : 1.0f;
        const float step = dt * style::anim_speed() / k_collapse_dur;
        anim_lin_ = std::clamp(anim_lin_ + dir * step, 0.0f, 1.0f);
        // ease_in_out_cubic — terminates exactly at 0/1.
        const float t = anim_lin_;
        arrow_anim_ = (t < 0.5f)
            ? 4.0f * t * t * t
            : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
    }

    chevron_rotation_ += (target_rotation_ - chevron_rotation_)
                       * std::min(1.0f, dt * anim_rate);

    auto p = parent_.lock();
    if (!p) return;

    const bool parent_is_column = p->type_name() == ", Column";
    if (!parent_is_column) {
        bounds_.x = p->bounds().x;
        bounds_.y = p->next_y();
        bounds_.w = p->bounds().w;
    }

    // Children content starts below the title row.
    next_y_ = bounds_.y + sz.control_h - 1.0f;

    // Conditional reveal: show only the master child until its predicate is
    // true, then reveal the rest. Done BEFORE the child update/sum so the
    // height collapses/grows with the visible set (no overlap). The cheat
    // menu's per-feature "Enabled" toggles use this (user 2026-06-11).
    if (reveal_pred_) {
        const bool on = reveal_pred_();
        const auto keep = reveal_master_.lock();
        if (!keep) {
            // No master row to keep — hide the ENTIRE groupbox when off
            // (used for a second box gated by another box's master, e.g.
            // the rage weapon "Accuracy" box behind "Enabled").
            visible_ = on;
        } else {
            for (auto& c : children_) {
                if (!c) continue;
                c->set_visible(c == keep ? true : on);
            }
        }
    }

    // Update children FIRST so their heights are current before we sum
    // them. Summing stale (last-frame / zero) heights made the box the
    // wrong size for ONE frame on first show, which read as a flicker
    // when switching tabs/strips (user 2026-06-10). Matches the binary,
    // which accumulates child heights AFTER iterating them.
    if (arrow_anim_ > 0.001f) {
        const float saved_x = bounds_.x;
        const float saved_w = bounds_.w;
        bounds_.x += sz.groupbox_padding;
        bounds_.w -= 2.0f * sz.groupbox_padding;

        for (auto& c : children_) {
            if (!c || !c->visible()) continue;
            // Repair parent links: children added inside a c_groupbox
            // SUBCLASS constructor (antiaim_groupbox_t, the visuals
            // composites) got an EMPTY parent — weak_from_this() is
            // empty during construction — so they never flowed from
            // next_y and rendered nowhere. Same fix as c_page::update.
            if (c->parent().expired()) c->set_parent(weak_from_this());
            c->update(dt);
        }

        bounds_.x = saved_x;
        bounds_.w = saved_w;
    }

    // Now sum the settled child heights scaling with visibility fraction
    float total_h = sz.control_h;

    for (auto& c : children_) {
        if (!c) continue;
        const float vf = c->visibility_fraction();
        if (vf > 0.0001f) {
            c->set_extra_height(sz.button_height);
            total_h += (c->bounds().h + c->extra_height()) * vf;
        }
    }

    // Bottom padding so bottom margin matches the top margin (~15px)
    total_h += (sz.control_h - sz.button_margin_y);

    if (total_h < sz.label_width) {
        total_h = sz.label_width;
    }

    // Freeze the expanded height while the collapse tween runs
    const bool tweening = anim_lin_ > 0.0f && anim_lin_ < 1.0f;
    if (!tweening) anim_total_h_ = total_h;

    const float collapsed_h = sz.label_width;
    const float expanded_h  = tweening ? anim_total_h_ : total_h;
    bounds_.h = collapsed_h + (expanded_h - collapsed_h) * arrow_anim_;

    if (!parent_is_column) {
        p->set_next_y(bounds_.y + bounds_.h + sz.control_gap_y);
    }

    // Collapse-button click. Binary writes this+348 as
    // {x + w - (label_width - 2) - 1, y + 1, label_width - 2, label_width - 2}.
    // Modal guard — popup/dialog must freeze the minimize button so
    // the user can't expand/collapse groupboxes through an open
    // colorpicker/dropdown/dialog (user 2026-06-02).
    if (is_interactive_ && !in.input_blocked_by_other(this)) {
        const float bs = sz.label_width - 2.0f;
        const float bx = bounds_.x + bounds_.w - bs - 1.0f;
        const float by = bounds_.y + 1.0f;
        Renderer::rect minbtn{bx, by, bs, bs};
        content_rect_ = {bx, by, bs, bs};
        hovered_ = minbtn.contains(in.mouse_pos);
        if (hovered_) {
            input::cursor().set(input::cursor_type::hand);
        }
        if (hovered_ && in.mouse_pressed[input::mb_left]) {
            collapsed_ = !collapsed_;
            // Spin clockwise on every click — binary stamps +328 / +336.
            target_rotation_ += 180.0f;
            in.consume_mouse_press();
        }
    } else {
        hovered_ = false;
    }

    const float hover_target = hovered_ ? 1.0f : 0.0f;
    chevron_hover_anim_ += (hover_target - chevron_hover_anim_) * std::clamp(dt * 14.0f * style::anim_speed(), 0.0f, 1.0f);
}

// BINARY: c_groupbox__render @ 0x140060700
// Colors from decompiled:
//   4280295460 = 0xFF202024 - header fill (when this+345 set)
//   4280492840 = 0xFF232328 - body fill
//   4292796126 = 0xFFDEDEDE - title text
//   4290690750 = 0xFFBEC4BE - chevron icon tint
//   render_outline uses default border color
void c_groupbox::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    const auto& sz = style::sizes();

    const float x = bounds_.x;
    const float y = bounds_.y;
    const float w = bounds_.w;
    const float h = bounds_.h;
    const float radius = style::rounding().of(style::rounding().groupbox);

    // Binary truth — `decompiled/c_groupbox__render.c:33, 35, 69-73`:
    //   vt[+288] render_filled_rect(bounds, *, 0xFF202024, rounded_flag)
    //   vt[+296] render_rect_outline(bounds)
    //   vt[+296] render_rect_outline({x+1, y+1, w-2, h-2})
    //
    // Body fill first, then two 1-px OUTLINE rings (outer dark, inner
    // light — the engine-default pair used everywhere else). The old
    // fill-inside-fill construction left the ring's inside edge soft:
    // the inner fill's alpha feather blended over the border ring.
    if (show_border_) {
        const Renderer::color body_col  {0x20, 0x20, 0x24, 0xFF};  // 0xFF202024
        const Renderer::color ring_outer{0x19, 0x19, 0x1D, 0xFF};  // 0xFF19191D
        const Renderer::color ring_inner{0x3E, 0x3E, 0x42, 0xFF};  // 0xFF3E3E42
        const float r0 = std::max(0.0f, radius);
        const float r1 = std::max(0.0f, radius - 1.0f);
        r.render_filled_rounded_rect(Renderer::rect{x, y, w, h},
                                     body_col, r0, 12);
        r.render_rounded_rect(Renderer::rect{x, y, w, h},
                              ring_outer, r0, 1.0f, 12);
        r.render_rounded_rect(Renderer::rect{x + 1.f, y + 1.f,
                                             w - 2.f, h - 2.f},
                              ring_inner, r1, 1.0f, 12);
    }

    // Compute title geometry up front so the border knows where to leave a
    // gap. If no title or no font, we draw an unbroken outline.
    float gap_x0 = 0.0f, gap_x1 = 0.0f;
    bool  has_gap = false;
    float text_x = 0.0f, text_y = 0.0f, text_w = 0.0f, font_h = 0.0f;
    if (show_title_ && title_font_) {
        text_w  = title_font_->measure_text(label_);
        font_h  = title_font_->pixel_height();
        const float text_pad = 6.0f;
        const float text_margin = sz.text_margin;
        const float max_title_w = w - 2.0f * text_margin - sz.label_width;
        text_x = (text_w <= max_title_w)
                   ? x + w * 0.5f - text_w * 0.5f
                   : x + text_margin;
        text_y = y - font_h * 0.5f;                // centered on top edge
        gap_x0 = text_x - text_pad;
        gap_x1 = text_x + text_w + text_pad;
        has_gap = true;
    }

    // Title cutout — paint over the 2 border pixels where the title sits
    // with the interior body color so the title "interrupts" the border
    // ring instead of overlapping it.
    if (show_border_ && has_gap) {
        const Renderer::color body{0x20, 0x20, 0x24, 0xFF};
        const float gx = std::max(x, gap_x0);
        const float gw = std::min(x + w, gap_x1) - gx;
        if (gw > 0.0f) {
            r.render_filled_rect({gx, y,         gw, 1.0f}, body); // outer
            r.render_filled_rect({gx, y + 1.0f,  gw, 1.0f}, body); // inner
        }
    }

    // Title text — drawn last so glyphs sit cleanly inside the gap.
    if (has_gap) {
        r.render_text(*title_font_, Renderer::vec2{text_x, text_y},
                      Renderer::color{0xDE, 0xDE, 0xDE, 0xFF}, label_.c_str());
    }

    // Minimize button — dumped `this+348` square at top-right.
    //   fill         : 0xFF232328 (35, 35, 40)
    //   left corners : rounded 1 px (right side sharp, sits flush against
    //                  the groupbox right border)
    //   icon         : triangle chevron — points DOWN when open (arrow_anim
    //                  > 0.5), points RIGHT when collapsed
    //   border       : drawn LAST so it stays on top and renders as a
    //                  complete square outline
    if (show_chevron_) {
        const float bs = sz.label_width - 2.0f;
        const float bx = x + w - bs - 1.0f;
        const float by = y + 1.0f;
        Renderer::rect btn{bx, by, bs, bs};

        const Renderer::color btn_fill{0x23, 0x23, 0x28, 0xFF};
        const Renderer::color def_border{0x3E, 0x3E, 0x42, 0xFF};
        const Renderer::color acc_border = style::current().accent;
        const Renderer::color btn_border = Renderer::color::lerp(
            def_border, acc_border, std::clamp(chevron_hover_anim_, 0.0f, 1.0f));
        const Renderer::color glyph{0xBE, 0xC4, 0xBE, 0xFF};

        // Diagonal-corner rounding: top-RIGHT and bottom-LEFT rounded,
        // top-LEFT and bottom-RIGHT sharp — now via the renderer's
        // per-corner primitives. The old draw-then-mask-wedges hack
        // bypassed snap_rect on the mask fills, so at fractional DPI
        // scales the wedges landed a pixel off the arcs (user 2026-07-17
        // "high rounding on high dpi but not always; changing dpi
        // fixes it").
        //
        // Top-right follows the groupbox radius CONCENTRICALLY: the
        // button sits 1 px inside the outer ring (bx+bs = x+w-1,
        // by = y+1), so radius-1 makes its arc hug the box corner and
        // the button stays flush with the outline at any Factor.
        // Bottom-left mirrors it (user 2026-07-17: "bottom left corner
        // isn't affected as much as top right") so the diagonal pair
        // reads symmetric. Radius 0 (toggle off) = fully square.
        const float br_tr = std::clamp(radius - 1.0f, 0.0f, bs * 0.5f);
        const float br_bl = br_tr;
        const float br_radii[4] = {0.0f, br_tr, 0.0f, br_bl}; // TL,TR,BR,BL

        r.render_filled_rounded_rect_ex(btn, btn_fill, br_radii, 12);

        // (b) Chevron — procedural rotated triangle. Binary uses a
        // rotated texture (`vt[+304]` with `g_texture_arrow`, see
        // `c_groupbox__render.c:356-362`) but my procedural V chevron
        // looked wrong to the user 2026-06-02, so we stay with the
        // triangle until the actual texture pixels can be extracted.
        // Per [[groupbox-chevron-anim]]:  0° = down (collapsed),
        //                                180° = up   (expanded).
        const float ax = bx + bs * 0.5f;
        const float ay = by + bs * 0.5f;
        {
            const float rad = chevron_rotation_ * 0.017453292f;
            const float c = std::cos(rad);
            const float s = std::sin(rad);
            auto rot = [&](float vx, float vy) {
                return Renderer::vec2{
                    ax + (vx * c - vy * s),
                    ay + (vx * s + vy * c)};
            };
            const Renderer::vec2 p1 = rot(-3.5f, -2.0f);
            const Renderer::vec2 p2 = rot( 3.5f, -2.0f);
            const Renderer::vec2 p3 = rot( 0.0f,  3.0f);
            r.render_filled_triangle(p1, p2, p3, glyph);
        }

        // (c) Border: same per-corner path as the fill, so the outline
        // and body edge coincide exactly — no masks, no sharp-L
        // overdraw sticking past the arcs.
        r.render_rounded_rect_ex(btn, btn_border, br_radii, 1.0f, 12);
    }

    // 7. Render children — keep drawing while arrow_anim_ > 0 so the
    // children visually shrink with the groupbox bounds during the
    // collapse tween. The clip starts BELOW the title bar so the first
    // child can never bleed up into the title area while shrinking,
    // and is cut off at the animated bottom edge so the last child
    // also clips correctly. User 2026-06-01: "minimizing is a bit
    // weird because when it goes up it takes a lot of time to stop
    // rendering first element at the top".
    if (arrow_anim_ > 0.001f) {
        const float body_top = bounds_.y + sz.control_h - 1.0f;
        const float body_bot = bounds_.y + bounds_.h;
        if (body_bot > body_top) {
            r.push_clip(Renderer::rect{
                bounds_.x, body_top,
                bounds_.w, body_bot - body_top});
            // Children render at their natural positions inside the
            // clip — never atomically removed.
            c_base_element::render(r);
            // Curtain edge over the children — two horizontal lines
            // matching the bottom border colours so the line sweeps with
            // the collapse animation. Drawn ONLY while the tween is
            // actually running: fully expanded, the real border rings
            // already draw the bottom edge, and the (unsnapped) curtain
            // rects sat 1 px wide/offset against the (snapped) rings —
            // the light inner line poking out left/right at the bottom
            // (user 2026-07-17; visible per-box depending on fractional
            // bounds, and whenever radius floored to 0 at Factor<0.51).
            // Inset floor of 1 keeps the ends inside both ring columns
            // even at radius 0; coords rounded to match snap_rect.
            if (arrow_anim_ < 0.999f) {
                const Renderer::color outer_col{0x19, 0x19, 0x1D, 0xFF};
                const Renderer::color inner_col{0x3E, 0x3E, 0x42, 0xFF};
                const float inset = std::max(1.0f, radius);
                const float x0 = std::round(bounds_.x + inset);
                const float x1 = std::round(bounds_.x + bounds_.w - inset);
                const float yb = std::round(body_bot);
                if (x1 > x0) {
                    r.render_filled_rect(
                        {x0, yb - 2.0f, x1 - x0, 1.0f}, inner_col);
                    r.render_filled_rect(
                        {x0, yb - 1.0f, x1 - x0, 1.0f}, outer_col);
                }
            }
            r.pop_clip();
        }
    }
}

// ============================================================================
// c_column
// ============================================================================

void c_column::update(float dt)
{
    if (columns_ < 1) columns_ = 1;
    const auto& sz = style::sizes();

    // Flow from the parent's next_y like c_groupbox does. c_page resets
    // every direct child's bounds to the page top (set_bounds(scrolled))
    // before update, so without this a column following direct groupbox
    // children laid out AT THE TOP and overlapped them (visible on every
    // Rage/Legit weapon page: "General" groupbox vs the 2-column block).
    if (auto p = parent_.lock()) {
        bounds_.y = p->next_y();
    }

    const float gap = sz.column_gap;
    const float col_w = (bounds_.w - gap * (columns_ - 1)) / columns_;

    std::vector<float> col_y(static_cast<std::size_t>(columns_), bounds_.y);
    int i = 0;
    for (auto& c : children_) {
        if (!c) { ++i; continue; }
        // Skip invisible children entirely — selection-gated groupboxes
        // (rage antiaim conditions, chams per-target boxes) must not
        // leave holes in the flow. The index still advances so the column
        // alternation of the static children stays stable.
        if (!c->visible()) { ++i; continue; }
        int col = i % columns_;
        if (auto it = col_override_.find(c.get()); it != col_override_.end())
            col = std::clamp(it->second, 0, columns_ - 1);
        const float h = c->bounds().h > 0 ? c->bounds().h : sz.control_h;
        c->set_bounds({
            bounds_.x + col * (col_w + gap),
            col_y[static_cast<std::size_t>(col)],
            col_w, h,
        });
        c->update(dt);
        col_y[static_cast<std::size_t>(col)] += c->bounds().h + sz.control_gap_y;
        ++i;
    }

    // Update self.bounds.h + parent.next_y to the tallest column so the
    // c_page chain can compute true content_height (otherwise overflow
    // stays 0 and scrolling is gated off).
    float max_y = bounds_.y;
    for (float y : col_y) if (y > max_y) max_y = y;
    bounds_.h = max_y - bounds_.y;
    if (auto p = parent_.lock()) {
        if (p->next_y() < max_y) p->set_next_y(max_y);
    }
}

void c_column::render(Renderer::c_renderer_core& r)
{
    c_base_element::render(r);
}

// ============================================================================
// c_scrollable_element
// ============================================================================

void c_scrollable_element::tick_scroll(float dt)
{
    const float k = std::min(1.0f, dt * smoothing_);
    scroll_y_ += (target_scroll_y_ - scroll_y_) * k;
    clamp_scroll();
}

void c_scrollable_element::clamp_scroll()
{
    const float max_scroll = std::max(0.0f, content_h_ - viewport_h_);
    scroll_y_ = std::clamp(scroll_y_, 0.0f, max_scroll);
    target_scroll_y_ = std::clamp(target_scroll_y_, 0.0f, max_scroll);
}

void c_scrollable_element::update_scrollbar()
{
    const auto& sz = style::sizes();

    scrollbar_visible_ = content_h_ > viewport_h_;
    if (!scrollbar_visible_) return;

    const float bar_w = sz.scrollbar_bar_w;
    const float pad = sz.scrollbar_pad;

    scrollbar_track_.x = bounds_.x + bounds_.w - bar_w - pad;
    scrollbar_track_.y = bounds_.y + pad;
    scrollbar_track_.w = bar_w;
    scrollbar_track_.h = bounds_.h - 2.0f * pad;

    const float ratio = viewport_h_ / content_h_;
    const float thumb_h = std::max(sz.scrollbar_min_h, scrollbar_track_.h * ratio);
    const float scroll_ratio = scroll_y_ / (content_h_ - viewport_h_);
    const float thumb_y = scrollbar_track_.y + scroll_ratio * (scrollbar_track_.h - thumb_h);

    scrollbar_thumb_.x = scrollbar_track_.x;
    scrollbar_thumb_.y = thumb_y;
    scrollbar_thumb_.w = bar_w;
    scrollbar_thumb_.h = thumb_h;
}

// ============================================================================
// c_body
// ============================================================================

void c_body::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;

    // Background - 0xFF232328
    r.render_filled_rect(Renderer::rect{0, 0,
                                        static_cast<float>(globals::g_screen_w),
                                        static_cast<float>(globals::g_screen_h)},
                         Renderer::color{0x23, 0x23, 0x28, 0xFF});
    c_base_element::render(r);
}

} // namespace GUI
