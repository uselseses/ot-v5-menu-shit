// =============================================================================
// simple_controls.cpp — c_label / c_separator / c_button / c_button_grid / c_checkbox
//
// All implementations verified against binary decompiles 2026-05-31.
// See HOW_IT_WORKS.md §14-17, §25 for exact render sequences.
// =============================================================================

#include <GUI/elements/c_label.hpp>
#include <GUI/elements/c_separator.hpp>
#include <GUI/elements/c_button.hpp>
#include <GUI/elements/c_button_grid.hpp>
#include <GUI/elements/c_checkbox.hpp>
#include <GUI/elements/c_colorpicker.hpp>

#include <GUI/core/layout_context.hpp>
#include <GUI/input/cursor.hpp>
#include <GUI/input/input_state.hpp>
#include <GUI/style/colorscheme.hpp>
#include <GUI/style/element_sizes.hpp>
#include <GUI/style/rounding.hpp>

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

// ---------------------------------------------------------------- c_label
// BINARY: c_label__render @ 0x1400621A0
// Sibling adornment handling: inspects children's element_flags to detect
// which adornments (checkbox, dropdown, keybinder, colorpicker) are present.
void c_label::update(float dt)
{
    const auto& sz = style::sizes();

    // Smooth visibility fade / collapse animation
    const float rate = 14.0f * style::anim_speed();
    const float target = target_visible_ ? 1.0f : 0.0f;
    anim_visibility_ += (target - anim_visibility_) * std::min(1.0f, dt * rate);
    if (!target_visible_ && anim_visibility_ <= 0.001f) {
        visible_ = false;
        anim_visibility_ = 0.0f;
    } else {
        visible_ = true;
    }

    if (!inline_layout_) {
        auto p = parent_.lock();
        if (p) {
            bounds_.x = p->bounds().x + sz.text_margin;
            bounds_.y = p->next_y();
            bounds_.w = p->bounds().w - 2.0f * sz.text_margin;
            bounds_.h = sz.checkbox_height;
            extra_height_ = sz.button_height;
            // Binary advances by bounds.h + button_height when visible.
            p->set_next_y(p->next_y() + bounds_.h + extra_height_);
        }
    }

    // Reset adornment state
    has_checkbox_left_ = false;
    has_dropdown_ = false;
    has_keybinder_ = false;
    adornment_count_ = 0;

    // Walk children to detect adornments
    for (auto& c : children_) {
        if (!c) continue;
        const auto flags = c->element_flags();

        if (flags & ef_keybinder) {
            has_keybinder_ = true;
        }
        if (flags & ef_colorpicker) {
            ++adornment_count_;
        }
        if (flags & ef_dropdown) {
            has_dropdown_ = true;
        }
        if (flags & ef_checkbox) {
            has_checkbox_left_ = true;
        }
    }

    // Cache label rect from bounds
    label_rect_ = bounds_;
    label_rect_.h = sz.checkbox_height;

    // Per-adornment-count shrink
    if (adornment_count_ > 0) {
        label_rect_.w -= adornment_count_ * (sz.checkbox_size + sz.label_offset);
    }

    // Checkbox-left adjustment
    if (has_checkbox_left_) {
        label_rect_.w -= sz.label_spacing;
        label_rect_.w -= sz.label_offset;
    }
}

