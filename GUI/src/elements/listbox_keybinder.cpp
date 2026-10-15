// =============================================================================
// listbox_keybinder.cpp — c_listbox + c_keybinder
//
// Binary verified — HOW_IT_WORKS.md §20, §21:
//   c_listbox render: 0x14004F9D0
//   c_keybinder render: 0x140048B10
// =============================================================================

#include <GUI/elements/c_listbox.hpp>
#include <GUI/elements/c_listbox_item.hpp>
#include <GUI/elements/c_keybinder.hpp>

#include <GUI/input/cursor.hpp>
#include <GUI/input/input_state.hpp>
#include <GUI/input/virtual_key_names.hpp>
#include <GUI/style/colorscheme.hpp>
#include <GUI/style/element_sizes.hpp>
#include <GUI/style/rounding.hpp>

#include <Renderer/c_renderer_core.hpp>
#include <Renderer/font.hpp>

#include <algorithm>
#include <cmath>

#include <Windows.h>

namespace GUI {

namespace {

inline Renderer::rect rrect(c_base_element::rect_t r) {
    return Renderer::rect{r.x, r.y, r.w, r.h};
}

const char* mode_label(keybinder_mode_e m) {
    // Binary order via create_context_menu_c_keybinder @ 0x1400485A0:
    //   0=Always, 1=Toggle, 2=Hold, 3=None
    switch (m) {
        case keybinder_mode_e::always: return "Always";
        case keybinder_mode_e::toggle: return "Toggle";
        case keybinder_mode_e::hold:   return "Hold";
        case keybinder_mode_e::none:   return "None";
    }
    return "?";
}

bool is_capturable(input::virtual_key_t k) {
    if (k == 0)         return false;
    if (k == VK_ESCAPE) return false;
    return true;
}

} // anon

// ============================================================================
// c_listbox
// ============================================================================

// BINARY: c_listbox__update @ 0x14004D620
void c_listbox::update(float dt)
{
    auto& in = input::current();
    auto& cur = input::cursor();
    const auto& sz = style::sizes();

    // Row height matches dropdown for comfortable text padding
    const float row_h = sz.dropdown_extra_h;  // 24.0f

    // Position from parent (binary: same pattern as c_dropdown/c_button).
    // bounds.h = label row + n_visible * row_h + 2px border slack.
    const float label_band = label_.empty() ? 0.0f : sz.checkbox_height;
    if (inline_layout()) {
        const float default_h = label_band + show_n_items_ * row_h + 2.0f;
        if (bounds_.h < default_h) bounds_.h = default_h;
    } else if (auto p = parent_.lock()) {
        const float parent_next_y = p->next_y();
        bounds_.x = p->bounds().x + sz.text_margin;
        bounds_.y = parent_next_y;
        bounds_.w = p->bounds().w - 2.0f * sz.text_margin;
        const float default_h = label_band + show_n_items_ * row_h + 2.0f;
        if (bounds_.h <= 0.0f) bounds_.h = default_h;
        extra_height_ = sz.button_height;
        p->set_next_y(parent_next_y + bounds_.h + extra_height_);
    }

    // Box area (excluding label row at top)
    const float label_h = label_band;
    const Renderer::rect br{
        bounds_.x, bounds_.y + label_h,
        bounds_.w, std::max(0.0f, bounds_.h - label_h)
    };

    // Modal guard
    bool owned_by_ancestor = false;
    for (auto a = parent_.lock(); a; a = a->parent().lock()) {
        if (static_cast<const void*>(a.get()) == in.modal_owner) {
            owned_by_ancestor = true;
            break;
        }
    }
    if (in.input_blocked_by_other(this) && !owned_by_ancestor) {
        hovered_   = false;
        hover_idx_ = -1;
        return;
    }

    hovered_ = br.contains(in.mouse_pos);
    hover_idx_ = -1;

    const float inner_h = std::max(0.0f, br.h - 2.0f);
    const float content_h = options_.size() * row_h;
    const float max_scroll = std::max(0.0f, content_h - inner_h);

    if (hovered_) {
        cur.set(input::cursor_type::hand);
        // Determine hovered row
        const float local_y = in.mouse_pos.y - br.y - 1.0f + scroll_y_;
        hover_idx_ = static_cast<int>(std::floor(local_y / row_h));
        if (hover_idx_ < 0 || hover_idx_ >= static_cast<int>(options_.size())) {
            hover_idx_ = -1;
        }
    }

    if (in.mouse_pressed[input::mb_left] && hovered_ && hover_idx_ >= 0) {
        in.consume_mouse_press();
        if (!required_ && get() == hover_idx_) {
            set(-1);
        } else {
            set(hover_idx_);
        }
    }

    // Wheel scroll — consumes wheel so parent page doesn't scroll simultaneously
    if (hovered_ && in.wheel_delta != 0.0f) {
        if (max_scroll > 0.0f) {
            target_scroll_y_ -= in.wheel_delta * 2.0f * row_h;
            target_scroll_y_ = std::clamp(target_scroll_y_, 0.0f, max_scroll);
        }
        in.wheel_delta = 0.0f; // Consume wheel delta
    }

    // Smooth scroll
    target_scroll_y_ = std::clamp(target_scroll_y_, 0.0f, max_scroll);
    scroll_y_ += (target_scroll_y_ - scroll_y_) * std::min(1.0f, dt * 20.0f);

    // Per-row hover fade
    if (row_anim_.size() != options_.size()) {
        row_anim_.assign(options_.size(), 0.0f);
    }
    const float rate = 14.0f * style::anim_speed();
    for (std::size_t i = 0; i < row_anim_.size(); ++i) {
        const float target = (static_cast<int>(i) == hover_idx_) ? 1.0f : 0.0f;
        row_anim_[i] += (target - row_anim_[i]) * std::min(1.0f, dt * rate);
    }
}

// BINARY: c_listbox__render_items @ 0x14004F9D0
void c_listbox::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    const auto& cs = style::current();
    const auto& sz = style::sizes();
    const float radius = style::rounding().of(style::rounding().listbox);

