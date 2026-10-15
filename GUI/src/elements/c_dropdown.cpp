// =============================================================================
// c_dropdown.cpp — closed / open states + option list hit-testing
//
// Binary verified — HOW_IT_WORKS.md §18:
//   c_dropdown render: 0x140046520
//   c_dropdown update: 0x1400461A0
// =============================================================================

#include <GUI/elements/c_dropdown.hpp>

#include <GUI/input/cursor.hpp>
#include <GUI/input/input_state.hpp>
#include <GUI/style/colorscheme.hpp>
#include <GUI/style/element_sizes.hpp>
#include <GUI/style/rounding.hpp>
#include <GUI/style/textures.hpp>

#include <Renderer/c_renderer_core.hpp>
#include <Renderer/font.hpp>

#include <algorithm>
#include <cmath>

namespace GUI {

namespace {
inline Renderer::rect rrect(c_base_element::rect_t r) {
    return Renderer::rect{r.x, r.y, r.w, r.h};
}
} // anon

// BINARY: c_dropdown__update @ 0x1400461A0
// Lays out from parent->next_y, then advances by bounds.h + extra_height.
// The box rect is the fixed 24px region below the label row.
// Right-click clears the value
void c_dropdown::update(float dt)
{
    auto& in = input::current();

    // Animation matched to c_groupbox (structural.cpp:51-72) — the user
    // wants the same feel. Open/close is a TIME-BASED linear progress over
    // ~200 ms (scaled by the GUI anim_speed slider) run through
    // ease_in_out_cubic, which terminates EXACTLY at 0/1. The old
    // exponential `lerp toward target` accelerated then crawled forever
    // (never reaching its target) and its speed varied with frame-rate —
    // that's the "too slow and fast" the user reported.
    const float anim_rate = 14.0f * style::anim_speed();   // chevron spin
    chevron_rotation_ += (target_rotation_ - chevron_rotation_)
                       * std::min(1.0f, dt * anim_rate);

    {
        const float k_dur = 0.2f;   // seconds at anim_speed 1
        const float step  = dt * style::anim_speed() / k_dur;
        open_lin_ = std::clamp(open_lin_ + (open_ ? 1.0f : -1.0f) * step,
                               0.0f, 1.0f);
        const float t = open_lin_;
        open_anim_ = (t < 0.5f)
            ? 4.0f * t * t * t
            : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
    }

    // Hover stays a cheap exponential colour fade — it's a small border
    // tint, not a geometry reveal, so the easing nicety doesn't matter.
    const float hover_target = hovered_ ? 1.0f : 0.0f;
    hover_anim_ += (hover_target - hover_anim_) * std::min(1.0f, dt * anim_rate);
    const auto& sz = style::sizes();
    auto p = parent_.lock();

    if (p) {
        const float parent_next_y = p->next_y();
        if (inline_layout_) {
            bounds_.x = p->bounds().x;
            bounds_.y = parent_next_y;
            bounds_.w = p->bounds().w;
            bounds_.h = sz.dropdown_extra_h + sz.button_height;
        } else {
            bounds_.x = p->bounds().x + sz.text_margin;
            bounds_.y = parent_next_y;
            bounds_.w = p->bounds().w - 2.0f * sz.text_margin;
            bounds_.h = sz.dropdown_extra_h + sz.button_height + sz.checkbox_height;
        }

        extra_height_ = visible_ ? sz.button_height : 0.0f;
        p->set_next_y(parent_next_y + bounds_.h + extra_height_);
    }

    // Row height matches dropdown box height for clean vertical spacing
    const float row_h = sz.dropdown_extra_h;  // 24.0f

    // Dropdown box rect — `decompiled/c_dropdown__update.c:125-131`:
    //   v13 = button_height + checkbox_height + this+100; box.y = v13
    // The `checkbox_height` term is the LABEL ROW that sits above the
    // box. When the dropdown is inline (no label row), the box should
    // sit at bounds.y directly — otherwise it floats `checkbox_height`
    // below where it belongs, which is exactly what user 2026-06-02
    // reported for ESP-row dropdowns ("selection is under not on same
    // line").
    dropdown_box_rect_.x = bounds_.x;
    dropdown_box_rect_.y = inline_layout_
        ? bounds_.y
        : bounds_.y + sz.button_height + sz.checkbox_height;
    dropdown_box_rect_.w = bounds_.w;
    dropdown_box_rect_.h = sz.dropdown_extra_h;

    const Renderer::rect box = rrect(dropdown_box_rect_);

    // User 2026-06-01: a trigger click steals modal from a peer popup
    // (dropdown→colorpicker→keybinder shuffle). User 2026-06-02:
    // when ANY peer (incl. dialog) owns the modal, freeze this
    // dropdown completely — no hover accent, no click. Outside-click
    // dismiss is handled by the modal owner itself.
    const bool blocked = in.input_blocked_by_other(this);
    if (blocked) {
        hovered_ = false;
        if (open_ && in.modal_owner != this) {
            open_ = false;
            target_rotation_ += 180.0f;
        }
        return;
    }

    // The popup shows at most k_max_visible_rows rows; longer lists
    // wheel-scroll (user 2026-07-17).
    const float content_h = row_h * options_.size();
    const float visible_h = row_h * std::min<std::size_t>(
        k_max_visible_rows, options_.size());
    const float max_scroll = std::max(0.0f, content_h - visible_h);

    // Trigger click — toggle popup, steal the modal. Uses
    // mouse_pressed_raw so a press that the previous modal owner
    // already consumed (close-on-outside) is still seen here.
    if (box.contains(in.mouse_pos) && in.mouse_pressed_raw[input::mb_left]) {
        open_ = !open_;
        target_rotation_ += 180.0f;   // chevron spin on every click
        if (open_) {
            // Displaced-by-peer close must ALSO spin the chevron — without
            // the rotation bump the arrow stayed pointing up after another
            // popup stole the modal.
            in.claim_modal(this, [this]{ open_ = false; target_rotation_ += 180.0f; });
            popup_rect_.x = box.x;
            popup_rect_.y = box.y + box.h;
            popup_rect_.w = box.w;
            popup_rect_.h = visible_h;
            // Open scrolled so the current selection is in view.
            const int sel_now = get();
            dd_target_scroll_y_ = (!multi_select_ && sel_now >= 0)
                ? std::clamp(sel_now * row_h - 2.0f * row_h, 0.0f, max_scroll)
                : 0.0f;
            dd_scroll_y_ = dd_target_scroll_y_;
        } else {
            in.release_modal(this);
        }
        in.consume_mouse_press();
        hovered_ = box.contains(in.mouse_pos);
        return;
    }

    // If we were open but lost the modal to a peer, close.
    if (open_ && in.modal_owner != this) {
        open_ = false;
        target_rotation_ += 180.0f;
    }

    if (blocked) { hovered_ = false; return; }

    hovered_ = box.contains(in.mouse_pos);
    if (hovered_) {
        input::cursor().set(input::cursor_type::hand);
    }

    if (open_) {
        // Refresh claim each frame so the close callback stays bound.
        // Displaced-by-peer close must ALSO spin the chevron — without
        // the rotation bump the arrow stayed pointing up after another
        // popup stole the modal.
        in.claim_modal(this, [this]{ open_ = false; target_rotation_ += 180.0f; });
        popup_rect_.x = box.x;
        popup_rect_.y = box.y + box.h;
        popup_rect_.w = box.w;
        popup_rect_.h = visible_h;

        // Wheel scroll (1 notch = 2 rows) + smooth follow.
        const Renderer::rect popup_hit = rrect(popup_rect_);
        if (max_scroll > 0.0f && in.wheel_delta != 0.0f &&
            (popup_hit.contains(in.mouse_pos) || box.contains(in.mouse_pos))) {
            dd_target_scroll_y_ = std::clamp(
                dd_target_scroll_y_ - in.wheel_delta * 2.0f * row_h,
                0.0f, max_scroll);
        }
        dd_scroll_y_ += (dd_target_scroll_y_ - dd_scroll_y_)
                      * std::min(1.0f, dt * 20.0f);

        // Hover index — rows offset by the scroll, hit only inside the
        // visible window.
        hover_idx_ = -1;
        const float list_y = popup_rect_.y - dd_scroll_y_;
        for (std::size_t i = 0; i < options_.size(); ++i) {
            Renderer::rect row{box.x, list_y + i * row_h, box.w, row_h};
            if (popup_hit.contains(in.mouse_pos) && row.contains(in.mouse_pos)) {
                hover_idx_ = static_cast<int>(i);
                break;
            }
        }
        // Hand cursor when hovering any popup row (matches listbox).
        if (hover_idx_ >= 0) {
            input::cursor().set(input::cursor_type::hand);
        }
        // Tick per-row hover anim toward its target (1 for hovered,
        // 0 for everything else). House rate 14/s — the previous 22/s
        // read as near-instant (user 2026-06-09: hover should be a bit
        // slower to be smoother).
        if (row_anim_.size() != options_.size()) {
            row_anim_.assign(options_.size(), 0.0f);
        }
        const float row_rate = 14.0f * style::anim_speed();
        for (std::size_t i = 0; i < row_anim_.size(); ++i) {
            const float target = (static_cast<int>(i) == hover_idx_) ? 1.0f : 0.0f;
            row_anim_[i] += (target - row_anim_[i]) * std::min(1.0f, dt * row_rate);
        }
    } else {
        in.release_modal(this);
        for (auto& v : row_anim_) v = 0.0f;
    }

    // Right-click clears value (binary: c_dropdown__on_click)
    if (in.mouse_pressed[input::mb_right] && hovered_) {
        set(-1);
        in.consume_mouse_press();
        return;
    }

    // Left-click handling for an already-open popup (option click or
    // click outside). Trigger-click case handled above.
    if (in.mouse_pressed[input::mb_left] && open_) {
        if (hover_idx_ >= 0) {
            if (multi_select_) {
                toggle_multi(hover_idx_);
                in.consume_mouse_press();
            } else {
                set(hover_idx_);
                open_ = false;
                target_rotation_ += 180.0f;
                in.release_modal(this);
                in.consume_mouse_press();
                return;
            }
        } else {
            // Click outside the popup AND outside the trigger — close
            // AND consume so no regular widget (checkbox/slider) under
            // the click reacts. Peer popup-triggers read
            // mouse_pressed_raw so they still see the press.
            open_ = false;
            target_rotation_ += 180.0f;
            in.release_modal(this);
            in.consume_mouse_press();
        }
    }
}

// BINARY: c_dropdown__render @ 0x140046520
// 1. Label text (if not hidden):
//    height = style.checkbox_height (NOT dropdown_height)
//    text_y = bounds.y + checkbox_height / 2
//    vt[+472] render_text(left-aligned, 0xFFBEBEBE)
// 2. Dropdown box body — SAME color as window body:
//    rounded_flag = style.dropdown_rounding ? 6 : 0
//    vt[+288] render_filled_rect(this+344, 0xFF232328, rounded_flag)
// 3. Debug outline (if g_state_textbox_focus_ptr):
//    vt[+264] push_clip(this+344, 1)
// 4. Build selected-items display string
// 5. Text width check: truncate if exceeds (this+352 - qword_1402461F8)
// 6. Chevron arrow on the right:
//    vt[+304] render_textured_rect(arrow_rect, g_texture_chevron, ...)
// 7. If popup open (overlay queue):
//    panel_h = item_count * row_h + style.dropdown_extra_h (24)
//    Each item row: hover → accent_color fill, else 0xFF232328
//    Item text: 0xFFBEBEBE, ALIGN_LEFT
void c_dropdown::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    const auto& cs = style::current();
    const auto& sz = style::sizes();
    const float radius = style::rounding().of(style::rounding().dropdown);
    const auto br = rrect(bounds_);
    const float row_h = sz.checkbox_height;