// c_label__render @ 0x1400621A0 — faithful port.
void c_label::render(Renderer::c_renderer_core& r)
{
    if (!visible_ || !font_ || anim_visibility_ <= 0.001f) return;
    const auto& cs = style::current();
    const auto& sz = style::sizes();

    const float swatch_w     = sz.checkbox_size;       // 12 px
    const float label_offset = sz.label_offset;        // gap between adornments

    // First pass: detect adornments + write per-child slot rects.
    int colorpicker_idx = 0;     // 0 = rightmost; grows leftward
    bool has_checkbox_left = false;
    bool has_dropdown      = false;
    bool has_keybinder     = false;

    for (auto& c : children_) {
        if (!c) continue;
        const auto flags = c->element_flags();

        if (flags & ef_keybinder) {
            has_keybinder = true;
            continue;
        }
        if (flags & ef_colorpicker) {
            const float swatch_x =
                bounds_.x + bounds_.w
                - static_cast<float>(colorpicker_idx) * (swatch_w + label_offset)
                - swatch_w;
            const float swatch_y =
                bounds_.y + bounds_.h * 0.5f - swatch_w * 0.5f;
            if (auto cp = std::dynamic_pointer_cast<c_colorpicker>(c)) {
                cp->set_swatch_rect(Renderer::rect{
                    swatch_x, swatch_y, swatch_w, swatch_w});
            }
            ++colorpicker_idx;
            continue;
        }
        if (flags & ef_dropdown) {
            has_dropdown = true;
            continue;
        }
        if (flags & ef_checkbox) {
            has_checkbox_left = true;
        }
    }

    // Text rect = full bounds, shrunk for every adornment.
    Renderer::rect text_rect{
        bounds_.x, bounds_.y, bounds_.w, bounds_.h
    };
    if (colorpicker_idx > 0) {
        text_rect.w -=
            colorpicker_idx * (swatch_w + label_offset);
    }
    // Shift text right past the checkbox slot when has_checkbox_left.
    if (has_checkbox_left) {
        const float keybinder_offset = swatch_w + label_offset;  // 20 px
        text_rect.x += keybinder_offset;
        text_rect.w -= keybinder_offset;
    }
    if (has_keybinder) {
        const float keybinder_offset = swatch_w + label_offset;
        text_rect.x += keybinder_offset;
        text_rect.w -= keybinder_offset;
    }

    for (auto& c : children_) {
        if (!c) continue;
        const auto flags = c->element_flags();
        if (flags & ef_checkbox) {
            c->set_bounds(rect_t{
                bounds_.x, bounds_.y, swatch_w, bounds_.h});
        }
    }

    // Render text — vertically centred with opacity fade and subtle slide
    Renderer::color tc = cs.text;
    tc.a = static_cast<std::uint8_t>(tc.a * std::clamp(anim_visibility_, 0.0f, 1.0f));

    const float y_glide = (1.0f - anim_visibility_) * -4.0f;
    const float baseline_y =
        bounds_.y + (sz.checkbox_height - font_->pixel_height()) * 0.5f + y_glide;
    r.render_text(*font_, Renderer::vec2{text_rect.x, baseline_y},
                  tc, label_.c_str());

    // Walk children render — the adornment children render themselves
    // at the rects we just stamped.
    c_base_element::render(r);
    (void)has_dropdown;
}

// ---------------------------------------------------------------- c_separator
// EXACT from c_separator__render @ 0x14005FF30:
//   1. Dark line (0xFF353538) at y = bounds.y + bounds.h/2
//   2. Highlight line (0xFF3E3E42) at y - 1
// Both are 1 pixel high, full width of bounds.
void c_separator::update(float /*dt*/)
{
    const auto& sz = style::sizes();
    if (auto p = parent_.lock()) {
        const float parent_next_y = p->next_y();
        // Match the checkbox/button advance pattern so the separator
        // visually allocates the same vertical slot as a normal row.
        // Without this the next widget rendered right on top of the
        // separator lines — the user-visible "doesn't work properly"
        // bug. The 2-px line pair is drawn centred in this band.
        bounds_.x = p->bounds().x + sz.text_margin;
        bounds_.y = parent_next_y;
        bounds_.w = p->bounds().w - 2.0f * sz.text_margin;
        bounds_.h = sz.checkbox_height * 0.5f;   // ~10 px at 1.0x scale
        extra_height_ = sz.button_height;        // ~6 px (same as other rows)
        p->set_next_y(parent_next_y + bounds_.h + extra_height_);
    }
}

void c_separator::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    const float y = bounds_.y + bounds_.h * 0.5f;
    // Binary literal 4281677112 = 0xFF353538
    const Renderer::color dark     {0x35, 0x35, 0x38, 0xFF};
    // Binary literal 4282269250 = 0xFF3E3E42
    const Renderer::color highlight{0x3E, 0x3E, 0x42, 0xFF};
    r.render_filled_rect(Renderer::rect{bounds_.x, y, bounds_.w, 1.0f}, dark);
    r.render_filled_rect(Renderer::rect{bounds_.x, y - 1.0f, bounds_.w, 1.0f}, highlight);
}