    // Row height matches dropdown
    const float row_h = sz.dropdown_extra_h;  // 24.0f

    // bounds_ covers both the label row at top and the listbox proper
    const float label_h = label_.empty() ? 0.0f : sz.checkbox_height;
    const Renderer::rect br{
        bounds_.x,
        bounds_.y + label_h,
        bounds_.w,
        std::max(0.0f, bounds_.h - label_h)
    };

    // Label
    if (font_ && !label_.empty()) {
        r.render_text(*font_,
                      Renderer::vec2{bounds_.x, bounds_.y + 2.0f},
                      cs.text, label_.c_str());
    }

    // Box frame
    r.render_filled_rounded_rect(br, cs.body_bg, radius);
    r.render_rounded_rect(br, cs.outline, radius, 1.0f);

    if (options_.empty()) {
        if (font_) {
            const float by = br.y + (br.h - font_->pixel_height()) * 0.5f;
            const float bx = br.x + (br.w - font_->measure_text("Empty")) * 0.5f;
            r.render_text(*font_, Renderer::vec2{bx, by}, cs.text_dim, "Empty");
        }
        return;
    }

    const Renderer::rect inner{br.x + 1.0f, br.y + 1.0f, br.w - 2.0f, br.h - 2.0f};
    r.push_clip(inner);

    const int sel = get();
    const float row_top = inner.y - scroll_y_;

    const Renderer::color row_bg_idle {0x19, 0x19, 0x1D, 0xFF};
    const Renderer::color row_bg_hover{0x23, 0x23, 0x28, 0xFF};

