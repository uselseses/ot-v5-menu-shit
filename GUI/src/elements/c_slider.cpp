// =============================================================================
// c_slider<T>.cpp — explicit instantiations for int / float / double / int64
//
// Verified against c_slider__render_0 @ 0x14004BB80 (2026-05-31).
// See HOW_IT_WORKS.md §16 for exact render sequence.
// =============================================================================

#include <GUI/elements/c_slider.hpp>

#include <GUI/input/cursor.hpp>
#include <GUI/style/colorscheme.hpp>
#include <GUI/style/element_sizes.hpp>
#include <GUI/style/rounding.hpp>

#include <Renderer/c_renderer_core.hpp>
#include <Renderer/font.hpp>

#include <algorithm>
#include <cstdio>
#include <cmath>

namespace GUI {

namespace {
inline Renderer::rect rrect(c_base_element::rect_t r) {
    return Renderer::rect{r.x, r.y, r.w, r.h};
}

template <typename T>
T lerp_range(T a, T b, float t) {
    if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(a + (b - a) * t);
    } else {
        return static_cast<T>(a + std::llround(static_cast<double>(b - a) * t));
    }
}

template <typename T>
float range_to_t(T v, T a, T b) {
    if (a == b) return 0;
    return static_cast<float>(static_cast<double>(v - a) / static_cast<double>(b - a));
}

template <typename T>
std::string format_value(T v) {
    char buf[64];
    if constexpr (std::is_same_v<T, float>) {
        std::snprintf(buf, sizeof(buf), "%.2f", v);
    } else if constexpr (std::is_same_v<T, double>) {
        std::snprintf(buf, sizeof(buf), "%.3f", v);
    } else {
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
    }
    return buf;
}
} // anon

template <typename T>
void c_slider<T>::update(float dt)
{
    auto& in = input::current();
    const auto& sz = style::sizes();

    auto p = this->parent_.lock();
    if (p) {
        const float parent_next_y = p->next_y();
        const bool inline_layout = this->inline_layout();
        this->bounds_.x = inline_layout ? p->bounds().x : p->bounds().x + sz.text_margin;
        this->bounds_.y = parent_next_y;
        this->bounds_.w = inline_layout ? p->bounds().w : p->bounds().w - 2.0f * sz.text_margin;
        // Slider row a touch taller than a checkbox so the label clears
        // the track with the larger (14px) font (user 2026-06-10).
        this->bounds_.h = sz.checkbox_height + 4.0f;

        this->extra_height_ = this->visible_ ? sz.button_height : 0.0f;
        if (!inline_layout) {
            p->set_next_y(parent_next_y + this->bounds_.h + this->extra_height_);
        }
    }

    const auto br = rrect(this->bounds_);
    const float track_h = std::max(sz.slider_track_h, 8.0f);
    const Renderer::rect bar{br.x, br.y + br.h - track_h, br.w, track_h};

    // Modal guard — suppress hover/click while a popup or dialog owns
    // the modal. Layout already ran above so the slider keeps its slot.
    const bool input_blocked = in.input_blocked_by_other(this);

    if (input_blocked) {
        // Hard-cancel hover/drag + snap the hover anim to 0 so the
        // slider track stops glowing the instant a popup/dialog
        // claims the modal.
        this->hovered_ = false;
        dragging_      = false;
        hover_anim_    = 0.0f;
        return;
    }

    this->hovered_ = bar.contains(in.mouse_pos);
    // User: slider hover should show the HAND cursor (same as logo).
    if (this->hovered_ || dragging_) {
        input::cursor().set(input::cursor_type::hand);
    }

    const float target_t = range_to_t<T>(this->get(), min_, max_);
    if (!anim_init_) {
        anim_t_ = target_t;
        anim_init_ = true;
    }

    if (this->hovered_ && in.mouse_pressed[input::mb_left]) {
        dragging_ = true;
        is_actively_dragging_ = false;
        drag_start_x_ = in.mouse_pos.x;
        in.consume_mouse_press();

        if (bar.w > 0) {
            float t = std::clamp((in.mouse_pos.x - bar.x) / bar.w, 0.0f, 1.0f);
            this->set(lerp_range<T>(min_, max_, t));
        }
    }

    if (!in.mouse_down[input::mb_left]) {
        dragging_ = false;
        is_actively_dragging_ = false;
    }

    const float hover_target = (this->hovered_ || dragging_) ? 1.0f : 0.0f;
    hover_anim_ += (hover_target - hover_anim_) * std::min(1.0f, dt * 14.0f);

    if (dragging_ && bar.w > 0) {
        if (std::abs(in.mouse_pos.x - drag_start_x_) > 1.5f) {
            is_actively_dragging_ = true;
        }

        float t = std::clamp((in.mouse_pos.x - bar.x) / bar.w, 0.0f, 1.0f);
        this->set(lerp_range<T>(min_, max_, t));

        if (is_actively_dragging_) {
            anim_t_ = t; // Instant 1:1 response when actively dragging
        } else {
            // Smooth glide when clicking/holding a spot
            const float new_target = range_to_t<T>(this->get(), min_, max_);
            anim_t_ += (new_target - anim_t_) * std::clamp(dt * 24.0f, 0.0f, 1.0f);
        }
    } else {
        const float cur_target = range_to_t<T>(this->get(), min_, max_);
        if (std::abs(cur_target - anim_t_) < 0.0005f) {
            anim_t_ = cur_target;
        } else {
            anim_t_ += (cur_target - anim_t_) * std::clamp(dt * 24.0f, 0.0f, 1.0f);
        }
    }

    fill_percent_ = anim_t_;
}