// ---------------------------------------------------------------- c_button
// BINARY: c_button__update @ 0x14003E390
// Layout: if NOT inline/sibling mode AND NOT (parent.flags & 0x800000),
// reads parent.bounds.x, parent.next_y, parent.bounds.w.
void c_button::update(float dt)
{
    auto& in = input::current();
    auto& cur = input::cursor();
    const auto& sz = style::sizes();

    // Modal guard — same shape as c_dropdown / c_colorpicker. Layout
    // still needs to run so the button keeps its slot in the page
    // flow; we only suppress hover/click reactions.
    // `bypass_modal_` lets dialog-owned Dismiss/Confirm buttons keep
    // working even though the dialog itself is the modal owner.
    const bool input_blocked = !bypass_modal_ && in.input_blocked_by_other(this);

    if (!inline_layout_) {
        auto p = parent_.lock();
        bool parent_blocked = p && (p->element_flags() & ef_blocked);

        if (p && !parent_blocked) {
            const float parent_next_y = p->next_y();
            bounds_.x = sz.text_margin + p->bounds().x;
            bounds_.y = sz.button_margin_y + parent_next_y;
            bounds_.w = p->bounds().w - 2.0f * sz.text_margin;
            bounds_.h = sz.button_h;

            extra_height_ = visible_ ? sz.button_height : 0.0f;
            p->set_next_y(parent_next_y + bounds_.h + extra_height_);
        }
    }

    const Renderer::rect br = rrect(bounds_);
    if (input_blocked) {
        // Snap hover/press anims to 0 instantly so the button reads
        // as completely inert while a popup/dialog is up — the user
        // 2026-06-02 spec: "not to be able to hover to change color
        // of them, not to be able to click on them".
        hovered_ = false;
        pressed_ = false;
        hover_anim_ = 0.0f;
        press_anim_ = 0.0f;
        return;
    }

    hovered_ = br.contains(in.mouse_pos);
    if (hovered_) {
        cur.set(input::cursor_type::hand);
    }

    if (hovered_ && in.mouse_pressed[input::mb_left]) {
        pressed_ = true;
        in.consume_mouse_press();
    }
    if (in.mouse_released[input::mb_left]) {
        if (pressed_ && hovered_) {
            if (on_click_) on_click_();
            fire_on_change();
        }
        pressed_ = false;
    }

    const float hover_target = hovered_ ? 1.0f : 0.0f;
    const float press_target = pressed_ ? 1.0f : 0.0f;
    hover_anim_ += (hover_target - hover_anim_) * std::clamp(dt * 14.0f, 0.0f, 1.0f);
    press_anim_ += (press_target - press_anim_) * std::clamp(dt * 18.0f, 0.0f, 1.0f);
}

// BINARY: c_button__render @ 0x14003E5B0
// Verified 2026-05-31 via decompile:
//   * Body fill: render_filled_rect_fn(..., 0xFF232328, rounded_flag)
//   * Border via anim_blend_colors_multi(has_focus, is_pressed,
//       0xFF3E3E42, accent | 0x80000000, accent | 0xFF000000, ...)
//     → idle = 0xFF3E3E42, hover = accent@128, pressed = accent solid
//   * Press also overlays a translucent accent fill on top of the body
//     (user's "the longer you hold the more it fills with accent").
void c_button::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    const auto& cs = style::current();
    const Renderer::rect br = rrect(bounds_);
    const float radius = style::rounding().of(style::rounding().button);

    // Body fill — 0xFF232328 (same as window body). Stays flat in
    // every state — user 2026-06-01: the press effect is BORDER ONLY.
    r.render_filled_rounded_rect(br, cs.button_bg, radius);

    // Border: idle → outline, hover → accent @ 0x80, pressed-and-held
    // → accent alpha ramps from 0x80 toward full 0xFF as press_anim_
    // climbs to 1. So the longer the hold, the brighter the border.
    const Renderer::color hover_color{cs.accent.r, cs.accent.g, cs.accent.b, 0x80};
    Renderer::color border_color = Renderer::color::lerp(cs.outline, hover_color, hover_anim_);
    border_color = Renderer::color::lerp(border_color, cs.accent, press_anim_);
    r.render_rounded_rect(br, border_color, radius, 1.0f);

    // Centered text — 0xFFBEBEBE
    if (font_) {
        const float text_w = font_->measure_text(label_);
        const float x = bounds_.x + (bounds_.w - text_w) * 0.5f;
        const float y = bounds_.y + (bounds_.h - font_->pixel_height()) * 0.5f;
        r.render_text(*font_, Renderer::vec2{x, y}, cs.button_text, label_.c_str());
    }
}