    auto fill_row = [&](Renderer::rect row, Renderer::color col) {
        const bool at_top    = radius > 0.0f && row.y <= inner.y + 0.5f;
        const bool at_bottom = radius > 0.0f && (row.y + row.h >= inner.y + inner.h - 0.5f);
        if (at_top && at_bottom) {
            const float r4[4] = {radius, radius, radius, radius};
            r.render_filled_rounded_rect_ex(row, col, r4);
        } else if (at_top) {
            const float r4[4] = {radius, radius, 0.0f, 0.0f};
            r.render_filled_rounded_rect_ex(row, col, r4);
        } else if (at_bottom) {
            const float r4[4] = {0.0f, 0.0f, radius, radius};
            r.render_filled_rounded_rect_ex(row, col, r4);
        } else {
            r.render_filled_rect(row, col);
        }
    };

    for (std::size_t i = 0; i < options_.size(); ++i) {
        const Renderer::rect row{inner.x, row_top + i * row_h, inner.w, row_h};
        if (row.y + row.h < inner.y || row.y > inner.y + inner.h) continue;

        const bool is_sel = (static_cast<int>(i) == sel);
        const bool is_hov = (static_cast<int>(i) == hover_idx_);

        // Row background — animated hover fade
        const float ra = (i < row_anim_.size()) ? row_anim_[i] : 0.0f;
        fill_row(row, Renderer::color::lerp(row_bg_idle, row_bg_hover, ra));

        if (font_) {
            const float baseline = row.y + (row.h - font_->pixel_height()) * 0.5f;
            Renderer::color tc = cs.text;
            if (is_sel)      tc = cs.accent;
            else if (is_hov) tc = cs.title_text;
            r.render_text(*font_, Renderer::vec2{row.x + 14.0f, baseline},
                          tc, options_[i].c_str());
        }
    }

    // Scrollbar pill
    const float content_h = options_.size() * row_h;
    if (content_h > inner.h + 0.5f) {
        const float pad = 2.0f;
        const float track_h = inner.h - 2.0f * pad;
        const float th = std::max(10.0f, track_h * inner.h / content_h);
        const float max_s = content_h - inner.h;
        const float sr = std::clamp(scroll_y_ / max_s, 0.0f, 1.0f);
        const float ty = inner.y + pad + sr * (track_h - th);
        r.render_filled_rounded_rect(
            Renderer::rect{inner.x + inner.w - 4.0f, ty, 2.0f, th},
            Renderer::color{0x3E, 0x3E, 0x42, 0xFF}, 1.0f);
    }

    r.pop_clip();
}

// ============================================================================
// c_keybinder
// ============================================================================

