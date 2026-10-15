// =============================================================================
// c_colorpicker.cpp — swatch + popup with HSV picker + optional alpha
//
// Binary layout verified — HOW_IT_WORKS.md §19:
//   Swatch render: 0x140042FD0
//   Popup render: 0x140041370
//   Layout: 0x1400448A0
// =============================================================================

#include <GUI/elements/c_colorpicker.hpp>

#include <GUI/input/cursor.hpp>
#include <GUI/input/input_state.hpp>
#include <GUI/style/colorscheme.hpp>
#include <GUI/style/element_sizes.hpp>
#include <GUI/style/rounding.hpp>

#include <Renderer/c_renderer_core.hpp>
#include <Renderer/font.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace GUI {

namespace {

inline Renderer::rect rrect(c_base_element::rect_t r) {
    return Renderer::rect{r.x, r.y, r.w, r.h};
}

// HSV (h in [0,360), s/v in [0,1]) -> sRGB — from binary @ 0x14000E850
Renderer::color hsv_to_rgb(float h, float s, float v, std::uint8_t a = 255) {
    h = std::fmod(h, 360.0f); if (h < 0) h += 360.0f;
    const float c = v * s;
    const float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float r = 0, g = 0, b = 0;
    if      (h <  60) { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x;        b = c; }
    else              { r = c;        b = x; }
    return Renderer::color{
        static_cast<std::uint8_t>((r + m) * 255.0f),
        static_cast<std::uint8_t>((g + m) * 255.0f),
        static_cast<std::uint8_t>((b + m) * 255.0f), a};
}

// sRGB -> HSV — from binary @ 0x14000E470
void rgb_to_hsv(Renderer::color c, float& h, float& s, float& v) {
    const float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
    const float maxv = std::max({r, g, b});
    const float minv = std::min({r, g, b});
    const float d = maxv - minv;
    v = maxv;
    s = (maxv > 0) ? (d / maxv) : 0;
    if (d == 0)              h = 0;
    else if (maxv == r)      h = 60.0f * std::fmod((g - b) / d + 6.0f, 6.0f);
    else if (maxv == g)      h = 60.0f * ((b - r) / d + 2.0f);
    else                     h = 60.0f * ((r - g) / d + 4.0f);
}

std::string color_to_hex(Renderer::color c) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
    return buf;
}

bool parse_hex_color(const char* text, Renderer::color& out) {
    if (!text) return false;
    while (*text == ' ' || *text == '\t') ++text;
    if (*text == '#') ++text;
    unsigned int v = 0;
    int n = 0;
#ifdef _MSC_VER
    if (sscanf_s(text, "%x%n", &v, &n) != 1) return false;
#else
    if (std::sscanf(text, "%x%n", &v, &n) != 1) return false;
#endif
    if (n == 6) {
        out = Renderer::color{static_cast<std::uint8_t>((v >> 16) & 0xFF),
                              static_cast<std::uint8_t>((v >> 8) & 0xFF),
                              static_cast<std::uint8_t>(v & 0xFF),
                              out.a};
        return true;
    }
    if (n == 8) {
        out = Renderer::color{static_cast<std::uint8_t>((v >> 24) & 0xFF),
                              static_cast<std::uint8_t>((v >> 16) & 0xFF),
                              static_cast<std::uint8_t>((v >> 8) & 0xFF),
                              static_cast<std::uint8_t>(v & 0xFF)};
        return true;
    }
    return false;
}

void copy_text_to_clipboard(const std::string& s) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, s.size() + 1);
    if (mem) {
        void* dst = GlobalLock(mem);
        if (dst) {
            std::memcpy(dst, s.c_str(), s.size() + 1);
            GlobalUnlock(mem);
            SetClipboardData(CF_TEXT, mem);
            mem = nullptr;
        }
        if (mem) GlobalFree(mem);
    }
    CloseClipboard();
#else
    (void)s;
#endif
}

