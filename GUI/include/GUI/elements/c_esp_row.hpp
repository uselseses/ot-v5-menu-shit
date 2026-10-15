#pragma once
// =============================================================================
// GUI::c_esp_row — composite "label row" that hosts inline adornments:
//   [☐] Label text ........ [chip] [■][■]
//   [ full-width dropdown row (optional) ]
//
// Mirrors the binary's label-row composites (sub_140035350 + the attach
// fns 0x1400355F0 / 0x140076720 / 0x140035D40 / 0x140076C30): checkbox
// LEFT of the text, keybinder chip + colorpicker swatches RIGHT-aligned
// on the same line, and an optional dropdown on its own row beneath.
//
// Promoted out of cheat_menu_builder.cpp (2026-06-11) so the Framework
// showcase rows use the same slot management — raw c_label adornment
// children positioned themselves from the label's stale next_y and all
// rendered at the label's x.
// =============================================================================

#include <memory>
#include <string>
#include <vector>

#include <GUI/elements/c_base_element.hpp>
#include <GUI/elements/c_checkbox.hpp>
#include <GUI/elements/c_colorpicker.hpp>
#include <GUI/elements/c_dropdown.hpp>
#include <GUI/elements/c_keybinder.hpp>
#include <GUI/input/input_state.hpp>
#include <GUI/style/colorscheme.hpp>
#include <GUI/style/element_sizes.hpp>

#include <Renderer/c_renderer_core.hpp>
#include <Renderer/font.hpp>

namespace GUI {

class c_esp_row : public c_base_element {
public:
    using bool_state_t  = wm::types::thread_safe_t<bool, bool>;
    using color_state_t = wm::types::thread_safe_t<std::uint32_t, std::uint32_t>;
    using int_state_t   = wm::types::thread_safe_t<int, int>;

    c_esp_row() : c_base_element{""} {}

    std::string_view type_name() const override { return ", ESPRow"; }
    bool             needs_hash() const override { return false; }

    void set_font(const Renderer::font* f) { font_ = f; }
    void set_label(std::string s) { label_ = std::move(s); }

    void set_checkbox(std::shared_ptr<c_checkbox> cb) {
        cb_ = std::move(cb);
        if (cb_) { cb_->set_inline_layout(true); add_child(cb_); }
    }
    // Appends — call twice for primary + secondary swatches. The FIRST
    // added renders right-most (matches c_label__render's index order).
    void set_colorpicker(std::shared_ptr<c_colorpicker> cp) {
        if (!cp) return;
        cp->set_inline_layout(true);
        cps_.push_back(cp);
        add_child(std::move(cp));
    }
    void set_dropdown(std::shared_ptr<c_dropdown> dd) {
        dd_ = std::move(dd);
        if (dd_) { dd_->set_inline_layout(true); add_child(dd_); }
    }
    void set_keybinder(std::shared_ptr<c_keybinder> kb) {
        kb_ = std::move(kb);
        if (kb_) add_child(kb_);
    }