// BINARY: c_keybinder slot4 layout @ 0x140049540
// Chip positioning:
//   chip.w = style.keybind_chip_width (+0x218) — 48
//   chip.h = style.keybind_chip_height (+0x220) — 20
//   chip.x = bounds.x + bounds.w - chip.w
//   chip.y = bounds.y
void c_keybinder::update(float dt)
{
    auto& in = input::current();
    auto& cur = input::cursor();
    const auto& sz = style::sizes();

    // Position from parent (binary: same pattern as c_dropdown/c_button).
    // Keybinder is a label-row widget with a small chip on the right —
    // bounds.h matches one row (checkbox_height = 20).
    if (auto p = parent_.lock()) {
        const float parent_next_y = p->next_y();
        bounds_.x = p->bounds().x + sz.text_margin;
        bounds_.y = parent_next_y;
        bounds_.w = p->bounds().w - 2.0f * sz.text_margin;
        bounds_.h = sz.checkbox_height;
        extra_height_ = sz.button_height;
        p->set_next_y(parent_next_y + bounds_.h + extra_height_);
    }

    const auto br = rrect(bounds_);

    // Chip dimensions from binary: label_spacing (48), checkbox_height (20)
    const float chip_w = sz.label_spacing;  // 48
    const float chip_h = sz.checkbox_height;  // 20

    chip_rect_.x = br.x + br.w - chip_w;
    chip_rect_.y = br.y + (br.h - chip_h) * 0.5f;
    chip_rect_.w = chip_w;
    chip_rect_.h = chip_h;

    // User 2026-06-01: chip click steals modal from peer popup
    // (dropdown/colorpicker close cleanly via the displacing
    // callback). User 2026-06-02: when a DIALOG owns the modal,
    // the keybinder must NOT steal — dialogs are absolute.
    const bool blocked = in.input_blocked_by_other(this);
    const Renderer::rect chip = rrect(chip_rect_);

    // Border accent tweens — EXACTLY the c_button model (user 2026-06-15:
    // "like dropdown, buttons and others"). hover_anim_ fades the gray
    // outline toward accent@0x80; press_anim_ ramps that toward FULL accent
    // the longer the mouse is held on the chip. pressed_ = chip held with
    // LMB down (and not blocked by a peer popup). Ticked every frame,
    // before the early-returns below, so the ramp/decay is uninterrupted.
    pressed_ = !blocked && chip.contains(in.mouse_pos)
               && in.mouse_down[input::mb_left];
    {
        const float hover_target = (!blocked && chip.contains(in.mouse_pos))
                                       ? 1.0f : 0.0f;
        const float press_target = pressed_ ? 1.0f : 0.0f;
        hover_anim_ += (hover_target - hover_anim_) * std::clamp(dt * 14.0f, 0.0f, 1.0f);
        press_anim_ += (press_target - press_anim_) * std::clamp(dt * 18.0f, 0.0f, 1.0f);
    }

    if (blocked) {
        hovered_ = false;
        ctx_open_ = false;
        capturing_ = false;
        return;
    }

    // Right-click on chip: open ctx menu, steal modal. Uses _raw so
    // the press survives a previous modal owner's consume.
    // User 2026-06-02: ctx menu lives to the RIGHT of the chip (was
    // below) with a 4-px gap. row_h 22 px x 4 rows = 88 px total height.
    if (chip.contains(in.mouse_pos) && in.mouse_pressed_raw[input::mb_right]) {
        ctx_open_ = true;
        ctx_rect_ = Renderer::rect{chip.x + chip.w + 4.0f, chip.y, 88.0f, 88.0f};
        in.claim_modal(this, [this]{ ctx_open_ = false; capturing_ = false; });
        in.consume_mouse_press(input::mb_right);
        hovered_ = true;
        return;
    }

    // Left-click on chip: enter capture, steal modal. _raw signal.
    if (chip.contains(in.mouse_pos) && in.mouse_pressed_raw[input::mb_left]) {
        capturing_     = true;
        capture_armed_ = false;
        in.claim_modal(this, [this]{ ctx_open_ = false; capturing_ = false; });
        in.consume_mouse_press();
        hovered_ = true;
        return;
    }

    // If we were ctx-open or capturing but lost the modal, close.
    if ((ctx_open_ || capturing_) && in.modal_owner != this) {
        ctx_open_ = false;
        capturing_ = false;
    }

    if (blocked) { hovered_ = false; return; }

    hovered_ = chip.contains(in.mouse_pos);
    if (hovered_) cur.set(input::cursor_type::hand);

    if (ctx_open_ || capturing_) {
        in.claim_modal(this, [this]{ ctx_open_ = false; capturing_ = false; });
    } else {
        in.release_modal(this);
    }

    // Trigger clicks (left/right) handled above with modal stealing.
    // What's left here is the popup-interior — the 4 mode rows in the
    // right-click context menu.

    if (ctx_open_) {
        const bool ctx_hovered = ctx_rect_.contains(in.mouse_pos);
        if (ctx_hovered) cur.set(input::cursor_type::hand);

        if (in.mouse_pressed[input::mb_left]) {
            const float row_h = 22.0f;
            auto state = bind();
            bool selected = false;
            for (int i = 0; i < 4; ++i) {
                Renderer::rect row{ctx_rect_.x, ctx_rect_.y + i * row_h, ctx_rect_.w, row_h};
                if (row.contains(in.mouse_pos)) {
                    switch (i) {
                        case 0: state.mode = keybinder_mode_e::always; break;
                        case 1: state.mode = keybinder_mode_e::toggle; break;
                        case 2: state.mode = keybinder_mode_e::hold;   break;
                        case 3: state.mode = keybinder_mode_e::none;   break;
                    }
                    set_bind(state);
                    selected = true;
                    break;
                }
            }
            ctx_open_ = false;
            in.release_modal(this);
            // Always consume — peer popup-triggers read
            // mouse_pressed_raw, so they still get the click.
            in.consume_mouse_press();
            if (selected) return;
        }
    }

    // Capture mode
    if (capturing_) {
        if (in.key_pressed[VK_ESCAPE]) {
            capturing_ = false;
        } else if (capture_armed_) {
            for (int k = 1; k < 256; ++k) {
                // Skip mouse button VKs (1=LBUTTON, 2=RBUTTON, 4=MBUTTON,
                // 5/6=X1/X2) so a click doesn't immediately resolve as
                // its own keybind.
                if (k == VK_LBUTTON || k == VK_RBUTTON || k == VK_CANCEL ||
                    k == VK_MBUTTON || k == VK_XBUTTON1 || k == VK_XBUTTON2)
                    continue;
                if (in.key_pressed[k] && is_capturable(static_cast<input::virtual_key_t>(k))) {
                    auto state = bind();
                    state.key = static_cast<input::virtual_key_t>(k);
                    // Default mode to "always" if it was uninitialised
                    // (the packed-0 state unpacks to mode=always already,
                    // but a fresh bind from a c_base_control with no
                    // state would not). Without this the bind would be
                    // saved but immediately read back as mode=none and
                    // never fire.
                    if (state.mode == keybinder_mode_e::none)
                        state.mode = keybinder_mode_e::always;
                    set_bind(state);
                    capturing_ = false;
                    break;
                }
            }
        }
        // After one frame of "armed = false", flip to armed so the
        // NEXT frame's key press is recognised.
        capture_armed_ = true;
    }

    // Toggle latch for toggle mode
    const auto b = bind();
    if (b.mode == keybinder_mode_e::toggle && b.key != 0) {
        if (in.key_pressed[b.key]) {
            toggle_latch_ = !toggle_latch_;
        }
    }
}