// ---------------------------------------------------------------- c_button_grid
void c_button_grid::update(float dt)
{
    if (per_row_ < 1) per_row_ = 1;
    const auto& sz = style::sizes();

    // Position the grid itself from parent.next_y() so it stacks with
    // the rest of the groupbox children (was unset → grid sat at the
    // bottom-right of the page somewhere).
    if (auto p = parent_.lock()) {
        const float parent_next_y = p->next_y();
        bounds_.x = p->bounds().x + sz.text_margin;
        bounds_.y = parent_next_y;
        bounds_.w = p->bounds().w - 2.0f * sz.text_margin;
        // bounds.h was set externally — keep it. If unset, derive from
        // the row count.
        const int n = static_cast<int>(children_.size());
        const int rows = (n + per_row_ - 1) / per_row_;
        if (bounds_.h <= 0.0f) {
            bounds_.h = rows * sz.button_h
                      + std::max(0, rows - 1) * 4.0f;  // matches tight gap below
        }
        extra_height_ = sz.button_height;
        p->set_next_y(parent_next_y + bounds_.h + extra_height_);
    }

    // Tight grid gaps — user 2026-06-02: previous column_gap (~20 px) +
    // control_gap_y (~6 px) left too much air between the cells. Drop
    // both to a fixed 4 px so the 2x2 reads as a unified block.
    const float gap    = 4.0f;
    const float gap_y  = 4.0f;
    const float cell_w = (bounds_.w - gap * (per_row_ - 1)) / per_row_;
    const float cell_h = sz.button_h;

    // "odd_nth_occupy_full_width" (binary string @ this widget) — when
    // the LAST child has no pair on its row, it spans the full grid
    // width on its own row. With per_row=2 and 5 children this gives
    // a 2+2+1 layout with the trailing button as a wide bar under the
    // 4 cells.
    const int n_children = static_cast<int>(children_.size());

    int i = 0;
    float y = bounds_.y;
    for (auto& c : children_) {
        if (!c) { ++i; continue; }
        const int col = i % per_row_;
        if (col == 0 && i > 0) y += cell_h + gap_y;

        const bool is_last_orphan =
            (i == n_children - 1) && (n_children % per_row_ != 0);

        const float this_x = is_last_orphan
                                ? bounds_.x
                                : bounds_.x + col * (cell_w + gap);
        const float this_w = is_last_orphan ? bounds_.w : cell_w;

        // CRITICAL: children must use inline layout, otherwise their
        // own update() will re-pull bounds from parent->next_y() and
        // clobber the grid's per-cell positions.
        c->set_inline_layout(true);
        c->set_bounds({ this_x, y, this_w, cell_h });
        c->update(dt);
        ++i;
    }
}

void c_button_grid::render(Renderer::c_renderer_core& r)
{
    c_base_element::render(r);
}