// BINARY: c_slider__render_0 @ 0x14004BB80
// 1. Label text at (bounds.x, bounds.y + height/2), color 0xFFBEBEBE
// 2. Value text right-aligned, color 0xFFAAAAAA
// 3. Track rect at this+336, filled with 0xFF19191D (NOT 0xFF1F1F1D)
// 4. Accent fill portion
// 5. Border
template <typename T>
void c_slider<T>::render(Renderer::c_renderer_core& r)
{
    if (!this->visible_) return;
    const auto& cs = style::current();
    const auto& sz = style::sizes();
    // Sliders take the shared radius DAMPED — the full Factor turned
    // the 8-px track into a pill (user 2026-07-17 "sliders shouldn't
    // be this affected, a bit less"). ~⅓ of track height caps it at
    // a subtle curve while leaving the default Factor 2.75 (radius 2)
    // untouched.
    const float radius = std::min(
        style::rounding().of(style::rounding().slider),
        std::floor(std::max(sz.slider_track_h, 8.0f) * 0.35f));
    const auto br = rrect(this->bounds_);

    // Label + value text — sit ABOVE the track with a clear gap (user
    // 2026-06-10: the label sat too low, almost touching the track).
    if (font_) {
        const float baseline_y = br.y - 2.0f;
        r.render_text(*font_, Renderer::vec2{br.x, baseline_y},
                      cs.text, this->label_.c_str());

        const auto value_text = format_value(this->get()) + suffix_;
        const float val_w = font_->measure_text(value_text);

        // VALUE TEXT COLOUR — per c_slider__render_0 @ 0x14004BB80
        // (line 0x14004C70C reads g_style_accent_color into the value
        // text call). User 2026-06-01: the dynamic-accent-at-zero
        // behaviour is for the "Min damage" slider only — every
        // other slider keeps the dim value text. Sliders opt in via
        // `set_accent_at_min(true)`.
        const bool dynamic = accent_at_min_ && (this->get() == min_);
        const Renderer::color val_col = dynamic ? cs.accent : cs.text_dim;
        r.render_text(*font_,
                      Renderer::vec2{br.x + br.w - val_w, baseline_y},
                      val_col, value_text.c_str());
    }

    // Track height: binary uses g_style_slider_height (which itself comes
    // from style_scale_value @ 0x140001390). Use the configured value; a
    // floor of 8 px gives a chunkier bar than the prior 6 px.
    const float track_h = std::max(sz.slider_track_h, 8.0f);
    Renderer::rect bar{br.x, br.y + br.h - track_h, br.w, track_h};
    // Snap the track to integer pixels so the track fill, the clipped
    // accent, and the 1-px border all share the SAME edges. On high DPI
    // the fractional bar made the (snapped) fill and the (unsnapped)
    // accent clip disagree by ~1px at the bottom, breaking the border
    // (user 2026-06-10).
    {
        const float L = std::round(bar.x);
        const float Tp = std::round(bar.y);
        const float R = std::round(bar.x + bar.w);
        const float B = std::round(bar.y + bar.h);
        bar = Renderer::rect{L, Tp, R - L, B - Tp};
    }

    // Track bg per c_slider__render_0 @ 0x14004BB80 line 0x14004BCD8:
    //   lerp_color(0xFF202024, 0xFF232328, hover_anim)
    // (decimal -15132387 = 0xFF202024, -14474456 = 0xFF232328).
    // NO accent-tinted border on hover — binary keeps the dark outline.
    const Renderer::color track_idle  {0x20, 0x20, 0x24, 0xFF};
    const Renderer::color track_hover {0x23, 0x23, 0x28, 0xFF};
    const Renderer::color track_bg =
        Renderer::color::lerp(track_idle, track_hover,
                              std::clamp(hover_anim_, 0.0f, 1.0f));
    r.render_filled_rounded_rect(bar, track_bg, radius);

    // Filled accent portion (the value-bar) — smoothly animated on click,
    // instant on mouse drag
    const float t  = std::clamp(anim_t_, 0.0f, 1.0f);
    const float fw = bar.w * t;
    if (fw > 0) {
        const float rR = std::clamp(radius - (bar.w - fw), 0.0f, radius);
        const float fill_radii[4] = {radius, rR, rR, radius};
        r.render_filled_rounded_rect_ex(
            Renderer::rect{bar.x, bar.y, fw, bar.h},
            cs.accent, fill_radii);
    }

    // Border — outline stays at cs.outline (binary doesn't switch to accent).
    r.render_rounded_rect(bar, cs.outline, radius, 1.0f);
}

// Explicit instantiations — matches the four RTTI variants
template class c_slider<int>;
template class c_slider<float>;
template class c_slider<double>;
template class c_slider<long long>;

} // namespace GUI