std::string read_text_from_clipboard() {
#ifdef _WIN32
    std::string out;
    if (!OpenClipboard(nullptr)) return out;
    HANDLE h = GetClipboardData(CF_TEXT);
    if (h) {
        if (const char* p = static_cast<const char*>(GlobalLock(h))) {
            out = p;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return out;
#else
    return {};
#endif
}

} // anon

// BINARY: c_colorpicker slot4 layout @ 0x1400448A0
// Swatch positioning:
//   swatch.x = bounds.x + bounds.w - checkbox_size - 1
//   swatch.y = bounds.y + bounds.h/2 - checkbox_size/2
//   swatch.w = checkbox_size (12)
//   swatch.h = checkbox_size (12)
void c_colorpicker::set_swatch_rect(const Renderer::rect& rc)
{
    swatch_rect_ = rect_t{rc.x, rc.y, rc.w, rc.h};
    swatch_rect_external_ = true;
}

// Single source of truth for the popup geometry — called from both
// popup-open paths. Layout:
//   SV plane → hue bar → alpha bar
//   → [ #hex | Copy | Paste ]        (boxed row)
void c_colorpicker::layout_popup(const Renderer::rect& swatch)
{
    const auto& sz = style::sizes();
    const float gk      = style::gui_scale();
    const float popup_w = sz.colorpicker_popup_w;       // scaled
    const float margin  = sz.colorpicker_popup_margin;  // 8
    const float gap     = sz.colorpicker_slider_gap;    // 8
    const float bar_h   = 10.0f * gk;
    const float sv_h    = 120.0f * gk;
    const float field_h = 22.0f * gk;   // hex/copy/paste boxes

    const float popup_h = margin + sv_h
                        + gap + bar_h                          // hue
                        + (has_alpha_ ? gap + bar_h : 0.0f)    // alpha
                        + gap + field_h                        // hex row
                        + margin;

    popup_rect_.x = swatch.x + swatch.w - popup_w;
    popup_rect_.y = swatch.y + swatch.h + 4.0f;
    popup_rect_.w = popup_w;
    popup_rect_.h = popup_h;

    sv_rect_.x = popup_rect_.x + margin;
    sv_rect_.y = popup_rect_.y + margin;
    sv_rect_.w = popup_w - 2.0f * margin;
    sv_rect_.h = sv_h;

    // Stacked bars, FULL WIDTH, one under the other.
    hue_rect_.x = sv_rect_.x;
    hue_rect_.y = sv_rect_.y + sv_rect_.h + gap;
    hue_rect_.w = sv_rect_.w;
    hue_rect_.h = bar_h;

    if (has_alpha_) {
        alpha_rect_.x = sv_rect_.x;
        alpha_rect_.y = hue_rect_.y + hue_rect_.h + gap;
        alpha_rect_.w = sv_rect_.w;
        alpha_rect_.h = bar_h;
    } else {
        alpha_rect_ = {};
    }

    // Row: hex field (left) + Copy + Paste (right).
    const float row_y = (has_alpha_ ? alpha_rect_.y + alpha_rect_.h
                                    : hue_rect_.y + hue_rect_.h) + gap;
    const float btn_w   = 37.0f * gk;   // measured: Copy/Paste ≈ 37 base
    const float btn_gap = 6.0f * gk;
    const float hex_w   = sv_rect_.w - 2.0f * (btn_w + btn_gap);
    hex_rect_   = rect_t{sv_rect_.x, row_y, hex_w, field_h};
    copy_rect_  = rect_t{sv_rect_.x + hex_w + btn_gap, row_y, btn_w, field_h};
    paste_rect_ = rect_t{copy_rect_.x + btn_w + btn_gap, row_y, btn_w, field_h};
}

// Parse the in-progress inline edit into the bound colour and clear focus.
void c_colorpicker::commit_edit()
{
    if (edit_field_ != edit_field_t::hex) return;
    auto c = color();
    Renderer::color parsed = c;
    if (parse_hex_color(edit_buf_.c_str(), parsed)) c = parsed;
    set_color(c);
    edit_field_ = edit_field_t::none;
    edit_buf_.clear();
    edit_caret_ = 0;
}

void c_colorpicker::update(float dt)
{
    auto& in = input::current();
    auto& cur = input::cursor();
    const auto& sz = style::sizes();

    // Position from parent (binary: same pattern as c_dropdown/c_button).
    // When inline_layout_ is true the host already set our bounds and
    // takes care of advancing the parent's next_y itself (used by
    // c_esp_row so the swatch can sit in a custom slot rect).
    if (auto p = parent_.lock(); p && !inline_layout_) {
        const float parent_next_y = p->next_y();
        bounds_.x = p->bounds().x + sz.text_margin;
        bounds_.y = parent_next_y;
        bounds_.w = p->bounds().w - 2.0f * sz.text_margin;
        bounds_.h = sz.checkbox_height;
        extra_height_ = sz.button_height;
        p->set_next_y(parent_next_y + bounds_.h + extra_height_);
    } else if (auto p = parent_.lock(); p && inline_layout_) {
        // Honour the bounds the host set externally; pull only x/w/y
        // from the parent's currently-stamped slot rect.
        bounds_.x = p->bounds().x;
        bounds_.y = p->next_y();
        bounds_.w = p->bounds().w;
        bounds_.h = sz.checkbox_height;
    }

    const auto br = rrect(bounds_);

    // Swatch rect — binary uses checkbox_size (12) for swatch dimensions.
    // When c_label has already stamped the swatch rect into us via
    // set_swatch_rect() (adornment pass, mirrors binary
    // c_label__render writing into cp+396), keep that rect verbatim.
    const float swatch_size = sz.checkbox_size;  // 12
    if (!swatch_rect_external_) {
        swatch_rect_.x = br.x + br.w - swatch_size - 1.0f;
        swatch_rect_.y = br.y + br.h * 0.5f - swatch_size * 0.5f;
        swatch_rect_.w = swatch_size;
        swatch_rect_.h = swatch_size;
    }

    const Renderer::rect swatch = rrect(swatch_rect_);

    // User 2026-06-01: swatch click must work even when a peer popup
    // owns the modal — opening this picker auto-closes the peer via
    // claim_modal(this, close_fn). Only popup-interior interactions
    // (drags / inner clicks) are gated by the block.
    // User 2026-06-02: when blocked, freeze the swatch completely so
    // the swatch border doesn't accent on hover and the click can't
    // open the picker — popup will close via its own outside-click
    // handler, then the user clicks again to open this one.
    const bool blocked = in.input_blocked_by_other(this);
    if (blocked) {
        hovered_       = false;
        swatch_pressed_= false;
        hover_anim_    = 0.0f;
        press_anim_    = 0.0f;
        if (popup_open_ && in.modal_owner != this) {
            commit_edit();
            popup_open_ = false;
            drag_target_ = drag_target_t::none;
        }
        return;
    }

    // Swatch trigger click — toggle popup, steal modal. Uses
    // mouse_pressed_raw so a press the previous modal owner already
    // consumed is still seen here.
    if (swatch.contains(in.mouse_pos) && in.mouse_pressed_raw[input::mb_left]) {
        const bool was_open = popup_open_;
        popup_open_ = !popup_open_;
        swatch_pressed_ = true;
        in.consume_mouse_press();
        if (popup_open_) {
            in.claim_modal(this, [this]{
                popup_open_ = false;
                drag_target_ = drag_target_t::none;
            });
        } else {
            in.release_modal(this);
            commit_edit();   // closing via the swatch flushes any field edit
        }
        hovered_ = swatch.contains(in.mouse_pos);
        if (hovered_) cur.set(input::cursor_type::hand);
        // First frame after opening: lay out the popup geometry now so
        // render() has valid rects. SINGLE source of truth (layout_popup)
        // shared with the mouse-pressed open path below.
        if (popup_open_ && !was_open) {
            layout_popup(swatch);
        }
        return;
    }

    // If we were open but lost the modal to a peer, close.
    if (popup_open_ && in.modal_owner != this) {
        commit_edit();
        popup_open_ = false;
        drag_target_ = drag_target_t::none;
    }

    if (blocked) { hovered_ = false; return; }

    hovered_ = swatch.contains(in.mouse_pos);
    if (hovered_) cur.set(input::cursor_type::hand);

    if (popup_open_) {
        // Refresh claim so the close-callback stays bound.
        in.claim_modal(this, [this]{
            popup_open_ = false;
            drag_target_ = drag_target_t::none;
        });
    } else {
        in.release_modal(this);
    }

    // Sync HSV state from the bound color ONLY when it changed
    // EXTERNALLY (config load, paste from a peer, builder write).
    // The old unconditional per-frame sync destroyed drag state:
    // RGB->HSV is lossy at sat/val 0, so dragging the hue bar on a
    // white/black color computed the same RGB back, the sync reset
    // hue_ to 0, and the slider snapped back while the color stayed
    // white. HSV is authoritative while the popup is interacting;
    // last_synced_color_ is refreshed at the end of update() after
    // any internal set_color() write.
    if (drag_target_ == drag_target_t::none &&
        color().packed() != last_synced_color_.packed()) {
        float h, s, v;
        rgb_to_hsv(color(), h, s, v);
        hue_ = h; sat_ = s; val_ = v;
    }

    // Swatch hover/press anim — matches checkbox/button feel.
    const float hover_target = hovered_ ? 1.0f : 0.0f;
    const float press_target = swatch_pressed_ ? 1.0f : 0.0f;
    hover_anim_ += (hover_target - hover_anim_) * std::clamp(dt * 14.0f, 0.0f, 1.0f);
    press_anim_ += (press_target - press_anim_) * std::clamp(dt * 18.0f, 0.0f, 1.0f);

    // Copy / Paste button anims — hover fades quickly; the press ramp is
    // SLOW so holding longer pushes the accent border alpha higher.
    if (popup_open_) {
        const Renderer::rect copy_r  = rrect(copy_rect_);
        const Renderer::rect paste_r = rrect(paste_rect_);
        const Renderer::rect hex_r   = rrect(hex_rect_);
        const bool copy_hov  = copy_r.contains(in.mouse_pos);
        const bool paste_hov = paste_r.contains(in.mouse_pos);
        const bool hex_hov   = hex_r.contains(in.mouse_pos);
        const bool down = in.mouse_down[input::mb_left];
        copy_hover_anim_  += ((copy_hov  ? 1.0f : 0.0f) - copy_hover_anim_)  * std::clamp(dt * 14.0f, 0.0f, 1.0f);
        paste_hover_anim_ += ((paste_hov ? 1.0f : 0.0f) - paste_hover_anim_) * std::clamp(dt * 14.0f, 0.0f, 1.0f);
        copy_press_anim_  += (((copy_hov  && down) ? 1.0f : 0.0f) - copy_press_anim_)  * std::clamp(dt * 5.0f, 0.0f, 1.0f);
        paste_press_anim_ += (((paste_hov && down) ? 1.0f : 0.0f) - paste_press_anim_) * std::clamp(dt * 5.0f, 0.0f, 1.0f);
        field_hover_anim_ += ((hex_hov ? 1.0f : 0.0f) - field_hover_anim_) * std::clamp(dt * 14.0f, 0.0f, 1.0f);
    } else {
        copy_hover_anim_ = copy_press_anim_ = 0.0f;
        paste_hover_anim_ = paste_press_anim_ = 0.0f;
        field_hover_anim_ = 0.0f;
    }

    if (in.mouse_pressed[input::mb_left]) {
        if (hovered_) {
            swatch_pressed_ = true;
            popup_open_ = !popup_open_;
            in.consume_mouse_press();

            if (popup_open_) {
                layout_popup(swatch);
            } else {
                drag_target_ = drag_target_t::none;
                commit_edit();   // closing flushes any field edit
            }
        } else if (popup_open_) {
            const Renderer::rect popup_r = rrect(popup_rect_);
            const Renderer::rect sv_r    = rrect(sv_rect_);
            const Renderer::rect hue_r   = rrect(hue_rect_);
            const Renderer::rect alpha_r = rrect(alpha_rect_);
            const Renderer::rect copy_r  = rrect(copy_rect_);
            const Renderer::rect paste_r = rrect(paste_rect_);
            const Renderer::rect hex_r   = rrect(hex_rect_);
            const auto a = color().a;

            // Focus hex field for inline editing
            auto begin_edit = [&](edit_field_t f) {
                if (edit_field_ != f) commit_edit();
                edit_field_ = f;
                const auto cc = color();
                if (f == edit_field_t::hex) {
                    edit_buf_ = color_to_hex(cc).substr(0, 7);  // "#RRGGBB"
                }
                edit_caret_ = static_cast<int>(edit_buf_.size());
                edit_sel_start_ = edit_sel_end_ = edit_caret_;
                edit_blink_ = 0.0f;
            };

            auto get_hex_caret_at_x = [&](float mx, Renderer::rect hr) -> int {
                if (!font_) return 0;
                const float tw = font_->measure_text(edit_buf_);
                const float tx = hr.x + (hr.w - tw) * 0.5f;
                const float local_x = mx - tx;
                float w = 0;
                int c_idx = 0;
                for (std::size_t i = 0; i < edit_buf_.size(); ++i) {
                    const float cw = font_->measure_text(edit_buf_.substr(i, 1));
                    if (w + cw * 0.5f > local_x) break;
                    w += cw;
                    c_idx = static_cast<int>(i + 1);
                }
                return c_idx;
            };

            bool field_hit = false;
            if (hex_r.contains(in.mouse_pos)) {
                begin_edit(edit_field_t::hex);
                hex_dragging_ = true;
                edit_caret_ = get_hex_caret_at_x(in.mouse_pos.x, hex_r);
                edit_sel_start_ = edit_sel_end_ = edit_caret_;
                field_hit = true;
                in.consume_mouse_press();
            }

            if (field_hit) {
                // already handled
            } else if (sv_r.contains(in.mouse_pos)) {
                commit_edit();
                drag_target_ = drag_target_t::sv;
                sat_ = (in.mouse_pos.x - sv_r.x) / sv_r.w;
                val_ = 1.0f - (in.mouse_pos.y - sv_r.y) / sv_r.h;
                set_color(hsv_to_rgb(hue_, std::clamp(sat_, 0.0f, 1.0f),
                                     std::clamp(val_, 0.0f, 1.0f), a));
                in.consume_mouse_press();
            } else if (hue_r.contains(in.mouse_pos)) {
                commit_edit();
                drag_target_ = drag_target_t::hue;
                hue_ = ((in.mouse_pos.x - hue_r.x) / hue_r.w) * 360.0f;
                set_color(hsv_to_rgb(std::clamp(hue_, 0.0f, 360.0f), sat_, val_, a));
                in.consume_mouse_press();
            } else if (has_alpha_ && alpha_r.contains(in.mouse_pos)) {
                commit_edit();
                drag_target_ = drag_target_t::alpha;
                const float t = std::clamp((in.mouse_pos.x - alpha_r.x) / alpha_r.w, 0.0f, 1.0f);
                auto c = color();
                c.a = static_cast<std::uint8_t>(t * 255.0f);
                set_color(c);
                in.consume_mouse_press();
            } else if (copy_r.contains(in.mouse_pos)) {
                copy_text_to_clipboard(color_to_hex(color()));
                in.consume_mouse_press();
            } else if (paste_r.contains(in.mouse_pos)) {
                auto c = color();
                const std::string clip = read_text_from_clipboard();
                bool ok = parse_hex_color(clip.c_str(), c);
                if (!ok) {
                    int comps[4] = {-1, -1, -1, -1};
                    int idx = 0;
                    const char* p = clip.c_str();
                    while (*p && idx < 4) {
                        if (*p >= '0' && *p <= '9') {
                            int v = 0;
                            while (*p >= '0' && *p <= '9') { v = v*10 + (*p - '0'); ++p; }
                            if (v > 255) v = 255;
                            comps[idx++] = v;
                        } else { ++p; }
                    }
                    if (idx == 4) {
                        c.a = static_cast<std::uint8_t>(comps[0]);
                        c.r = static_cast<std::uint8_t>(comps[1]);
                        c.g = static_cast<std::uint8_t>(comps[2]);
                        c.b = static_cast<std::uint8_t>(comps[3]);
                        ok = true;
                    }
                }
                if (ok) set_color(c);
                in.consume_mouse_press();
            } else if (!popup_r.contains(in.mouse_pos)) {
                commit_edit();
                popup_open_ = false;
                drag_target_ = drag_target_t::none;
                in.release_modal(this);
                in.consume_mouse_press();
            } else {
                drag_target_ = drag_target_t::none;
                in.consume_mouse_press();
            }
        }
    }

    if (!in.mouse_down[input::mb_left]) swatch_pressed_ = false;

    if (popup_open_ && in.mouse_down[input::mb_left]) {
        const Renderer::rect sv_r = rrect(sv_rect_);
        const Renderer::rect hue_r = rrect(hue_rect_);
        const Renderer::rect alpha_r = rrect(alpha_rect_);
        const auto a = color().a;

        if (drag_target_ == drag_target_t::sv || (drag_target_ == drag_target_t::none && sv_r.contains(in.mouse_pos))) {
            drag_target_ = drag_target_t::sv;
            sat_ = std::clamp((in.mouse_pos.x - sv_r.x) / sv_r.w, 0.0f, 1.0f);
            val_ = std::clamp(1.0f - (in.mouse_pos.y - sv_r.y) / sv_r.h, 0.0f, 1.0f);
            set_color(hsv_to_rgb(hue_, sat_, val_, a));
        } else if (drag_target_ == drag_target_t::hue || (drag_target_ == drag_target_t::none && hue_r.contains(in.mouse_pos))) {
            drag_target_ = drag_target_t::hue;
            hue_ = std::clamp(((in.mouse_pos.x - hue_r.x) / hue_r.w) * 360.0f, 0.0f, 360.0f);
            set_color(hsv_to_rgb(hue_, sat_, val_, a));
        } else if (has_alpha_ && (drag_target_ == drag_target_t::alpha || (drag_target_ == drag_target_t::none && alpha_r.contains(in.mouse_pos)))) {
            drag_target_ = drag_target_t::alpha;
            const float t = std::clamp((in.mouse_pos.x - alpha_r.x) / alpha_r.w, 0.0f, 1.0f);
            auto c = color();
            c.a = static_cast<std::uint8_t>(t * 255.0f);
            set_color(c);
        }
    } else if (!in.mouse_down[input::mb_left]) {
        drag_target_ = drag_target_t::none;
    }

    // Drag-selection inside hex box
    if (hex_dragging_ && in.mouse_down[input::mb_left] && edit_field_ == edit_field_t::hex && font_) {
        const Renderer::rect hex_r = rrect(hex_rect_);
        const float tw = font_->measure_text(edit_buf_);
        const float tx = hex_r.x + (hex_r.w - tw) * 0.5f;
        const float local_x = in.mouse_pos.x - tx;
        float w = 0;
        int c_idx = 0;
        for (std::size_t i = 0; i < edit_buf_.size(); ++i) {
            const float cw = font_->measure_text(edit_buf_.substr(i, 1));
            if (w + cw * 0.5f > local_x) break;
            w += cw;
            c_idx = static_cast<int>(i + 1);
        }
        edit_caret_ = c_idx;
        edit_sel_end_ = edit_caret_;
    }
    if (!in.mouse_down[input::mb_left]) hex_dragging_ = false;

    // Update SV cursor drag animation
    const float target_sv_drag = (drag_target_ == drag_target_t::sv) ? 1.0f : 0.0f;
    sv_drag_anim_ += (target_sv_drag - sv_drag_anim_) * std::clamp(dt * 18.0f, 0.0f, 1.0f);

    // Inline field editing — accept typed characters / backspace / commit
    if (edit_field_ != edit_field_t::none) {
        edit_blink_ += dt * 6.28318f;
        if (edit_blink_ > 6.28318f * 2.0f) edit_blink_ -= 6.28318f * 2.0f;

        const int s_min = std::min(edit_sel_start_, edit_sel_end_);
        const int s_max = std::max(edit_sel_start_, edit_sel_end_);
        const bool has_sel = (s_min != s_max);

        auto push_undo = [&]() {
            undo_state_t st{edit_buf_, edit_caret_, edit_sel_start_, edit_sel_end_};
            if (edit_undo_stack_.empty() || edit_undo_stack_.back().text != st.text) {
                edit_undo_stack_.push_back(std::move(st));
                if (edit_undo_stack_.size() > 100) edit_undo_stack_.erase(edit_undo_stack_.begin());
                edit_redo_stack_.clear();
            }
        };

        auto erase_sel = [&]() -> bool {
            if (has_sel) {
                push_undo();
                edit_buf_.erase(s_min, s_max - s_min);
                edit_caret_ = s_min;
                edit_sel_start_ = edit_sel_end_ = edit_caret_;
                return true;
            }
            return false;
        };

        const bool ctrl  = in.key_down[VK_CONTROL] || ((GetKeyState(VK_CONTROL) & 0x8000) != 0);
        const bool shift = in.key_down[VK_SHIFT]   || ((GetKeyState(VK_SHIFT) & 0x8000) != 0);

        const bool key_a = in.key_pressed['A'] || in.key_pressed['a'];
        const bool key_c = in.key_pressed['C'] || in.key_pressed['c'];
        const bool key_x = in.key_pressed['X'] || in.key_pressed['x'];
        const bool key_v = in.key_pressed['V'] || in.key_pressed['v'];
        const bool key_z = in.key_pressed['Z'] || in.key_pressed['z'];
        const bool key_y = in.key_pressed['Y'] || in.key_pressed['y'];

        // Ctrl + Z: Undo
        if (ctrl && !shift && key_z) {
            if (!edit_undo_stack_.empty()) {
                edit_redo_stack_.push_back(undo_state_t{edit_buf_, edit_caret_, edit_sel_start_, edit_sel_end_});
                auto prev = edit_undo_stack_.back();
                edit_undo_stack_.pop_back();
                edit_buf_ = std::move(prev.text);
                edit_caret_ = prev.caret;
                edit_sel_start_ = prev.sel_start;
                edit_sel_end_ = prev.sel_end;
                commit_edit();
                edit_blink_ = 0.0f;
            }
        }
        // Ctrl + Y or Ctrl + Shift + Z: Redo
        else if ((ctrl && key_y) || (ctrl && shift && key_z)) {
            if (!edit_redo_stack_.empty()) {
                edit_undo_stack_.push_back(undo_state_t{edit_buf_, edit_caret_, edit_sel_start_, edit_sel_end_});
                auto next = edit_redo_stack_.back();
                edit_redo_stack_.pop_back();
                edit_buf_ = std::move(next.text);
                edit_caret_ = next.caret;
                edit_sel_start_ = next.sel_start;
                edit_sel_end_ = next.sel_end;
                commit_edit();
                edit_blink_ = 0.0f;
            }
        }
        // Ctrl + A: Select All
        else if (ctrl && key_a) {
            edit_sel_start_ = 0;
            edit_sel_end_ = static_cast<int>(edit_buf_.size());
            edit_caret_ = edit_sel_end_;
            edit_blink_ = 0.0f;
        }
        // Ctrl + C: Copy
        else if (ctrl && key_c) {
            if (has_sel) {
                copy_text_to_clipboard(edit_buf_.substr(s_min, s_max - s_min));
            } else {
                copy_text_to_clipboard(edit_buf_);
            }
        }
        // Ctrl + X: Cut
        else if (ctrl && key_x) {
            if (has_sel) {
                push_undo();
                copy_text_to_clipboard(edit_buf_.substr(s_min, s_max - s_min));
                erase_sel();
            } else {
                push_undo();
                copy_text_to_clipboard(edit_buf_);
                edit_buf_.clear();
                edit_caret_ = edit_sel_start_ = edit_sel_end_ = 0;
            }
            edit_blink_ = 0.0f;
        }
        // Ctrl + V: Paste
        else if (ctrl && key_v) {
            const std::string clip = read_text_from_clipboard();
            std::string clean;
            for (char c : clip) {
                if (std::isxdigit(static_cast<unsigned char>(c)) || c == '#') clean.push_back(c);
            }
            if (!clean.empty()) {
                push_undo();
                erase_sel();
                edit_buf_.insert(edit_caret_, clean);
                if (edit_buf_.size() > 7) edit_buf_ = edit_buf_.substr(0, 7);
                edit_caret_ = static_cast<int>(edit_buf_.size());
                edit_sel_start_ = edit_sel_end_ = edit_caret_;
                commit_edit();
                edit_blink_ = 0.0f;
            }
        }
        // Backspace
        else if (in.key_pressed[VK_BACK]) {
            if (!erase_sel() && edit_caret_ > 0) {
                push_undo();
                edit_buf_.erase(edit_caret_ - 1, 1);
                --edit_caret_;
                edit_sel_start_ = edit_sel_end_ = edit_caret_;
            }
            edit_blink_ = 0.0f;
        }
        // Delete
        else if (in.key_pressed[VK_DELETE]) {
            if (!erase_sel() && edit_caret_ < static_cast<int>(edit_buf_.size())) {
                push_undo();
                edit_buf_.erase(edit_caret_, 1);
                edit_sel_start_ = edit_sel_end_ = edit_caret_;
            }
            edit_blink_ = 0.0f;
        }
        // Left Arrow
        else if (in.key_pressed[VK_LEFT]) {
            const int new_c = std::max(0, edit_caret_ - 1);
            if (shift) {
                if (!has_sel) edit_sel_start_ = edit_caret_;
                edit_caret_ = new_c;
                edit_sel_end_ = edit_caret_;
            } else {
                edit_caret_ = has_sel ? s_min : new_c;
                edit_sel_start_ = edit_sel_end_ = edit_caret_;
            }
            edit_blink_ = 0.0f;
        }
        // Right Arrow
        else if (in.key_pressed[VK_RIGHT]) {
            const int new_c = std::min(static_cast<int>(edit_buf_.size()), edit_caret_ + 1);
            if (shift) {
                if (!has_sel) edit_sel_start_ = edit_caret_;
                edit_caret_ = new_c;
                edit_sel_end_ = edit_caret_;
            } else {
                edit_caret_ = has_sel ? s_max : new_c;
                edit_sel_start_ = edit_sel_end_ = edit_caret_;
            }
            edit_blink_ = 0.0f;
        }
        // Home
        else if (in.key_pressed[VK_HOME]) {
            if (shift) {
                if (!has_sel) edit_sel_start_ = edit_caret_;
                edit_caret_ = 0;
                edit_sel_end_ = edit_caret_;
            } else {
                edit_caret_ = 0;
                edit_sel_start_ = edit_sel_end_ = edit_caret_;
            }
            edit_blink_ = 0.0f;
        }
        // End
        else if (in.key_pressed[VK_END]) {
            const int end_pos = static_cast<int>(edit_buf_.size());
            if (shift) {
                if (!has_sel) edit_sel_start_ = edit_caret_;
                edit_caret_ = end_pos;
                edit_sel_end_ = edit_caret_;
            } else {
                edit_caret_ = end_pos;
                edit_sel_start_ = edit_sel_end_ = edit_caret_;
            }
            edit_blink_ = 0.0f;
        }
        // Return / Escape: commit edit
        else if (in.key_pressed[VK_RETURN] || in.key_pressed[VK_ESCAPE]) {
            commit_edit();
        }
        // Type characters (when not holding Ctrl)
        else if (!ctrl) {
            const int maxlen = 7;     // "#RRGGBB"
            for (wchar_t wc : in.chars_typed) {
                if (wc < 32 || wc > 126) continue;
                const char ch = static_cast<char>(wc);
                const bool ok = (std::isxdigit(static_cast<unsigned char>(ch)) || ch == '#');
                if (!ok) continue;
                if (!erase_sel()) push_undo();
                if (static_cast<int>(edit_buf_.size()) >= maxlen) continue;
                edit_buf_.insert(edit_buf_.begin() + edit_caret_, ch);
                ++edit_caret_;
                edit_sel_start_ = edit_sel_end_ = edit_caret_;
                edit_blink_ = 0.0f;
            }
        }
    }

    // Every internal write this frame is now "seen" — only future
    // EXTERNAL changes trigger the HSV resync above.
    last_synced_color_ = color();
}

// BINARY: c_colorpicker render @ 0x140042FD0 (swatch), 0x140041370 (popup)
// Swatch: live bound color fill (no hardcoded constant)
// Popup contents:
//   1. SV picker square — texture qword_140245F78
//   2. Hue vertical bar — texture qword_140245F70
//   3. Alpha bar (if has_alpha) — texture qword_140245F68
//   4. Checkerboard behind transparent swatch — texture qword_140245F88
//   5. Selection cursor: vt[+528] render_filled_circle(cursor_pos, 0xFFFFFFFF)
void c_colorpicker::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    const auto& cs = style::current();
    const float radius = style::rounding().of(style::rounding().colorpicker);
    const auto br = rrect(bounds_);

    // Label text — suppressed for inline (row-slotted) pickers
    if (font_ && !inline_layout_) {
        const float baseline = br.y + (br.h - font_->pixel_height()) * 0.5f;
        r.render_text(*font_, Renderer::vec2{br.x, baseline}, cs.text, label_.c_str());
    }

    // Swatch
    Renderer::rect swatch = rrect(swatch_rect_);
    swatch.x = std::floor(swatch.x);
    swatch.y = std::floor(swatch.y);
    swatch.w = std::floor(swatch.w);
    swatch.h = std::floor(swatch.h);

    const Renderer::color hover_col{cs.accent.r, cs.accent.g, cs.accent.b, 0x80};
    const Renderer::color outline_base{0x3E, 0x3E, 0x42, 0xFF};
    Renderer::color border = Renderer::color::lerp(outline_base, hover_col, hover_anim_);
    border = Renderer::color::lerp(border, cs.accent, press_anim_);
    const auto row_col = [&](float k) {
        const Renderer::color row_bg{0x20, 0x20, 0x24, 0xFF};
        return Renderer::color::lerp(row_bg, border, k);
    };
    const auto cur = color();
    const Renderer::color cur_edge{cur.r, cur.g, cur.b,
        static_cast<std::uint8_t>(cur.a / 2)};

    const Renderer::rect swatch_in1{swatch.x + 1.0f, swatch.y + 1.0f,
                                    swatch.w - 2.0f, swatch.h - 2.0f};
    const Renderer::rect swatch_in2{swatch.x + 2.0f, swatch.y + 2.0f,
                                    swatch.w - 4.0f, swatch.h - 4.0f};
    const Renderer::rect swatch_in3{swatch.x + 3.0f, swatch.y + 3.0f,
                                    swatch.w - 6.0f, swatch.h - 6.0f};
    const float r0 = radius > 0.0f ? radius + 1.0f : 0.0f;
    const float r_in1 = std::max(0.0f, r0 - 1.0f);
    const float r_in2 = std::max(0.0f, r0 - 2.0f);
    const float r_in3 = std::max(0.0f, r0 - 3.0f);

    // 1. Border rows: 50% outer, 90% middle, 30% inner.
    r.render_rounded_rect(swatch, row_col(0.50f), r0, 1.0f, 12);
    r.render_rounded_rect(swatch_in1, row_col(0.90f), r_in1, 1.0f, 12);
    r.render_rounded_rect(swatch_in2, row_col(0.30f), r_in2, 1.0f, 12);
    // 2. Checkerboard backing + full-alpha picked colour core.
    r.render_filled_rounded_rect(swatch_in3, Renderer::color{120, 120, 120}, r_in3);
    r.render_filled_rounded_rect(swatch_in3, cur, r_in3);
    // 3. Half-alpha colour edge ring OVER the inner border row
    r.render_rounded_rect(swatch_in2, cur_edge, r_in2, 1.0f, 12);

    if (!popup_open_) return;

    // Popup on overlay layer
    r.push_layer(Renderer::layer_e::overlay);

    const Renderer::rect popup = rrect(popup_rect_);

    // Popup background — 0xFF232328 (same as body)
    r.render_filled_rounded_rect(popup, cs.body_bg, style::rounding().of(style::rounding().context_menu));
    r.render_rounded_rect(popup, cs.outline, style::rounding().of(style::rounding().context_menu), 1.0f);

    // SV plane
    const Renderer::rect sv_plane = rrect(sv_rect_);
    const auto hue_color = hsv_to_rgb(hue_, 1.0f, 1.0f);

    // Gradient: white (left) -> hue (right)
    r.render_gradient_rect(sv_plane, Renderer::color{255,255,255}, hue_color,
                                     hue_color, Renderer::color{255,255,255});
    // Overlay: transparent (top) -> black (bottom)
    r.render_gradient_rect(sv_plane, Renderer::color{0,0,0, 0}, Renderer::color{0,0,0, 0},
                                     Renderer::color{0,0,0,255}, Renderer::color{0,0,0,255});

    // SV cursor — white circle with dark outline, smoothly expands when dragging around
    {
        const float mx = sv_plane.x + sat_ * sv_plane.w;
        const float my = sv_plane.y + (1.0f - val_) * sv_plane.h;
        const float r_cur = 4.0f + sv_drag_anim_ * 2.5f; // expands when dragging
        const float thk   = 1.5f + sv_drag_anim_ * 0.5f;
        r.render_circle(Renderer::vec2{mx, my}, r_cur + 0.75f, Renderer::color{0, 0, 0, 160}, thk, 24);
        r.render_circle(Renderer::vec2{mx, my}, r_cur, Renderer::color{255, 255, 255, 255}, thk, 24);
    }

    // Hue bar — 6-stop horizontal rainbow with a white CIRCLE handle
    const float btn_rad = style::rounding().of(style::rounding().button);
    const Renderer::rect hue_bar = rrect(hue_rect_);
    {
        const Renderer::color stops[7] = {
            {255,   0,   0}, {255, 255,   0}, {  0, 255,   0},
            {  0, 255, 255}, {  0,   0, 255}, {255,   0, 255},
            {255,   0,   0},
        };
        for (int i = 0; i < 6; ++i) {
            const float x0 = hue_bar.x + i * (hue_bar.w / 6.0f);
            const float x1 = hue_bar.x + (i + 1) * (hue_bar.w / 6.0f);
            r.render_gradient_rect(
                Renderer::rect{x0, hue_bar.y, x1 - x0, hue_bar.h},
                stops[i], stops[i + 1], stops[i + 1], stops[i]);
        }
        r.render_rect(hue_bar, cs.outline, 1.0f);
        const float hx = hue_bar.x + (hue_ / 360.0f) * hue_bar.w;
        const float hcy = hue_bar.y + hue_bar.h * 0.5f;
        const float hr  = hue_bar.h * 0.5f;
        r.render_filled_circle(Renderer::vec2{hx, hcy}, hr, Renderer::color{255,255,255});
        r.render_circle(Renderer::vec2{hx, hcy}, hr, Renderer::color{0,0,0,160}, 1.0f, 16);
    }

    // Alpha bar — plain transparent→colour gradient over a faint dark base, white CIRCLE handle
    if (has_alpha_) {
        const Renderer::rect alpha_bar = rrect(alpha_rect_);
        r.render_filled_rect(alpha_bar, Renderer::color{0x20, 0x20, 0x24, 0xFF});
        r.render_gradient_rect(alpha_bar,
            Renderer::color{cur.r, cur.g, cur.b, 0},
            Renderer::color{cur.r, cur.g, cur.b, 255},
            Renderer::color{cur.r, cur.g, cur.b, 255},
            Renderer::color{cur.r, cur.g, cur.b, 0});
        r.render_rect(alpha_bar, cs.outline, 1.0f);
        const float ax  = alpha_bar.x + (cur.a / 255.0f) * alpha_bar.w;
        const float acy = alpha_bar.y + alpha_bar.h * 0.5f;
        const float ar  = alpha_bar.h * 0.5f;
        r.render_filled_circle(Renderer::vec2{ax, acy}, ar, Renderer::color{255,255,255});
        r.render_circle(Renderer::vec2{ax, acy}, ar, Renderer::color{0,0,0,160}, 1.0f, 16);
    }

    // A boxed EDITABLE field: 1-px outlined dark rect with centred text.
    auto draw_field = [&](Renderer::rect rr, const std::string& live,
                          Renderer::color tc, edit_field_t field) {
        const bool editing = (edit_field_ == field);
        const std::string& val = editing ? edit_buf_ : live;
        r.render_filled_rounded_rect(rr, cs.body_bg, btn_rad);
        const Renderer::color hov_col{cs.accent.r, cs.accent.g, cs.accent.b, 0x80};
        Renderer::color brd = Renderer::color::lerp(
            cs.outline, hov_col,
            std::clamp(field_hover_anim_, 0.0f, 1.0f));
        if (editing) brd = cs.accent;
        r.render_rounded_rect(rr, brd, btn_rad, 1.0f);
        if (!font_) return;

        const float tw = font_->measure_text(val);
        const float tx = rr.x + (rr.w - tw) * 0.5f;
        const float ty = rr.y + (rr.h - font_->pixel_height()) * 0.5f;

        const int s0 = std::min(edit_sel_start_, edit_sel_end_);
        const int s1 = std::max(edit_sel_start_, edit_sel_end_);
        const bool has_sel = editing && (s0 != s1);

        // Selection highlight rectangle
        if (has_sel) {
            const std::string head = val.substr(0, s0);
            const std::string sel  = val.substr(s0, s1 - s0);
            const float sel_x = tx + font_->measure_text(head);
            const float sel_w = font_->measure_text(sel);
            r.render_filled_rounded_rect(
                Renderer::rect{sel_x - 1.0f, rr.y + 3.0f, sel_w + 2.0f, rr.h - 6.0f},
                Renderer::color{cs.accent.r, cs.accent.g, cs.accent.b, 0xB0}, 2.0f);
        }

        if (!val.empty()) {
            if (has_sel) {
                const std::string head = val.substr(0, s0);
                const std::string sel  = val.substr(s0, s1 - s0);
                const std::string tail = val.substr(s1);

                float cur_x = tx;
                if (!head.empty()) {
                    r.render_text(*font_, Renderer::vec2{cur_x, ty}, tc, head.c_str());
                    cur_x += font_->measure_text(head);
                }
                if (!sel.empty()) {
                    // Bright high-contrast white text for selected portion
                    r.render_text(*font_, Renderer::vec2{cur_x, ty},
                                  Renderer::color{0xFF, 0xFF, 0xFF, 0xFF}, sel.c_str());
                    cur_x += font_->measure_text(sel);
                }
                if (!tail.empty()) {
                    r.render_text(*font_, Renderer::vec2{cur_x, ty}, tc, tail.c_str());
                }
            } else {
                r.render_text(*font_, Renderer::vec2{tx, ty}, tc, val.c_str());
            }
        }

        if (editing) {
            const int head_n = std::min(static_cast<int>(val.size()),
                                        std::max(0, edit_caret_));
            const float cx = tx + font_->measure_text(val.substr(0, head_n));
            const float aa = 0.5f + 0.5f * std::cos(edit_blink_);
            const std::uint8_t al = static_cast<std::uint8_t>(255.0f * aa);
            r.render_filled_rect(Renderer::rect{cx, rr.y + 3.0f, 1.0f, rr.h - 6.0f},
                                 Renderer::color{cs.accent.r, cs.accent.g,
                                                 cs.accent.b, al});
        }
    };

    // Boxed hex field (left) + Copy + Paste (right).
    draw_field(rrect(hex_rect_), color_to_hex(cur).substr(0, 7), cs.text,
               edit_field_t::hex);

    // Copy / Paste — boxed buttons with hover + held-press accent ramp.
    auto render_popup_button = [&](Renderer::rect rr, const char* text,
                                   float hov_a, float press_a) {
        r.render_filled_rounded_rect(rr, cs.button_bg, btn_rad);
        if (press_a > 0.01f) {
            const auto fa = static_cast<std::uint8_t>(
                64.0f * std::clamp(press_a, 0.0f, 1.0f));
            r.render_filled_rounded_rect(
                rr, Renderer::color{cs.accent.r, cs.accent.g, cs.accent.b, fa},
                btn_rad);
        }
        const Renderer::color hov_col{cs.accent.r, cs.accent.g, cs.accent.b, 0x80};
        Renderer::color brd = Renderer::color::lerp(cs.outline, hov_col,
                                                    std::clamp(hov_a, 0.0f, 1.0f));
        brd = Renderer::color::lerp(brd, cs.accent, std::clamp(press_a, 0.0f, 1.0f));
        r.render_rounded_rect(rr, brd, btn_rad, 1.0f);
        if (font_) {
            const float tw = font_->measure_text(text);
            r.render_text(*font_,
                          Renderer::vec2{rr.x + (rr.w - tw) * 0.5f,
                                         rr.y + (rr.h - font_->pixel_height()) * 0.5f},
                          cs.button_text, text);
        }
    };
    render_popup_button(rrect(copy_rect_),  "Copy",  copy_hover_anim_,  copy_press_anim_);
    render_popup_button(rrect(paste_rect_), "Paste", paste_hover_anim_, paste_press_anim_);

    r.pop_layer();
}

} // namespace GUI