    void update(float dt) override
    {
        const auto& sz = style::sizes();
        auto p = parent_.lock();
        if (!p) return;

        // Self bounds: a single row at parent's next_y, full inner width.
        const float row_x = p->bounds().x + sz.text_margin;
        const float row_y = p->next_y();
        const float row_w = p->bounds().w - 2.0f * sz.text_margin;
        const float row_h = sz.checkbox_height;        // 20
        // The multi-select dropdown is NOT inline with the checkbox — it
        // takes its own FULL-WIDTH line directly under the row (matches
        // the original's ESP Weapons/Flags rows).
        const float dd_row_h = dd_ ? row_h + 4.0f : 0.0f;
        bounds_.x = row_x; bounds_.y = row_y;
        bounds_.w = row_w; bounds_.h = row_h + dd_row_h;
        // Advance parent flow exactly like c_checkbox would.
        p->set_next_y(row_y + row_h + dd_row_h + sz.button_height);

        // Slot sizes.
        const float sw  = sz.checkbox_size;            // 12 — swatch / box
        const float pad = sz.label_offset > 0.0f ? sz.label_offset : 6.0f;

        // Label-text hover span (lights the checkbox hover while the
        // cursor is over the TEXT, not only the box).
        const float lbl_x0 = row_x + (cb_ ? sw + pad : 0.0f);
        const float lbl_x1 = row_x + row_w
                           - static_cast<float>(cps_.size()) * (sw + pad)
                           - (kb_ ? sz.label_spacing + pad : 0.0f);
        bool label_hover = false;
        {
            auto& in = input::current();
            const auto& mp = in.mouse_pos;
            label_hover = !in.input_blocked_by_other(this) &&
                          mp.x >= lbl_x0 && mp.x < lbl_x1 &&
                          mp.y >= row_y && mp.y < row_y + row_h;
        }
        if (cb_) cb_->set_force_hover(label_hover);

        // Checkbox on the FAR LEFT.
        if (cb_) {
            const float saved_x = bounds_.x;
            const float saved_w = bounds_.w;
            bounds_.x = row_x;
            bounds_.w = sw;
            next_y_   = row_y;
            cb_->update(dt);
            bounds_.x = saved_x;
            bounds_.w = saved_w;
        }
        text_x_ = row_x + (cb_ ? sw + pad : 0.0f);

        // Colorpicker swatches on the FAR RIGHT (first added = right-most).
        float right_x = row_x + row_w;
        for (auto& cp : cps_) {
            if (!cp) continue;
            const float saved_x = bounds_.x;
            const float saved_w = bounds_.w;
            bounds_.x = right_x - sw - 2.0f;
            bounds_.w = sw + 2.0f;
            next_y_   = row_y;
            cp->update(dt);
            bounds_.x = saved_x;
            bounds_.w = saved_w;
            right_x  -= sw + pad;
        }

        // Keybinder chip right-aligned after the swatches. c_keybinder
        // positions itself from parent bounds + text_margin, so widen the
        // fake slot by text_margin on both sides to land the chip flush
        // with right_x.
        if (kb_) {
            const float saved_x = bounds_.x;
            const float saved_w = bounds_.w;
            bounds_.x = row_x - sz.text_margin;
            bounds_.w = (right_x - row_x) + 2.0f * sz.text_margin;
            next_y_   = row_y;
            kb_->update(dt);
            bounds_.x = saved_x;
            bounds_.w = saved_w;
            right_x  -= sz.label_spacing + pad;
        }

        // Clicking the label text toggles the checkbox (original behavior).
        if (cb_) {
            auto& in = input::current();
            if (label_hover && in.mouse_pressed[input::mb_left]) {
                cb_->set(!cb_->get());
                in.consume_mouse_press();
            }
        }

        // Dropdown — full-width row UNDER the checkbox + swatch line.
        if (dd_) {
            const float saved_x = bounds_.x;
            const float saved_w = bounds_.w;
            bounds_.x = row_x;
            bounds_.w = row_w;
            next_y_   = row_y + row_h + 4.0f;
            dd_->update(dt);
            bounds_.x = saved_x;
            bounds_.w = saved_w;
        }
    }

    void render(Renderer::c_renderer_core& r) override
    {
        if (!visible_) return;
        if (font_) {
            const auto& cs = style::current();
            // Centre within the CHECKBOX row only — bounds_.h also
            // includes the dropdown row when present.
            const float row_h = style::sizes().checkbox_height;
            const float baseline = bounds_.y +
                (row_h - font_->pixel_height()) * 0.5f;
            r.render_text(*font_, Renderer::vec2{text_x_, baseline},
                          cs.text, label_.c_str());
        }
        // Children (checkbox, swatches, keybinder, dropdown) render via base.
        c_base_element::render(r);
    }

private:
    const Renderer::font*                       font_ = nullptr;
    std::shared_ptr<c_checkbox>                 cb_;
    std::vector<std::shared_ptr<c_colorpicker>> cps_;
    std::shared_ptr<c_dropdown>                 dd_;
    std::shared_ptr<c_keybinder>                kb_;
    float                                       text_x_ = 0.0f;
};

} // namespace GUI
