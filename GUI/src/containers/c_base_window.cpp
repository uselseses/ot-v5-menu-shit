// =============================================================================
// c_base_window.cpp --- bg + outline + title bar; default vertical
// child layout in content_rect().
// =============================================================================

#include <GUI/containers/c_base_window.hpp>

#include <GUI/style/colorscheme.hpp>
#include <GUI/style/element_sizes.hpp>
#include <GUI/style/rounding.hpp>

#include <Renderer/c_renderer_core.hpp>
#include <Renderer/font.hpp>

namespace GUI {

c_base_element::rect_t c_base_window::content_rect() const
{
    const auto& sz = style::sizes();
    return {
        bounds_.x,
        bounds_.y + sz.title_bar_h,
        bounds_.w,
        bounds_.h - sz.title_bar_h,
    };
}

void c_base_window::layout_children()
{
    const auto cr = content_rect();
    const auto& sz = style::sizes();
    float y = cr.y;
    for (auto& c : children_) {
        if (!c || !c->visible()) continue;
        // Default: stack children full-width with control_h height.
        const float h = c->bounds().h > 0 ? c->bounds().h : sz.control_h;
        c->set_bounds({cr.x, y, cr.w, h});
        y += h + sz.control_gap_y;
    }
}

void c_base_window::update(float dt)
{
    layout_children();
    for (auto& c : children_) if (c) c->update(dt);
}

void c_base_window::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;

    const auto& cs = style::current();
    const auto& sz = style::sizes();

    const Renderer::rect outer{bounds_.x, bounds_.y, bounds_.w, bounds_.h};
    const Renderer::rect title{bounds_.x, bounds_.y, bounds_.w, sz.title_bar_h};

    const float radius = style::rounding().of(style::rounding().menu_window);
    r.render_filled_rounded_rect(outer, cs.window_bg, radius);
    r.render_filled_rect        (title, cs.title_bg);     // sharp under the bar so it fills the curve
    // Re-cover the top corners of the title bar with the window bg curve:
    //   trick --- redraw a small rounded rect at top edge only.
    r.render_filled_rounded_rect(Renderer::rect{outer.x, outer.y, outer.w, sz.title_bar_h * 0.5f},
                                 cs.title_bg, radius);
    r.render_filled_rect        (Renderer::rect{outer.x, outer.y + sz.title_bar_h * 0.5f,
                                                outer.w, sz.title_bar_h * 0.5f},
                                 cs.title_bg);
    r.render_rounded_rect       (outer, cs.window_border, radius, sz.window_outline_px);

    // Children clip to content_rect.
    const auto cr = content_rect();
    r.push_clip(Renderer::rect{cr.x, cr.y, cr.w, cr.h});
    c_base_element::render(r);
    r.pop_clip();
}

// =============================================================================
// c_dialog_window
// =============================================================================

} // namespace GUI

#include <GUI/containers/c_dialog_window.hpp>
#include <GUI/core/globals.hpp>
#include <GUI/input/cursor.hpp>
#include <GUI/input/input_state.hpp>

#include <algorithm>