    // 1. Label — 0xFFBEBEBE
    if (font_) {
        const float label_y = br.y + (row_h - font_->pixel_height()) * 0.5f;
        r.render_text(*font_, Renderer::vec2{br.x, label_y}, cs.text, label_.c_str());
    }

    // 2. Dropdown box — 0xFF232328 (same as window body).
    // User 2026-06-01: the border NEVER wraps the label. When CLOSED,
    // only the box itself gets an outline (accent on hover, outline
    // otherwise). When OPEN, the box border is suppressed here and
    // replaced after the popup draws by a single continuous accent
    // border around (box + popup) — drawn at the end of render().
    // User 2026-06-02: render via TWO concentric filled rounded rects
    // instead of fill + outline. At small radii the outline path's
    // 6-segment arc strips rasterise asymmetrically — visible as
    // "rounding not the same on right vs left side". Concentric fills
    // share their arc geometry so left and right corners are always
    // identical pixel-for-pixel.
    // Binary `c_dropdown__render.c:130, 484-510`:
    //   vt[+288] render_filled_rect(dropdown_box, *, 0xFF232328, rounded)
    //   anim_blend_colors_multi(hovered, open,
    //                           0xFF3E3E42, accent|0x80, accent|0xFF, _, anim)
    //   vt[+296] render_rect_outline(dropdown_box-or-combined)
    //
    // 3-stop lerp driven by hover_anim_ and open_anim_ (set in update()).
    // When open, the outline rect is the combined (box + popup) so we
    // defer the outline render until after the popup. Body fill still
    // happens here for both states.
    const Renderer::rect box = rrect(dropdown_box_rect_);