// ---------------------------------------------------------------- c_checkbox
// BINARY: c_checkbox__update @ 0x14003FBC0
// Reads parent->next_y (+60) for Y position.
// padding_offset based on parent.flags & ef_has_label (0x100).
// text_margin inset applied when NOT inline/sibling mode.
void c_checkbox::update(float dt)
{
    auto& in = input::current();
    const auto& sz = style::sizes();

    // Modal guard captured here, used below — layout MUST still run
    // so the sibling after this checkbox gets the right y. If we
    // returned early, the next sibling would land on top of this
    // one (user-reported: "if you open a color picker, it breaks
    // elements of the menu, they overlap").
    const bool input_blocked = in.input_blocked_by_other(this);

    auto p = parent_.lock();
    if (!p) return;

    // Binary: padding_offset = (parent.flags & 0x100) ? g_style_checkbox_pad : 0
    const int padding_offset = (p->element_flags() & ef_has_label)
                             ? static_cast<int>(sz.checkbox_pad) : 0;

    float parent_x = p->bounds().x;
    const float parent_next_y = p->next_y();
    float parent_y = static_cast<float>(padding_offset) + parent_next_y;
    float parent_w = p->bounds().w;

    bounds_.x = parent_x;
    bounds_.y = parent_y;
    bounds_.w = parent_w;
    bounds_.h = sz.checkbox_height;  // 20

    if (!inline_layout_) {
        bounds_.x += sz.text_margin;
        bounds_.w -= 2.0f * sz.text_margin;
    }

    extra_height_ = visible_ ? sz.button_height : 0.0f;

    if (!inline_layout_) {
        p->set_next_y(parent_next_y + bounds_.h + extra_height_);
    }

    // Box position at +340, +344 in binary.
    // User 2026-06-01: SNAP box origin to whole pixels so the rounded
    // outline rasterizes symmetrically. When the parent column lanes
    // come out at non-integer widths (e.g. col_w = 174.5) the box's
    // unrounded x lands at .5 and one side of the corner-arc fans
    // falls on a different pixel row than the other — read by the
    // user as "rounded on one side only".
    if (!inline_layout_) {
        box_x_ = std::floor(bounds_.x);
        box_y_ = std::floor(bounds_.y + bounds_.h / 2.0f - sz.checkbox_size / 2.0f + 1.0f);
    } else {
        box_x_ = std::floor(bounds_.x);
        box_y_ = std::floor(bounds_.y + bounds_.h / 2.0f - sz.checkbox_size / 2.0f);
    }

    hit_rect_ = bounds_;

    // Checked crossfade — snap on the first frame (no fade-in for
    // boxes that load already checked), then tween at the house 14/s.
    // Ticked BEFORE the modal early-out so an uncheck done right as a
    // popup opens still fades instead of freezing mid-state.
    if (!check_anim_init_) {
        check_anim_ = get() ? 1.0f : 0.0f;
        check_anim_init_ = true;
    }
    {
        const float target = get() ? 1.0f : 0.0f;
        check_anim_ += (target - check_anim_)
                     * std::clamp(dt * 14.0f, 0.0f, 1.0f);
    }

    if (input_blocked) {
        // Hard-cancel hover + animation so the border stops changing
        // colour the instant a popup/dialog claims the modal.
        hovered_ = false;
        hover_anim_ = 0.0f;
        return;
    }

    // Box hover OR a forced hover from the host row (label-text hover).
    hovered_ = hit_test(in.mouse_pos.x, in.mouse_pos.y) || force_hover_;

    if (hit_test(in.mouse_pos.x, in.mouse_pos.y) &&
        in.mouse_pressed[input::mb_left]) {
        set(!get());
        in.consume_mouse_press();
    }

    // Tick hover anim toward target (binary uses
    // anim_calc_progress_4ch). Lerp constant ~14/s matches button anim.
    const float hover_target = hovered_ ? 1.0f : 0.0f;
    hover_anim_ += (hover_target - hover_anim_) * std::clamp(dt * 14.0f, 0.0f, 1.0f);
}