bool c_keybinder::active(const input::input_state& in) const
{
    const auto b = bind();
    if (b.mode == keybinder_mode_e::none) return false;
    if (b.key == 0 && b.mode != keybinder_mode_e::always) return false;
    switch (b.mode) {
        case keybinder_mode_e::always: return true;
        case keybinder_mode_e::toggle: return toggle_latch_;
        case keybinder_mode_e::hold:   return in.key_down[b.key];
        case keybinder_mode_e::none:   return false;
    }
    return false;
}

// BINARY: c_keybinder__render @ 0x140048B10
// 1. vt[+288] render_filled_rect(chip_rect, 0xFF232328, rounding)
// 2. vt[+296] render_rect_outline(chip_rect) — accent when listening
// 3. resolve VK name via style.vk_name_table[VK] (+0x5E0)
// 4. vt[+472] render_text(chip_center, vk_name,
//                         listening ? accent : 0xFFBEBEBE, ALIGN_CENTER)
// 5. if listening: show "[...]" prompt
void c_keybinder::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    const auto& cs = style::current();
    const float radius = style::rounding().of(style::rounding().keybinder);
    const auto br = rrect(bounds_);

    // Label on the left — 0xFFBEBEBE
    if (font_) {
        const float baseline = br.y + (br.h - font_->pixel_height()) * 0.5f;
        r.render_text(*font_, Renderer::vec2{br.x, baseline}, cs.text, label_.c_str());
    }

    // Chip
    const Renderer::rect chip = rrect(chip_rect_);
    const auto b = bind();

    // Chip background — stays at body_bg in ALL states. User
    // 2026-06-01: when capturing a key, the chip interior must NOT
    // turn accent (was an accent tint at alpha 90). The accent
    // border + "..." text already communicate the listening state.
    r.render_filled_rounded_rect(chip, cs.body_bg, radius);

    // Border — IDENTICAL formula to c_button (simple_controls.cpp:341-343):
    //   idle              -> gray outline
    //   hover             -> accent @ 0x80 (lower alpha) via hover_anim_
    //   press-and-hold    -> accent alpha ramps toward FULL via press_anim_
    //
    // Clicking the chip flips capturing_ true on the SAME frame, so the
    // "listening" solid-accent cue must only apply once the mouse is
    // RELEASED (!pressed_). While the button is still held, fall through
    // to the animated ramp — otherwise capturing_ instantly paints solid
    // accent and the press animation is never seen (user: "it's instant").
    Renderer::color chip_border;
    if ((capturing_ || ctx_open_) && !pressed_) {
        chip_border = cs.accent;
    } else {
        const Renderer::color hover_color{cs.accent.r, cs.accent.g,
                                          cs.accent.b, 0x80};
        chip_border = Renderer::color::lerp(cs.outline, hover_color,
                                            std::clamp(hover_anim_, 0.0f, 1.0f));
        chip_border = Renderer::color::lerp(chip_border, cs.accent,
                                            std::clamp(press_anim_, 0.0f, 1.0f));
    }
    r.render_rounded_rect(chip, chip_border, radius, 1.0f);

    // Text inside chip
    if (font_) {
        const char* text;
        if (capturing_) {
            text = "...";
        } else if (b.key == 0) {
            text = "None";   // user 2026-05-31: unbound chip reads "None"
        } else {
            text = input::virtual_key_names[b.key].data();
        }
        const std::string s = text;
        const float text_w = font_->measure_text(s);
        const float tx = chip.x + (chip.w - text_w) * 0.5f;
        const float ty = chip.y + (chip.h - font_->pixel_height()) * 0.5f;

        // Accent text ONLY while capturing.
        r.render_text(*font_, Renderer::vec2{tx, ty},
                      capturing_ ? cs.accent : cs.text, s.c_str());
    }

    // Mode label is NOT rendered below the chip in the steady state.
    // Per user: "only when right click with the modes its supposed to
    // render the mode" — the right-click popup (drawn below) is the
    // only place the mode is visible.

    // Context menu (overlay)
    if (ctx_open_) {
        r.push_layer(Renderer::layer_e::overlay);

        // User 2026-06-02: ctx menu is a single solid panel with a
        // border — NO per-row hover/press background flashes, NO
        // dropdown/listbox palette. Only the TEXT colour reflects
        // state:
        //   - selected mode  -> accent
        //   - hovered row    -> title_text (brighter)
        //   - default        -> cs.text
        r.render_filled_rounded_rect(ctx_rect_, cs.body_bg, style::rounding().of(style::rounding().context_menu));
        r.render_rounded_rect(ctx_rect_, cs.outline, style::rounding().of(style::rounding().context_menu), 1.0f);

        // Labels match the binary's create_context_menu_c_keybinder
        // strings @ 0x1401F668C / 0x1401F6694 / 0x1401F669C and the
        // 4th item ("None") which is allocated inline.
        static const char* items[4] = {"Always", "Toggle", "Hold", "None"};
        const float row_h = 22.0f;
        for (int i = 0; i < 4; ++i) {
            Renderer::rect row{ctx_rect_.x, ctx_rect_.y + i * row_h, ctx_rect_.w, row_h};
            const bool hov = row.contains(input::current().mouse_pos);
            if (font_) {
                const float ty = row.y + (row.h - font_->pixel_height()) * 0.5f;
                Renderer::color tc = cs.text;
                if (static_cast<int>(b.mode) == i) tc = cs.accent;
                else if (hov)                     tc = cs.title_text;
                r.render_text(*font_, Renderer::vec2{row.x + 8, ty}, tc, items[i]);
            }
        }

        r.pop_layer();
    }
}

} // namespace GUI