namespace GUI {

// Title-band height is the vertical space the inline title label takes
// at the top of the dialog body. Matches what render() draws at
// `bounds_.y + 10`. Children stack BELOW this band.
static float dialog_title_band(const Renderer::font* title_font, const std::string& label) {
    if (!title_font || label.empty()) return 0.0f;
    return title_font->pixel_height() + 18.0f;
}

void c_dialog_window::layout_children()
{
    const auto& sz = style::sizes();
    const float title_band  = dialog_title_band(title_font_, label_);
    const float side_pad    = sz.text_margin;
    const float button_band = std::max(sz.button_h, 24.0f) + 10.0f * 2.0f;

    const float content_top    = bounds_.y + title_band;
    const float content_bottom = bounds_.y + bounds_.h - button_band;

    float y = content_top;
    for (auto& c : children_) {
        if (!c || !c->visible()) continue;
        // Force inline layout so c_label etc. don't override the
        // bounds we set here with their own parent-derived layout.
        c->set_inline_layout(true);

        float h = c->bounds().h;
        if (h <= 0.0f) {
            // Default to the body font's line height when we can —
            // most dialog children are c_labels.
            const Renderer::font* body = title_font_ ? title_font_ : button_font_;
            h = body ? body->pixel_height() + 2.0f : sz.control_h;
        }
        c->set_bounds({
            bounds_.x + side_pad, y,
            bounds_.w - 2.0f * side_pad, h
        });
        y += h + 2.0f;   // tight per-line spacing for label stacks
        if (y >= content_bottom) break;
    }
}

void c_dialog_window::update(float dt)
{
    if (!visible_) {
        if (was_visible_) {
            // Just became hidden — release modal so peer widgets can
            // resume processing input.
            input::current().release_modal(this);
            was_visible_ = false;
            auto_sized_  = false;     // re-measure on next open
        }
        return;
    }

    auto& in = input::current();
    auto& cur = input::cursor();
    const auto& sz = style::sizes();

    // Auto-size from content + title + buttons. Done BEFORE we centre
    // so the centred position is correct for the new bounds. Skipped
    // if the host turned auto-size off. Stable across frames — runs
    // ONCE per open and caches the result so the dialog doesn't
    // re-measure its children every frame (which caused a visible
    // flicker as the children's bounds reflowed). Re-runs when the
    // host opens the dialog again.
    if (auto_size_ && !auto_sized_) {
        const float side_pad     = sz.text_margin;
        const float title_band   = dialog_title_band(title_font_, label_);
        const float btn_band_h   = std::max(sz.button_h, 24.0f) + 10.0f * 2.0f;

        // Width: use measured TEXT widths from children labels (their
        // bounds.w hasn't been set yet on the first frame). Plus the
        // dialog's own title width and the button-pair width.
        float content_w = 0.0f;
        float content_h = 0.0f;
        const Renderer::font* body = title_font_ ? title_font_ : button_font_;
        const float line_h = body ? body->pixel_height() + 2.0f : sz.control_h;
        for (auto& c : children_) {
            if (!c || !c->visible()) continue;
            if (body) {
                content_w = std::max(content_w,
                                     body->measure_text(c->label().c_str()));
            }
            content_w = std::max(content_w, c->bounds().w);
            const float h_use = c->bounds().h > 0.0f ? c->bounds().h : line_h;
            content_h += h_use + 2.0f;   // matches tight spacing in layout_children
        }
        // Title text contributes to the min width (so the title isn't
        // squeezed). +24 px slack for the close X.
        float title_w = 0.0f;
        if (title_font_) {
            title_w = title_font_->measure_text(label_.c_str()) + 24.0f;
        }
        float btn_pair_w = 220.0f;
        if (button_font_) {
            const float dw = button_font_->measure_text(dismiss_label_.c_str());
            const float cw = button_font_->measure_text(confirm_label_.c_str());
            btn_pair_w = std::max(dw, cw) * 2.0f + 12.0f + 48.0f;
        }

        const float screen_w = static_cast<float>(globals::g_screen_w);
        const float screen_h = static_cast<float>(globals::g_screen_h);
        const float min_w = 280.0f * style::gui_scale();
        const float max_w = (screen_w > 0.0f) ? screen_w * 0.6f : 1000.0f;

        float want_w = std::max({content_w + 2.0f * side_pad, title_w, btn_pair_w, min_w});
        want_w = std::min(want_w, max_w);

        // Height = title band + 8 px breather + content + small breather +
        // buttons. The below-content breather is tight (2 px) so the
        // Dismiss button hugs the content instead of floating far below it
        // (user 2026-06-11: "too much spacing between the changelog text
        // and the Dismiss button").
        float want_h = title_band + 8.0f + content_h + 2.0f + btn_band_h;
        if (screen_h > 0.0f) want_h = std::min(want_h, screen_h * 0.85f);

        bounds_.w = want_w;
        bounds_.h = want_h;
        sized_w_ = want_w;
        sized_h_ = want_h;
        auto_sized_ = true;
    }

    // Re-apply the persisted size EVERY frame — the parent menu window
    // stomps bounds_ with its full content rect before update, which
    // made dialogs render menu-sized (user 2026-06-09 "dialog size is
    // too massive").
    if (sized_w_ > 0.0f && sized_h_ > 0.0f) {
        bounds_.w = sized_w_;
        bounds_.h = sized_h_;
    }

    // User 2026-06-02: dialog is centred INSIDE the menu_window
    // (its parent). When there's no parent (host-driven popup),
    // fall back to screen-centring.
    if (auto p = parent_.lock()) {
        const auto pb = p->bounds();
        bounds_.x = pb.x + (pb.w - bounds_.w) * 0.5f;
        bounds_.y = pb.y + (pb.h - bounds_.h) * 0.5f;
    } else if (globals::g_screen_w > 0 && globals::g_screen_h > 0) {
        bounds_.x = (static_cast<float>(globals::g_screen_w) - bounds_.w) * 0.5f;
        bounds_.y = (static_cast<float>(globals::g_screen_h) - bounds_.h) * 0.5f;
    }

    // Animate opening / closing — smooth relaxed fade rate
    const float rate = 5.0f * style::anim_speed();
    if (closing_) {
        anim_progress_ = std::max(0.0f, anim_progress_ - dt * rate);
        if (anim_progress_ <= 0.001f) {
            visible_ = false;
            closing_ = false;
            if (was_visible_) {
                in.release_modal(this);
                was_visible_ = false;
            }
            if (on_close_) on_close_();
            return;
        }
    } else {
        anim_progress_ = std::min(1.0f, anim_progress_ + dt * rate);
    }

    // Claim modal on the visible→true transition so peer popups
    // (dropdown / colorpicker) cannot open while the dialog is up. The
    // displacing callback is a no-op so a peer popup attempting to
    // steal the modal cannot force the dialog to close — only the X /
    // Dismiss / Confirm buttons can.
    if (!was_visible_) {
        in.claim_modal(this, [](){});
        was_visible_ = true;
    } else if (in.modal_owner != this) {
        // Something stole the modal mid-frame (defensive). Reclaim.
        in.claim_modal(this, [](){});
    }

    title_bar_rect_.x = bounds_.x;
    title_bar_rect_.y = bounds_.y;
    title_bar_rect_.w = bounds_.w;
    title_bar_rect_.h = sz.title_bar_h;

    body_rect_.x = bounds_.x;
    body_rect_.y = bounds_.y + sz.title_bar_h;
    body_rect_.w = bounds_.w;
    body_rect_.h = bounds_.h - sz.title_bar_h;

    close_button_rect_.x = bounds_.x + bounds_.w - 24.0f;
    close_button_rect_.y = bounds_.y + 8.0f;
    close_button_rect_.w = 16.0f;
    close_button_rect_.h = 16.0f;

    // User 2026-06-01: Dismiss + Confirm buttons are now sized
    // proportional to the dialog body width and CENTERED as a pair
    // along the body's horizontal axis.
    const float btn_h    = std::max(sz.button_h, 24.0f);
    const float btn_gap  = 6.0f;
    const float btn_margin_bot = 10.0f;
    const float btn_inset = sz.text_margin;
    const float btn_y = body_rect_.y + body_rect_.h - btn_h - btn_margin_bot;

    if (show_confirm_) {
        // Two-button mode: dismiss + confirm centred as a pair.
        float btn_w = std::clamp(body_rect_.w * 0.28f, 72.0f, 110.0f);
        if (btn_w * 2.0f + btn_gap > body_rect_.w - 2.0f * btn_inset) {
            btn_w = (body_rect_.w - 2.0f * btn_inset - btn_gap) * 0.5f;
        }
        const float pair_w = 2.0f * btn_w + btn_gap;
        const float pair_x = body_rect_.x + (body_rect_.w - pair_w) * 0.5f;

        dismiss_button_rect_.x = pair_x;
        dismiss_button_rect_.y = btn_y;
        dismiss_button_rect_.w = btn_w;
        dismiss_button_rect_.h = btn_h;

        confirm_button_rect_.x = pair_x + btn_w + btn_gap;
        confirm_button_rect_.y = btn_y;
        confirm_button_rect_.w = btn_w;
        confirm_button_rect_.h = btn_h;
    } else {
        // Single-button mode: full-width Dismiss pill
        dismiss_button_rect_.x = body_rect_.x + btn_inset;
        dismiss_button_rect_.y = btn_y;
        dismiss_button_rect_.w = body_rect_.w - 2.0f * btn_inset;
        dismiss_button_rect_.h = btn_h;
        confirm_button_rect_ = rect_t{0, 0, 0, 0};
    }

    // Lazy-build Dismiss + Confirm as real c_button instances
    if (!buttons_built_) {
        dismiss_btn_ = std::make_shared<c_button>(dismiss_label_, [this] {
            if (on_dismiss_)    on_dismiss_();
            close();
        });
        confirm_btn_ = std::make_shared<c_button>(confirm_label_, [this] {
            if (on_confirm_) on_confirm_();
            close();
        });
        dismiss_btn_->set_inline_layout(true);
        confirm_btn_->set_inline_layout(true);
        dismiss_btn_->set_bypass_modal(true);
        confirm_btn_->set_bypass_modal(true);
        buttons_built_ = true;
    }
    // Keep fonts + labels fresh in case they were reset after build.
    dismiss_btn_->set_font(button_font_);
    confirm_btn_->set_font(button_font_);
    // Position the buttons via the rects computed above.
    dismiss_btn_->set_bounds(dismiss_button_rect_);
    confirm_btn_->set_bounds(confirm_button_rect_);
    dismiss_btn_->set_visible(true);
    confirm_btn_->set_visible(show_confirm_);

    auto contains = [](const rect_t& r, Renderer::vec2 p) {
        return p.x >= r.x && p.x <= r.x + r.w &&
               p.y >= r.y && p.y <= r.y + r.h;
    };

    const bool hov_close = contains(close_button_rect_, in.mouse_pos);
    if (hov_close) cur.set(input::cursor_type::hand);

    // Update CHILDREN first so embedded inputs can receive click BEFORE
    // the dialog swallows stray body presses.
    c_base_window::update(dt);

    // Dialog's own Dismiss / Confirm buttons
    dismiss_btn_->update(dt);
    if (show_confirm_) confirm_btn_->update(dt);

    // X (close) glyph + outside-click absorption
    if (in.mouse_pressed[input::mb_left]) {
        if (hov_close) {
            close();
            in.consume_mouse_press();
            return;
        }
        in.consume_mouse_press();
    }
    if (in.mouse_pressed[input::mb_right]) {
        in.consume_mouse_press(input::mb_right);
    }
}

// User 2026-06-02: rewritten to match the binary screenshots
void c_dialog_window::render(Renderer::c_renderer_core& r)
{
    if (!visible_ || anim_progress_ <= 0.001f) return;

    const auto& sz = style::sizes();
    const auto& cs = style::current();

    // Smooth ease curve for pure alpha fade
    const float t = anim_progress_;
    const float eased = t * t * (3.0f - 2.0f * t); // Smoothstep curve

    r.push_layer(Renderer::layer_e::overlay);

    // 1. Screen-dimming backdrop — smoothly dims in/out
    if (globals::g_screen_w > 0 && globals::g_screen_h > 0) {
        const std::uint8_t bg_a = static_cast<std::uint8_t>(0x80 * eased);
        r.render_filled_rect(
            Renderer::rect{0.0f, 0.0f,
                           static_cast<float>(globals::g_screen_w),
                           static_cast<float>(globals::g_screen_h)},
            Renderer::color{0x00, 0x00, 0x00, bg_a});
    }

    r.push_alpha(eased);

    // 2. Border + body — single body fill + 1-px outline strokes (no stacked multi-fills)
    {
        const Renderer::color border_outer{0x20, 0x20, 0x24, 0xFF};
        const Renderer::color border_inner{0x60, 0x60, 0x64, 0xFF};
        const Renderer::color body_fill   {0x20, 0x20, 0x24, 0xFF};
        const float rr = style::rounding().dialog_window
                             ? style::rounding().chrome_radius()
                             : 0.0f;
        // Body fill (single pass)
        r.render_filled_rounded_rect(
            Renderer::rect{bounds_.x, bounds_.y, bounds_.w, bounds_.h},
            body_fill, rr);
        // Border outline rings (no interior fill to bleed through)
        r.render_rounded_rect(
            Renderer::rect{bounds_.x - 1.0f, bounds_.y - 1.0f,
                           bounds_.w + 2.0f, bounds_.h + 2.0f},
            border_inner, rr + 1.0f, 1.0f);
        r.render_rounded_rect(
            Renderer::rect{bounds_.x - 2.0f, bounds_.y - 2.0f,
                           bounds_.w + 4.0f, bounds_.h + 4.0f},
            border_outer, rr + 2.0f, 1.0f);
    }

    // 3. Title — left-aligned label INSIDE the body
    if (title_font_ && !label_.empty()) {
        const float tx = bounds_.x + sz.text_margin;
        const float ty = bounds_.y + 10.0f;
        r.render_text(*title_font_, Renderer::vec2{tx, ty},
                      Renderer::color{0xDE, 0xDE, 0xDE, 0xFF}, label_.c_str());
    }

    // 4. Close X in the top-right corner of the body.
    const float cx = bounds_.x + bounds_.w - 16.0f;
    const float cy = bounds_.y + 16.0f;
    r.render_line({cx - 4, cy - 4}, {cx + 4, cy + 4}, cs.text, 1.5f);
    r.render_line({cx - 4, cy + 4}, {cx + 4, cy - 4}, cs.text, 1.5f);

    // 5. Children stacked below the title, above the button row.
    const float title_band  = dialog_title_band(title_font_, label_);
    const float side_pad    = sz.text_margin;
    const float button_band = std::max(sz.button_h, 24.0f) + 10.0f * 2.0f;
    Renderer::rect content_clip{
        bounds_.x + side_pad,
        bounds_.y + title_band,
        bounds_.w - 2.0f * side_pad,
        std::max(0.0f, bounds_.h - title_band - button_band)
    };
    r.push_clip(content_clip);
    c_base_element::render(r);
    r.pop_clip();

    // 6. Confirm / Dismiss buttons
    if (dismiss_btn_) dismiss_btn_->render(r);
    if (show_confirm_ && confirm_btn_) confirm_btn_->render(r);

    r.pop_alpha();

    r.pop_layer();
}

} // namespace GUI
