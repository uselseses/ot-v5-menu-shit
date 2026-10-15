// =============================================================================
// textbox_textcontainer.cpp — c_textbox_new + c_text_container
//
// Binary verified — HOW_IT_WORKS.md §22, §27:
//   c_textbox render: 0x14005A820 (thunk -> slot+40)
//   c_text_container: 0x1401F7720
// =============================================================================

#include <GUI/elements/c_textbox_new.hpp>
#include <GUI/elements/c_text_container.hpp>

#include <GUI/input/cursor.hpp>
#include <GUI/input/input_state.hpp>
#include <GUI/style/colorscheme.hpp>
#include <GUI/style/element_sizes.hpp>
#include <GUI/style/rounding.hpp>

#include <Renderer/c_renderer_core.hpp>
#include <Renderer/font.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <cmath>

namespace GUI {

namespace {
inline Renderer::rect rrect(c_base_element::rect_t r) {
    return Renderer::rect{r.x, r.y, r.w, r.h};
}

void utf8_append(std::string& dst, wchar_t wc)
{
    const auto u = static_cast<std::uint32_t>(wc);
    if (u < 0x80) {
        dst.push_back(static_cast<char>(u));
    } else if (u < 0x800) {
        dst.push_back(static_cast<char>(0xC0 | (u >> 6)));
        dst.push_back(static_cast<char>(0x80 | (u & 0x3F)));
    } else {
        dst.push_back(static_cast<char>(0xE0 | (u >> 12)));
        dst.push_back(static_cast<char>(0x80 | ((u >> 6) & 0x3F)));
        dst.push_back(static_cast<char>(0x80 | (u & 0x3F)));
    }
}

int utf8_char_size(const std::string& s, int i)
{
    if (i < 0 || i >= static_cast<int>(s.size())) return 0;
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80)        return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

int utf8_step_back(const std::string& s, int i)
{
    if (i <= 0) return 0;
    --i;
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
    return i;
}

inline void set_clipboard_text_u8(const std::string& text)
{
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    const int len_w = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (len_w > 0) {
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len_w * sizeof(wchar_t));
        if (hMem) {
            wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
            if (pMem) {
                MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pMem, len_w);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
    }
    CloseClipboard();
}

inline std::string get_clipboard_text_u8()
{
    if (!OpenClipboard(nullptr)) return "";
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) {
        CloseClipboard();
        return "";
    }
    const wchar_t* pText = static_cast<const wchar_t*>(GlobalLock(hData));
    if (!pText) {
        CloseClipboard();
        return "";
    }
    int len_u8 = WideCharToMultiByte(CP_UTF8, 0, pText, -1, nullptr, 0, nullptr, nullptr);
    std::string result;
    if (len_u8 > 1) {
        result.resize(len_u8 - 1);
        WideCharToMultiByte(CP_UTF8, 0, pText, -1, result.data(), len_u8, nullptr, nullptr);
    }
    GlobalUnlock(hData);
    CloseClipboard();
    return result;
}

inline bool is_word_char(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

inline int find_prev_word_pos(const std::string& s, int pos)
{
    if (pos <= 0) return 0;
    int i = pos;
    while (i > 0 && s[i - 1] == ' ') --i;
    while (i > 0 && is_word_char(s[i - 1])) --i;
    if (i == pos && i > 0) {
        while (i > 0 && !is_word_char(s[i - 1]) && s[i - 1] != ' ') --i;
    }
    return i;
}

inline int find_next_word_pos(const std::string& s, int pos)
{
    const int n = static_cast<int>(s.size());
    if (pos >= n) return n;
    int i = pos;
    while (i < n && is_word_char(s[i])) ++i;
    while (i < n && s[i] == ' ') ++i;
    if (i == pos && i < n) {
        while (i < n && !is_word_char(s[i]) && s[i] != ' ') ++i;
        while (i < n && s[i] == ' ') ++i;
    }
    return i;
}
} // anon

// ============================================================================
// c_textbox_new
// ============================================================================

void c_textbox_new::update(float dt)
{
    auto& in = input::current();
    auto& cur = input::cursor();
    const auto& sz = style::sizes();

    // Auto-layout from parent — without this the textbox stays at
    // (0,0,0,0) when added to a groupbox without an explicit
    // set_bounds() (user 2026-06-02: "textbox in elements doesn't
    // render"). Mirrors the layout shape used by c_checkbox /
    // c_slider.
    if (auto p = parent_.lock()) {
        if (!inline_layout_) {
            const float parent_next_y = p->next_y();
            bounds_.x = p->bounds().x + sz.text_margin;
            bounds_.w = p->bounds().w - 2.0f * sz.text_margin;
            bounds_.y = parent_next_y;
            if (bounds_.h <= 0.0f) bounds_.h = sz.textbox_height;
            // Unset sentinel → default 6 px; explicit values (incl.
            // negative overlap) attach the next element tight below —
            // the cloud search box shares its bottom border with the
            // list's top border.
            extra_height_ = visible_
                ? (flow_gap_ > -1.0e8f ? flow_gap_ : sz.button_height)
                : 0.0f;
            p->set_next_y(parent_next_y + bounds_.h + extra_height_);
        } else {
            // Inline: the HOST owns the bounds (dialog layout_children,
            // row composites). Reading p->next_y() here put the dialog's
            // "Config name" input at the top of the screen, clipped away
            // (user 2026-06-11: "create button does not show input text").
            if (bounds_.h <= 0.0f) bounds_.h = sz.textbox_height;
        }
    }

    const auto br = rrect(bounds_);

    // Compute active box and text rect. The dumped input path chooses a
    // dedicated hit rect (+80/+96); clicking the label side must not focus.
    // When there's NO label (search box / dialog input) the whole control
    // is the editable box instead of just the right 55%.
    const float label_frac = label_.empty() ? 0.0f : 0.45f;
    const float box_x = br.x + br.w * label_frac;
    box_rect_ = rect_t{box_x, br.y, br.w - (box_x - br.x), br.h};
    text_rect_.x = box_rect_.x + 6.0f;
    text_rect_.y = box_rect_.y + 2.0f;
    text_rect_.w = box_rect_.w - 12.0f;
    text_rect_.h = box_rect_.h - 4.0f;

    const auto box = rrect(box_rect_);

    // Modal guard — a FOREIGN modal freezes the textbox, but a textbox
    // INSIDE the modal owner (e.g. the "create config" dialog's name
    // input) must stay editable.
    bool owned_by_ancestor = false;
    for (auto a = parent_.lock(); a; a = a->parent().lock()) {
        if (static_cast<const void*>(a.get()) == in.modal_owner) {
            owned_by_ancestor = true;
            break;
        }
    }
    if (in.input_blocked_by_other(this) && !owned_by_ancestor) {
        hovered_     = false;
        focused_     = false;
        pressed_     = false;
        hover_anim_  = 0.0f;
        press_anim_  = 0.0f;
        return;
    }

    hovered_ = box.contains(in.mouse_pos);
    if (hovered_) cur.set(input::cursor_type::ibeam);

    auto get_caret_from_x = [&](float mx) -> int {
        if (!font_) return 0;
        const float local_x = mx - text_rect_.x;
        float w = 0;
        int result = 0;
        for (int i = 0; i < static_cast<int>(value_.size()); ) {
            const int n = utf8_char_size(value_, i);
            const std::string c = value_.substr(i, n);
            const float cw = font_->measure_text(c);
            if (w + cw * 0.5f > local_x) break;
            w += cw;
            i += n;
            result = i;
        }
        return result;
    };

    if (in.mouse_pressed[input::mb_left]) {
        pressed_ = hovered_;
        if (hovered_) in.consume_mouse_press();
        focused_ = hovered_;
        if (focused_) {
            caret_ = get_caret_from_x(in.mouse_pos.x);
            sel_start_ = sel_end_ = caret_;
            blink_ = 0.0f;
        }
    }
    if (pressed_ && in.mouse_down[input::mb_left]) {
        caret_ = get_caret_from_x(in.mouse_pos.x);
        sel_end_ = caret_;
    }
    if (!in.mouse_down[input::mb_left]) pressed_ = false;

    const float hover_target = (hovered_ || focused_) ? 1.0f : 0.0f;
    const float press_target = pressed_ ? 1.0f : 0.0f;
    hover_anim_ += (hover_target - hover_anim_) * std::clamp(dt * 14.0f, 0.0f, 1.0f);
    press_anim_ += (press_target - press_anim_) * std::clamp(dt * 18.0f, 0.0f, 1.0f);

    if (!focused_) return;

    const int s_min = std::min(sel_start_, sel_end_);
    const int s_max = std::max(sel_start_, sel_end_);
    const bool has_sel = (s_min != s_max);

    auto push_undo = [&]() {
        undo_state_t st{value_, caret_, sel_start_, sel_end_};
        if (undo_stack_.empty() || undo_stack_.back().text != st.text) {
            undo_stack_.push_back(std::move(st));
            if (undo_stack_.size() > 100) undo_stack_.erase(undo_stack_.begin());
            redo_stack_.clear();
        }
    };

    auto erase_selection = [&]() -> bool {
        if (has_sel) {
            push_undo();
            value_.erase(s_min, s_max - s_min);
            caret_ = s_min;
            sel_start_ = sel_end_ = caret_;
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
        if (!undo_stack_.empty()) {
            redo_stack_.push_back(undo_state_t{value_, caret_, sel_start_, sel_end_});
            auto prev = undo_stack_.back();
            undo_stack_.pop_back();
            value_ = std::move(prev.text);
            caret_ = prev.caret;
            sel_start_ = prev.sel_start;
            sel_end_ = prev.sel_end;
            clamp_caret();
            blink_ = 0.0f;
        }
    }
    // Ctrl + Y or Ctrl + Shift + Z: Redo
    else if ((ctrl && key_y) || (ctrl && shift && key_z)) {
        if (!redo_stack_.empty()) {
            undo_stack_.push_back(undo_state_t{value_, caret_, sel_start_, sel_end_});
            auto next = redo_stack_.back();
            redo_stack_.pop_back();
            value_ = std::move(next.text);
            caret_ = next.caret;
            sel_start_ = next.sel_start;
            sel_end_ = next.sel_end;
            clamp_caret();
            blink_ = 0.0f;
        }
    }
    // Ctrl + A: Select All
    else if (ctrl && key_a) {
        sel_start_ = 0;
        sel_end_ = static_cast<int>(value_.size());
        caret_ = sel_end_;
        blink_ = 0.0f;
    }
    // Ctrl + C: Copy
    else if (ctrl && key_c) {
        if (has_sel) {
            set_clipboard_text_u8(value_.substr(s_min, s_max - s_min));
        }
    }
    // Ctrl + X: Cut
    else if (ctrl && key_x) {
        if (has_sel) {
            push_undo();
            set_clipboard_text_u8(value_.substr(s_min, s_max - s_min));
            erase_selection();
            blink_ = 0.0f;
        }
    }
    // Ctrl + V: Paste
    else if (ctrl && key_v) {
        std::string clip = get_clipboard_text_u8();
        std::string clean;
        clean.reserve(clip.size());
        for (char c : clip) {
            if (c != '\r' && c != '\n' && c != '\t') clean.push_back(c);
        }
        if (!clean.empty()) {
            push_undo();
            erase_selection();
            value_.insert(caret_, clean);
            caret_ += static_cast<int>(clean.size());
            sel_start_ = sel_end_ = caret_;
            blink_ = 0.0f;
        }
    }
    // Ctrl + Backspace: Delete previous word
    else if (ctrl && in.key_pressed[VK_BACK]) {
        if (!erase_selection()) {
            push_undo();
            const int prev = find_prev_word_pos(value_, caret_);
            value_.erase(prev, caret_ - prev);
            caret_ = prev;
            sel_start_ = sel_end_ = caret_;
        }
        blink_ = 0.0f;
    }
    // Ctrl + Delete: Delete next word
    else if (ctrl && in.key_pressed[VK_DELETE]) {
        if (!erase_selection()) {
            push_undo();
            const int next = find_next_word_pos(value_, caret_);
            value_.erase(caret_, next - caret_);
            sel_start_ = sel_end_ = caret_;
        }
        blink_ = 0.0f;
    }
    // Backspace
    else if (in.key_pressed[VK_BACK]) {
        if (!erase_selection()) {
            if (caret_ > 0) {
                push_undo();
                const int prev = utf8_step_back(value_, caret_);
                value_.erase(prev, caret_ - prev);
                caret_ = prev;
                sel_start_ = sel_end_ = caret_;
            }
        }
        blink_ = 0.0f;
    }
    // Delete
    else if (in.key_pressed[VK_DELETE]) {
        if (!erase_selection()) {
            if (caret_ < static_cast<int>(value_.size())) {
                push_undo();
                const int n = utf8_char_size(value_, caret_);
                value_.erase(caret_, n);
                sel_start_ = sel_end_ = caret_;
            }
        }
        blink_ = 0.0f;
    }
    // Left Arrow
    else if (in.key_pressed[VK_LEFT]) {
        const int new_c = ctrl ? find_prev_word_pos(value_, caret_) : utf8_step_back(value_, caret_);
        if (shift) {
            if (!has_sel) sel_start_ = caret_;
            caret_ = new_c;
            sel_end_ = caret_;
        } else {
            caret_ = has_sel ? s_min : new_c;
            sel_start_ = sel_end_ = caret_;
        }
        blink_ = 0.0f;
    }
    // Right Arrow
    else if (in.key_pressed[VK_RIGHT]) {
        const int new_c = ctrl ? find_next_word_pos(value_, caret_) : (caret_ + utf8_char_size(value_, caret_));
        if (shift) {
            if (!has_sel) sel_start_ = caret_;
            caret_ = new_c;
            sel_end_ = caret_;
        } else {
            caret_ = has_sel ? s_max : new_c;
            sel_start_ = sel_end_ = caret_;
        }
        clamp_caret();
        blink_ = 0.0f;
    }
    // Home
    else if (in.key_pressed[VK_HOME]) {
        if (shift) {
            if (!has_sel) sel_start_ = caret_;
            caret_ = 0;
            sel_end_ = caret_;
        } else {
            caret_ = 0;
            sel_start_ = sel_end_ = caret_;
        }
        blink_ = 0.0f;
    }
    // End
    else if (in.key_pressed[VK_END]) {
        const int end_pos = static_cast<int>(value_.size());
        if (shift) {
            if (!has_sel) sel_start_ = caret_;
            caret_ = end_pos;
            sel_end_ = caret_;
        } else {
            caret_ = end_pos;
            sel_start_ = sel_end_ = caret_;
        }
        blink_ = 0.0f;
    }
    // Escape / Enter
    else if (in.key_pressed[VK_ESCAPE] || in.key_pressed[VK_RETURN]) {
        sel_start_ = sel_end_ = caret_;
        if (in.key_pressed[VK_ESCAPE]) focused_ = false;
    }

    // Insert typed characters (when not holding Ctrl)
    if (!in.chars_typed.empty() && !ctrl) {
        std::string ins;
        for (wchar_t wc : in.chars_typed) {
            if (wc < 0x20 || wc == 0x7F) continue;
            utf8_append(ins, wc);
        }
        if (!ins.empty()) {
            if (!erase_selection()) push_undo();
            value_.insert(caret_, ins);
            caret_ += static_cast<int>(ins.size());
            sel_start_ = sel_end_ = caret_;
            blink_ = 0.0f;
        }
    }

    // Caret blink (Jul 25: fade in/out)
    blink_ += dt * 6.28318f;
    if (blink_ > 6.28318f * 2) blink_ -= 6.28318f * 2;
}

// BINARY: c_textbox render @ 0x14005A820
// 1. vt[+288] render_filled_rect(bounds, 0xFF232328, textbox_rounding)
// 2. vt[+296] render_rect_outline(bounds) — 0xFF3E3E42 unfocused, accent focused
// 3. vt[+264] push_clip(text_bounds)
// 4. if (has_selection):
//    vt[+288] render_filled_rect(sel_rect, alpha_blend(accent, 0x60), 0)
// 5. vt[+472] render_text(text_pos, decoded_value, 0xFFBEBEBE, ALIGN_LEFT)
// 6. if (focused && cursor_visible):
//    cursor_alpha = blink_fade(GetTickCount64())
//    vt[+288] render_filled_rect(cursor_rect, alpha_blend(accent, cursor_alpha))
void c_textbox_new::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    const auto& cs = style::current();
    const float radius = style::rounding().of(style::rounding().textbox);
    const auto br = rrect(bounds_);

    // Label (left side)
    if (font_) {
        const float baseline = br.y + (br.h - font_->pixel_height()) * 0.5f;
        r.render_text(*font_, Renderer::vec2{br.x, baseline}, cs.text, label_.c_str());
    }

    // Box on right half — 0xFF232328 (same as body)
    const float box_x = br.x + br.w * 0.45f;
    const Renderer::rect box = box_rect_.w > 0.0f
        ? rrect(box_rect_)
        : Renderer::rect{box_x, br.y, br.w - (box_x - br.x), br.h};
    r.render_filled_rounded_rect(box, cs.body_bg, radius);

    // User 2026-06-02: NO accent fill on click/focus — only the
    // border reflects the focused state. The previous accent overlay
    // (capped at 0x40) was the "different type" feel the user
    // wanted removed.

    // Border — 0xFF3E3E42 unfocused, accent on hover (alpha 128) and
    // solid accent when pressed/focused. Same three-step lerp as
    // c_button::render.
    const Renderer::color hover_col{cs.accent.r, cs.accent.g, cs.accent.b, 0x80};
    Renderer::color border = Renderer::color::lerp(cs.outline, hover_col, hover_anim_);
    border = Renderer::color::lerp(border, cs.accent, focused_ ? 1.0f : press_anim_);
    r.render_rounded_rect(box, border, radius, 1.0f);

    if (!font_) return;

    r.push_clip(box);

    const float text_x_origin = box.x + 6.0f;
    const float baseline = box.y + (box.h - font_->pixel_height()) * 0.5f;

    // Selection highlight (if any)
    if (has_selection()) {
        const int s0 = std::min(sel_start_, sel_end_);
        const int s1 = std::max(sel_start_, sel_end_);
        const std::string head = value_.substr(0, s0);
        const std::string sel  = value_.substr(s0, s1 - s0);
        const float sel_x = text_x_origin + font_->measure_text(head);
        const float sel_w = font_->measure_text(sel);
        // Vivid, highly visible selection background (alpha 0xB0)
        r.render_filled_rounded_rect(Renderer::rect{sel_x - 1.0f, box.y + 3.0f, sel_w + 2.0f, box.h - 6.0f},
                                     Renderer::color{cs.accent.r, cs.accent.g, cs.accent.b, 0xB0}, 2.0f);
    }

    // Text rendering
    if (value_.empty() && !focused_ && !placeholder_.empty()) {
        r.render_text(*font_, Renderer::vec2{text_x_origin, baseline},
                      cs.text_disabled, placeholder_.c_str());
    } else if (!value_.empty()) {
        if (has_selection()) {
            const int s0 = std::min(sel_start_, sel_end_);
            const int s1 = std::max(sel_start_, sel_end_);
            const std::string head = value_.substr(0, s0);
            const std::string sel  = value_.substr(s0, s1 - s0);
            const std::string tail = value_.substr(s1);

            float cur_x = text_x_origin;
            if (!head.empty()) {
                r.render_text(*font_, Renderer::vec2{cur_x, baseline}, cs.text, head.c_str());
                cur_x += font_->measure_text(head);
            }
            if (!sel.empty()) {
                // Bright high-contrast white text over the accent selection box
                r.render_text(*font_, Renderer::vec2{cur_x, baseline},
                              Renderer::color{0xFF, 0xFF, 0xFF, 0xFF}, sel.c_str());
                cur_x += font_->measure_text(sel);
            }
            if (!tail.empty()) {
                r.render_text(*font_, Renderer::vec2{cur_x, baseline}, cs.text, tail.c_str());
            }
        } else {
            r.render_text(*font_, Renderer::vec2{text_x_origin, baseline},
                          cs.text, value_.c_str());
        }
    }

    // Caret — alpha modulates with blink
    if (focused_) {
        const std::string head = value_.substr(0, caret_);
        const float cx = text_x_origin + font_->measure_text(head);
        const float cy = box.y + 4.0f;
        const float ch = box.h - 8.0f;
        const float a = 0.5f + 0.5f * std::cos(blink_);
        const std::uint8_t alpha = static_cast<std::uint8_t>(255.0f * a);
        r.render_filled_rect(Renderer::rect{cx, cy, 1.0f, ch},
                             Renderer::color{cs.accent.r, cs.accent.g, cs.accent.b, alpha});
    }

    r.pop_clip();
}

// ============================================================================
// c_text_container
// ============================================================================

void c_text_container::rewrap()
{
    lines_.clear();
    rewrap_ = false;
    if (!font_ || text_.empty() || bounds_.w <= 0) return;

    const float inner_w = bounds_.w - 16.0f;
    const std::string& s = text_;
    std::size_t i = 0;
    while (i < s.size()) {
        const std::size_t nl = s.find('\n', i);
        const std::size_t segment_end = (nl == std::string::npos) ? s.size() : nl;

        std::size_t line_start = i;
        std::size_t last_space = std::string::npos;
        float w_so_far = 0;
        for (std::size_t j = i; j < segment_end; ) {
            const int n = utf8_char_size(s, static_cast<int>(j));
            const std::string c = s.substr(j, n);
            const float cw = font_->measure_text(c);
            if (w_so_far + cw > inner_w && line_start < j) {
                const std::size_t break_pt = (last_space != std::string::npos && last_space > line_start)
                    ? last_space : j;
                lines_.push_back(s.substr(line_start, break_pt - line_start));
                line_start = (break_pt == last_space) ? break_pt + 1 : break_pt;
                last_space = std::string::npos;
                w_so_far = 0;
                for (std::size_t k = line_start; k < j; ) {
                    const int nn = utf8_char_size(s, static_cast<int>(k));
                    w_so_far += font_->measure_text(s.substr(k, nn));
                    k += nn;
                }
            }
            if (c == " ") last_space = j;
            w_so_far += cw;
            j += n;
        }
        if (line_start < segment_end) {
            lines_.push_back(s.substr(line_start, segment_end - line_start));
        } else if (line_start == segment_end) {
            lines_.emplace_back();
        }
        i = segment_end + (nl == std::string::npos ? 0 : 1);
    }

    last_w_ = bounds_.w;
}

void c_text_container::update(float /*dt*/)
{
    if (rewrap_ || std::fabs(bounds_.w - last_w_) > 0.5f) rewrap();

    auto& in = input::current();
    const auto br = rrect(bounds_);
    hovered_ = br.contains(in.mouse_pos);
    if (hovered_ && in.wheel_delta != 0 && font_) {
        const float line_h = font_->line_advance();
        const float content_h = lines_.size() * line_h + (header_.empty() ? 0 : font_->pixel_height() + 8);
        const float visible_h = br.h;
        if (content_h > visible_h) {
            scroll_y_ -= in.wheel_delta * 3 * line_h;
            scroll_y_ = std::clamp(scroll_y_, 0.0f, content_h - visible_h);
        }
    }
}

// BINARY: c_text_container render
// 1. (optional) bg fill
// 2. vt[+264] push_clip(content_bounds)
// 3. vt[+472] render_text(lines, font, 0xFFBEBEBE, ALIGN_LEFT, ALIGN_TOP)
// 4. pop clip
// 5. scrollbar when overflow
void c_text_container::render(Renderer::c_renderer_core& r)
{
    if (!visible_ || !font_) return;
    const auto& cs = style::current();
    const float radius = style::rounding().of(style::rounding().text_container);
    const auto br = rrect(bounds_);

    // Background — recessed inset (0xFF19191D) for the changelog, else the
    // body bg (0xFF232328). 1-px 0xFF3E3E42 outline. Skipped when frameless.
    if (!frameless_) {
        r.render_filled_rounded_rect(br, recessed_ ? cs.tab_strip_bg : cs.body_bg, radius);
        r.render_rounded_rect(br, cs.outline, radius, 1.0f);
    }

    r.push_clip(br);

    float y = br.y + 8 - scroll_y_;
    if (!header_.empty()) {
        r.render_text(*font_, Renderer::vec2{br.x + 8, y}, cs.title_text, header_.c_str());
        y += font_->pixel_height() + 6;
        // Separator — 0xFF353538
        r.render_filled_rect(Renderer::rect{br.x + 8, y, br.w - 16, 1.0f}, cs.separator_dark);
        y += 4;
    }
    const float line_h = font_->line_advance();
    for (const auto& ln : lines_) {
        if (y + line_h >= br.y && y <= br.y + br.h) {
            // Text — 0xFFBEBEBE
            r.render_text(*font_, Renderer::vec2{br.x + 8, y}, cs.text, ln.c_str());
        }
        y += line_h;
    }

    r.pop_clip();
}

} // namespace GUI