    // While the popup is visible (incl. the close tween) the box joins
    // the list below it, so its BOTTOM corners must be square — the
    // popup continues the shape and carries the rounded bottom (user
    // 2026-07-17 "the selected one is rounded at the bottom when
    // dropped down when it shouldn't").
    const bool popup_visible = open_anim_ > 0.003f && !options_.empty();
    const float box_radii[4] = {radius, radius,
                                popup_visible ? 0.0f : radius,
                                popup_visible ? 0.0f : radius};
    r.render_filled_rounded_rect_ex(box, cs.body_bg, box_radii);

    // Border colour = 3-stop blend: outline → accent@0x80 → accent_solid
    const Renderer::color hover_col{cs.accent.r, cs.accent.g, cs.accent.b, 0x80};
    Renderer::color border = Renderer::color::lerp(cs.outline, hover_col, hover_anim_);
    border = Renderer::color::lerp(border, cs.accent, open_anim_);

    // Standalone box outline only while NO popup chrome is on screen —
    // during the close tween the combined outline below still owns the
    // border, and drawing both put a rounded bottom edge on top of the
    // square-joined box.
    if (!popup_visible) {
        r.render_rounded_rect(box, border, radius, 1.0f);
    }
    // When open: the (box + popup) outline is drawn after the popup
    // overlay below, using `hover_anim_`/`open_anim_` re-blended there.