// BINARY: c_checkbox__render_slot3 @ 0x140040000
// 1. Label first via vt[+472]
//    Position: {bounds.x + checkbox_size + label_offset, bounds.y + height/2}
//    Color: disabled = 0x80BEBEBE, enabled = 0xFFBEBEBE
// 2. Border via anim_blend_colors_multi:
//    unchecked = 0xFF3E3E42, hover = 0xFF606064, checked = accent
// 3. If checked: fill rect then checkmark lines
//    If unchecked: outline only
// 4. Checkmark geometry (EXACT from binary multipliers):
//    p1 = {box_x + 0.22*size, box_y + 0.50*size}
//    p2 = {box_x + 0.42*size, box_y + 0.73*size}
//    p3 = {box_x + 0.80*size, box_y + 0.27*size}
//    Line color: 0xFF202024 (dark), NOT 0xFF1EA7A4 as old docs claimed
void c_checkbox::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    const auto& cs = style::current();
    const auto& sz = style::sizes();

    // Snap box size to integer for the same reason box_x_/box_y_ are
    // floored — keeps the rounded outline pixel-symmetric across all
    // four corners under non-integer DPI scaling.
    const float box_size  = std::floor(sz.checkbox_size);
    const float label_off = sz.label_offset;

    // 1. Render label first
    const float label_x = bounds_.x + box_size + label_off;
    Renderer::color text_color = cs.text;  // 0xFFBEBEBE
    if (font_) {
        const float baseline_y = bounds_.y + (bounds_.h - font_->pixel_height()) * 0.5f;
        r.render_text(*font_, Renderer::vec2{label_x, baseline_y}, text_color, label_.c_str());
    }

    // Box rect
    const float box_x = box_x_;
    const float box_y = box_y_;
    const Renderer::rect box{box_x, box_y, box_size, box_size};

    // 2. 4-way border-color blend matching the binary's
    // anim_blend_colors_multi @ c_checkbox__render line 132:
    //   idle           = 0xFF3E3E42 (cs.checkbox_bg)
    //   hover          = 0xFF606064 (cs.checkbox_hover)
    //   checked        = cs.accent
    //   checked+hover  = each accent channel + 25.5 clamped to 255
    // Hover progress is hover_anim_; checked is a hard 0/1 by is_checked.
    auto lerp_u8 = [](std::uint8_t a, std::uint8_t b, float t) {
        const float v = static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t;
        return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 255.0f));
    };
    auto lerp_col = [&](Renderer::color a, Renderer::color b, float t) {
        return Renderer::color{
            lerp_u8(a.r, b.r, t),
            lerp_u8(a.g, b.g, t),
            lerp_u8(a.b, b.b, t),
            lerp_u8(a.a, b.a, t),
        };
    };
    auto bump = [](std::uint8_t v) {
        return static_cast<std::uint8_t>(std::min(255.0f, static_cast<float>(v) + 25.5f));
    };

    const Renderer::color accent_bright{
        bump(cs.accent.r), bump(cs.accent.g), bump(cs.accent.b), cs.accent.a};

    const Renderer::color col_idle  = cs.checkbox_bg;
    const Renderer::color col_hover = cs.checkbox_hover;
    const Renderer::color col_chk   = cs.accent;
    const Renderer::color col_chk_h = accent_bright;

    // Checked axis is the smoothed check_anim_ (not the raw bool) so
    // unchecking fades the accent out of the border instead of
    // snapping (user 2026-07-17).
    const float ca = std::clamp(check_anim_, 0.0f, 1.0f);
    const Renderer::color row_lo = lerp_col(col_idle,  col_chk,   ca);
    const Renderer::color row_hi = lerp_col(col_hover, col_chk_h, ca);
    const Renderer::color border_col = lerp_col(row_lo, row_hi, hover_anim_);

    // 3. Render box.
    //
    // Binary `c_checkbox__render_slot3.c:155-186`:
    //   if (is_checked) {
    //       vt[+288] render_filled_rect(box, *, final_color, g_visibility_flag);
    //       vt[+384] render_line(p1, p2, 0xFF202024, ...);
    //       vt[+384] render_line(p2, p3, 0xFF202024, ...);
    //   } else {
    //       vt[+296] render_rect_outline(box);    // sharp in binary
    //   }
    //
    // User 2026-06-02: BOTH states must read as rounded. `render_rounded_rect`
    // at radius 3 with 6 segments produces visible arc-strip facets — looks
    // "broken" / asymmetric. Instead, draw the unchecked border as a
    // concentric-fill ring: outer rounded fill = border colour, inner rounded
    // fill 1 px inset = parent body colour. Same arc geometry, no facets.
    // Cap at a quarter of the box, not half — half turned the box into
    // a full circle at high rounding Factor (user 2026-07-17 "checkboxes
    // became circles on 10 rounding"). Quarter keeps straight edge on
    // every side at any Factor; unchanged at the default radius 2.
    const float r_sym = std::floor(
        std::min(style::rounding().of(style::rounding().checkbox), box_size * 0.25f));

    // Unchecked chrome — always drawn as the base layer so the checked
    // accent can crossfade over it.
    //
    // Border is a 3-row ring: outer and inner rows at reduced alpha,
    // middle row full — reads as a soft 3 px border instead of a hard
    // full-alpha band (user 2026-07-17). Interior = 0xFF1E1E22
    // (30,30,34), slightly darker than the body fill, per the same
    // request.
    if (ca < 0.999f) {
        // Rows are OPAQUE colours pre-blended against the 0x202024 body
        // (translucent stacked strokes read dimmer on the horizontal
        // runs — user 2026-07-17 "top and bottom shouldn't become
        // dimmer"). Ratios from the user-measured pixels: outer
        // 47,47,51 = 50% · middle 59,59,63 = 90% · inner 41,41,45 ≈
        // 30% of the border colour over the body. Blending here (not
        // hardcoding) keeps hover/accent states working.
        const Renderer::color row_bg{0x20, 0x20, 0x24, 0xFF};
        const auto row_col = [&](float k) {
            return lerp_col(row_bg, border_col, k);
        };
        // +1 on the shared radius: at the default Factor (2.75 → 2) the
        // decremented inner rows went square and the outline read as
        // barely rounded (user 2026-07-17).
        const float r0 = r_sym > 0.0f ? r_sym + 1.0f : 0.0f;
        r.render_rounded_rect(box, row_col(0.50f), r0, 1.0f, 12);
        r.render_rounded_rect({box_x + 1.0f, box_y + 1.0f,
                               box_size - 2.0f, box_size - 2.0f},
                              row_col(0.90f), std::max(0.0f, r0 - 1.0f),
                              1.0f, 12);
        r.render_rounded_rect({box_x + 2.0f, box_y + 2.0f,
                               box_size - 4.0f, box_size - 4.0f},
                              row_col(0.30f), std::max(0.0f, r0 - 2.0f),
                              1.0f, 12);
        const Renderer::rect interior{box_x + 3.0f, box_y + 3.0f,
                                      box_size - 6.0f, box_size - 6.0f};
        r.render_filled_rounded_rect(interior,
                                     Renderer::color{0x1E, 0x1E, 0x22, 0xFF},
                                     std::max(0.0f, r0 - 3.0f));
    }

    // Checked layer — accent fill + checkmark, alpha-scaled by the
    // crossfade so checking/unchecking fades instead of snapping.
    if (ca > 0.001f) {
        const auto scale_a = [&](Renderer::color c) {
            return Renderer::color{c.r, c.g, c.b,
                static_cast<std::uint8_t>(c.a * ca)};
        };
        r.render_filled_rounded_rect(box, scale_a(cs.accent), r_sym);
        // White wash on hover — not in binary, kept as a small flourish.
        if (hover_anim_ > 0.001f) {
            const std::uint8_t a = static_cast<std::uint8_t>(
                std::clamp(hover_anim_, 0.0f, 1.0f) * 0x40 * ca);
            r.render_filled_rounded_rect(box,
                Renderer::color{0xFF, 0xFF, 0xFF, a}, r_sym);
        }

        // Checkmark — multipliers + color from binary lines 164-181.
        // Filled circle at the elbow: the two 2-px line quads meet at
        // p2 with square butt ends, which left a notch at the joint
        // (user 2026-07-17 "where they meet at the bottom it's cut
        // off").
        const float s = box_size;
        Renderer::vec2 p1{box_x + s * 0.22f, box_y + s * 0.50f};
        Renderer::vec2 p2{box_x + s * 0.42f, box_y + s * 0.73f};
        Renderer::vec2 p3{box_x + s * 0.80f, box_y + s * 0.27f};
        const Renderer::color chk = scale_a(cs.checkbox_check);
        r.render_line(p1, p2, chk, 2.0f);
        r.render_line(p2, p3, chk, 2.0f);
        r.render_filled_circle(p2, 1.0f, chk);
    }
}

} // namespace GUI