    // 3. Selected option text — multi-select shows a comma-joined list
    // of the picked options. ORIGINAL shows "-" when nothing is picked
    // (orig-now-2 capture) and never lets the text escape the box: clip
    // to the box interior minus the chevron area (user 2026-06-09: the
    // selected items rendered outside the closed dropdown).
    const int sel = get();
    if (font_ && !options_.empty()) {
        const float baseline = box.y + (box.h - font_->pixel_height()) * 0.5f;
        std::string display;
        if (multi_select_) {
            for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
                if (!is_selected_multi(i)) continue;
                if (!display.empty()) display += ", ";
                display += options_[i];
            }
            if (display.empty()) display = "-";
        } else if (sel >= 0 && sel < static_cast<int>(options_.size())) {
            display = options_[sel];
        } else {
            display = "-";
        }
        if (!display.empty()) {
            const float text_clip_w = std::max(0.0f, box.w - 8.0f - 24.0f);
            r.push_clip(Renderer::rect{box.x + 8, box.y, text_clip_w, box.h});
            r.render_text(*font_, Renderer::vec2{box.x + 8, baseline},
                          cs.text, display.c_str());
            r.pop_clip();
        }
    }

    // 4. Chevron — procedural rotated triangle. Binary uses a rotated
    // texture (`vt[+304]` with `g_texture_arrow`, see
    // `c_dropdown__render.c:430-436`) but the procedural V chevron we
    // tried looked wrong to the user 2026-06-02. Stay with the
    // triangle until we can extract the actual texture pixels.
    const float cx = box.x + box.w - 12.0f;
    const float cy = box.y + box.h * 0.5f;
    {
        const float rad = chevron_rotation_ * 0.017453292f;
        const float ca = std::cos(rad);
        const float sa = std::sin(rad);
        auto rot = [&](float vx, float vy) {
            return Renderer::vec2{cx + (vx * ca - vy * sa),
                                  cy + (vx * sa + vy * ca)};
        };
        r.render_filled_triangle(rot(-4.0f, -2.0f), rot(4.0f, -2.0f),
                                 rot(0.0f, 3.0f), cs.text_dim);
    }

    // 5. Open popup (overlay layer) — animated roll-down / roll-up reveal.
    //
    // Gated on open_anim_ (not open_) so the CLOSE animation plays out:
    // open_ flips false the instant the user clicks away, but open_anim_
    // keeps decaying toward 0 for a few frames. Rendering while it's
    // still non-zero lets the list retract instead of vanishing. On open
    // it grows from 0, so the list slides out from under the box.
    //
    // popup_rect_ holds the FULL-height geometry (set in update while
    // open_). We clip every popup primitive to popup.h * open_anim_ so
    // partial rows are cut cleanly at the animated edge.
    if (popup_visible) {
        r.push_layer(Renderer::layer_e::overlay);

        // Geometry rebuilt from the CURRENT box every frame
        const float row_h = sz.dropdown_extra_h; // 24.0f
        const float vis_rows_h = row_h * std::min<std::size_t>(
            k_max_visible_rows, options_.size());
        const float full_h = row_h * options_.size();
        const Renderer::rect popup{box.x, box.y + box.h, box.w, vis_rows_h};
        const float anim_h = std::max(0.0f, popup.h * open_anim_);

        // Clip the whole list to the animated height (the reveal mask).
        r.push_clip(Renderer::rect{popup.x, popup.y, popup.w, anim_h});

        // Popup rows mirror the LISTBOX colour scheme
        const Renderer::color row_bg_idle {0x19, 0x19, 0x1D, 0xFF};
        const Renderer::color row_bg_hover{0x23, 0x23, 0x28, 0xFF};
        const float list_radii[4] = {0.0f, 0.0f, radius, radius};
        r.render_filled_rounded_rect_ex(popup, row_bg_idle, list_radii);

        const float list_y = popup.y - dd_scroll_y_;
        for (std::size_t i = 0; i < options_.size(); ++i) {
            const bool is_sel = multi_select_
                ? is_selected_multi(static_cast<int>(i))
                : (static_cast<int>(i) == sel);
            const bool is_hov = (static_cast<int>(i) == hover_idx_);

            const Renderer::rect row{popup.x, list_y + i * row_h,
                                     popup.w, row_h};
            // Scrolled out of the visible window — nothing to draw.
            if (row.y + row.h <= popup.y || row.y >= popup.y + anim_h)
                continue;
            // Smoothly fade the hover bg with the per-row anim.
            const float ra = (i < row_anim_.size()) ? row_anim_[i]
                                                    : (is_hov ? 1.0f : 0.0f);
            if (ra > 0.001f) {
                const std::uint8_t a = static_cast<std::uint8_t>(
                    std::clamp(ra, 0.0f, 1.0f) * 0xFF);
                const Renderer::color hc{
                    row_bg_hover.r, row_bg_hover.g, row_bg_hover.b, a};
                const bool reaches_bottom = (row.y + row.h >= popup.y + popup.h - 1.0f);
                if (reaches_bottom && radius > 0.0f) {
                    const float row_radii[4] = {0.0f, 0.0f, radius, radius};
                    r.render_filled_rounded_rect_ex(row, hc, row_radii);
                } else {
                    r.render_filled_rect(row, hc);
                }
            }
            (void)is_hov;

            if (font_) {
                const float baseline = (list_y + i * row_h)
                                     + (row_h - font_->pixel_height()) * 0.5f;
                Renderer::color tc = cs.text;
                if (is_sel)      tc = cs.accent;
                else if (is_hov) tc = cs.title_text;
                r.render_text(*font_,
                              Renderer::vec2{popup.x + 14, baseline},
                              tc, options_[i].c_str());
            }
        }

        // Scrollbar pill — only when the list overflows the window.
        if (full_h > popup.h + 0.5f) {
            const float pad = 2.0f;
            const float track_h = popup.h - 2.0f * pad;
            const float th = std::max(10.0f, track_h * popup.h / full_h);
            const float max_s = full_h - popup.h;
            const float sr = std::clamp(dd_scroll_y_ / max_s, 0.0f, 1.0f);
            const float ty = popup.y + pad + sr * (track_h - th);
            r.render_filled_rounded_rect(
                Renderer::rect{popup.x + popup.w - 4.0f, ty, 2.0f, th},
                Renderer::color{0x3E, 0x3E, 0x42, 0xFF}, 1.0f);
        }

        // Done with the clipped list region.
        r.pop_clip();

        // Binary `c_dropdown__render.c:458-475` — 1-px separator line
        // between the box and the popup. Drawn at `box.bottom - 1` so
        // it sits INSIDE the combined outline, dividing the chrome into
        // a "header" (closed-state look) on top and a list area below.
        // Ends inset past the corner arcs and alpha tied to the tween —
        // full-width it overlapped the combined outline's rounded
        // corners in the last frames of the close animation (user
        // 2026-07-17 "the separating line renders on the rounded part").
        const std::uint8_t sep_a = static_cast<std::uint8_t>(
            std::clamp(open_anim_, 0.0f, 1.0f) * 0xFF);
        const Renderer::color sep_col{0x3E, 0x3E, 0x42, sep_a};  // 0xFF3E3E42
        // Fully open the joint sits between straight side walls, so the
        // line spans the full interior (user 2026-07-17: the inset made
        // it read as rounded while open). The bottom arcs only reach
        // the joint once the remaining popup height drops under the
        // radius — grow the inset just for those closing frames.
        const float sep_inset = std::max(1.0f, radius - anim_h);
        const float sep_w = box.w - 2.0f * sep_inset;
        if (sep_w > 0.0f && anim_h > 1.0f) {
            r.render_filled_rect(
                Renderer::rect{box.x + sep_inset, box.y + box.h - 1.0f,
                               sep_w, 1.0f},
                sep_col);
        }

        // Outer outline around (box + animated popup) — same border colour
        // the closed state would have used. Computed from hover_anim_ /
        // open_anim_ so it tweens smoothly into accent_solid as the
        // dropdown opens (matches binary `anim_blend_colors_multi`). The
        // bottom edge follows the animated height so the outline grows and
        // shrinks WITH the revealed list rather than snapping to full size.
        const Renderer::color hov{cs.accent.r, cs.accent.g, cs.accent.b, 0x80};
        Renderer::color outline = Renderer::color::lerp(cs.outline, hov, hover_anim_);
        outline = Renderer::color::lerp(outline, cs.accent, open_anim_);

        const Renderer::rect combined{
            box.x, box.y,
            std::max(box.w, popup.w),
            (popup.y + anim_h) - box.y,
        };
        r.render_rounded_rect(combined, outline, radius, 1.0f);

        r.pop_layer();
    }
}

} // namespace GUI
