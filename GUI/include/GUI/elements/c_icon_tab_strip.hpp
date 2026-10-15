#pragma once
// =============================================================================
// GUI::c_icon_tab_strip — horizontal tab strip with built-in icon glyphs.
//
// Designed for the top of the Visuals page (Enemy / Team / Self) — a row of
// buttons each combining a small procedural icon with a label, with one
// active tab at a time. Children are pages that the strip shows/hides; the
// active page is rendered below the strip with the strip height accounted
// for in the layout.
//
// Click selects a tab; hover sets cursor_type::hand.
// =============================================================================

#include <GUI/elements/c_base_element.hpp>
#include <GUI/containers/c_page.hpp>
#include <GUI/input/cursor.hpp>
#include <GUI/input/input_state.hpp>
#include <GUI/style/colorscheme.hpp>
#include <GUI/style/element_sizes.hpp>
#include <Renderer/c_renderer_core.hpp>
#include <Renderer/font.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace GUI {

// Stylised silhouettes drawn procedurally so no icon-texture asset is
// required. Each renders centered inside the supplied rect.
enum class tab_icon_e {
    none = 0,
    enemy,   // crosshair-marked figure (red accent)
    team,    // two stacked figures (green accent)
    self,    // single figure facing forward (orange accent)
    // Weapon entries — binary uses the embedded `csgo_icons` v1.1 CFF
    // icon font (header @ 0x140205C87 in IDB; dict tables @ 0x14018B940).
    // Reconstruction needs to extract the font + glyph mapping before
    // these can render. Until then the strip falls back to text labels.
    pistol,
    heavy,
    shotgun,
    smg,
    rifle,
    scout,
    awp,
    autosniper,
};

// Global player-icon font — the binary's embedded Glyphter TTF
// (exported_textures/player_icons.otf, carved by
// scripts/extract_icon_font.py). Person glyphs live at 'A' (enemy),
// 'B' (team), 'C' (self). The host sets this after loading; strips fall
// back to the procedural silhouettes when null.
inline const Renderer::font*& player_icon_font() {
    static const Renderer::font* f = nullptr;
    return f;
}

class c_icon_tab_strip : public c_base_element {
public:
    struct entry_t {
        std::string  label;
        tab_icon_e   icon = tab_icon_e::none;
    };

    explicit c_icon_tab_strip(std::vector<entry_t> entries)
        : c_base_element{""}, entries_{std::move(entries)} {}

    std::string_view type_name() const override { return ", IconTabStrip"; }
    bool             needs_hash() const override { return false; }

    void set_font(const Renderer::font* f) { font_ = f; }

    // Optional: weapon icon font (the `csgo_icons` CFF/PUA glyph font).
    // When set, weapon tab_icon_e entries render via render_text against
    // this font using `weapon_glyph(icon)` as the UTF-8 sequence. When
    // null, weapon icons render as nothing (text label only).
    void set_icon_font(const Renderer::font* f) { icon_font_ = f; }

    // Override the default PUA mapping (codepoints in 0xE000..0xE007 by
    // default). Use the codepoint your font expects per weapon.
    void set_weapon_glyph(tab_icon_e w, std::uint32_t codepoint) {
        if (w >= tab_icon_e::pistol && w <= tab_icon_e::autosniper) {
            const int idx = static_cast<int>(w) - static_cast<int>(tab_icon_e::pistol);
            if (idx >= 0 && idx < 8) weapon_codepoints_[idx] = codepoint;
        }
    }

    int  active_index() const noexcept { return active_; }
    void set_active_index(int i) {
        if (i >= 0 && i < static_cast<int>(entries_.size()) && i != active_) {
            active_ = i;
            page_anim_alpha_ = 0.0f;
        }
    }

    // Strip height (icon+label+padding). Pages render below this.
    float strip_height() const noexcept { return strip_h_; }

    // Layout rule per user 2026-05-31:
    //   * Strip flush to the TOP of the parent page — no left/right/top
    //     padding. The page's content_rect insets are bypassed so the
    //     strip touches the menu border + tab-strip border.
    //   * Equal-width flex distribution — no horizontal gaps.
    //   * Sharp 0-px corners.
    void update(float dt) override
    {
        const int n_int = static_cast<int>(entries_.size());
        if (n_int <= 0) return;

        // User 2026-06-01: strip sits at the TOP of the page (outer y,
        // no inset) and spans the full page width MINUS the 1-px
        // vertical separator at the left edge of the content area
        // (c_menu_window renders that as the "bar from logo" running
        // down the right side of the side-tab strip). Without the
        // +1/-1 nudge the leftmost weapon tab's background covers
        // that separator.
        if (auto p = parent_.lock()) {
            const auto* pg = dynamic_cast<c_page*>(p.get());
            const auto pb = pg ? pg->outer_bounds() : p->bounds();
            bounds_.x = pb.x + 1.0f;
            bounds_.y = pb.y;
            bounds_.w = pb.w - 1.0f;
            // bounds_.h left as-set by page (full page height)
        }

        // Strip height scales with the GUI scale so the row stays
        // proportional on high-DPI monitors.
        const float k = style::gui_scale();
        const float scaled_strip_h = strip_h_ * k;

        const float btn_w = bounds_.w / static_cast<float>(n_int);
        const float btn_h = scaled_strip_h;
        const float y     = bounds_.y;

        auto& in  = input::current();
        auto& cur = input::cursor();

        // Modal guard — popup/dialog freezes the icon strip.
        const bool input_blocked = in.input_blocked_by_other(this);

        if (static_cast<int>(hover_anim_.size()) != n_int)
            hover_anim_.assign(n_int, 0.0f);
        if (static_cast<int>(active_anim_.size()) != n_int)
            active_anim_.assign(n_int, 0.0f);

        page_anim_alpha_ += (1.0f - page_anim_alpha_) * std::clamp(dt * 18.0f, 0.0f, 1.0f);

        bool any_hover = false;
        for (int i = 0; i < n_int; ++i) {
            const float x = bounds_.x + static_cast<float>(i) * btn_w;
            Renderer::rect br{x, y, btn_w, btn_h};
            const bool hov = !input_blocked && br.contains(in.mouse_pos);
            if (hov) {
                any_hover = true;
                if (in.mouse_pressed[input::mb_left]) {
                    if (active_ != i) {
                        active_ = i;
                        page_anim_alpha_ = 0.0f;
                    }
                    in.consume_mouse_press();
                }
            }
            // Per-tab hover fade
            const float target_hov = (hov && i != active_) ? 1.0f : 0.0f;
            hover_anim_[i] += (target_hov - hover_anim_[i]) *
                              std::clamp(dt * 12.0f * style::anim_speed(), 0.0f, 1.0f);

            // Per-tab active state transition
            const float target_act = (i == active_) ? 1.0f : 0.0f;
            active_anim_[i] += (target_act - active_anim_[i]) *
                               std::clamp(dt * 14.0f * style::anim_speed(), 0.0f, 1.0f);
        }
        if (any_hover) cur.set(input::cursor_type::hand);

        // Smooth active indicator geometry interpolation (relative to bounds_.x)
        float target_rel_x = static_cast<float>(active_) * btn_w;
        float target_aw    = (active_ == n_int - 1) ? (bounds_.w - target_rel_x) : btn_w;
        if (active_ == 0) {
            target_rel_x -= 1.0f;
            target_aw    += 1.0f;
        }

        if (!anim_init_) {
            anim_rel_x_ = target_rel_x;
            anim_w_     = target_aw;
            anim_init_  = true;
        } else {
            const float speed = std::clamp(dt * 20.0f, 0.0f, 1.0f);
            anim_rel_x_ += (target_rel_x - anim_rel_x_) * speed;
            anim_w_     += (target_aw - anim_w_) * speed;
        }

        // Lay out the active child page in the area below the strip.
        rect_t page_bounds{
            bounds_.x,
            bounds_.y + scaled_strip_h,
            bounds_.w,
            bounds_.h - scaled_strip_h,
        };

        for (std::size_t i = 0; i < children_.size(); ++i) {
            auto& c = children_[i];
            if (!c) continue;
            const bool active = (static_cast<int>(i) == active_);
            c->set_visible(active);
            if (active) {
                c->set_bounds(page_bounds);
                c->update(dt);
            }
        }
    }

    // Render rule per user 2026-05-31 spec (Rage > Weapons screenshot):
    //   * Strip height 60 px. Each tab 79 × 60.
    //   * NO inter-tab dividers — inactive tabs sit on the dark strip
    //     background with no visible separation.
    //   * Inactive: text/icon dim grey (#7A7A7A), no fill.
    //   * Active: 2-px border (outer 0xFF3E3E42 light + inner 0xFF202024
    //     dark) on top + left + right edges. BOTTOM edge is OPEN — the
    //     selected tab's content-area-coloured fill bleeds into the
    //     panel below (same idea as the side tab manager).
    //   * Active text/icon: pure white (#FFFFFF).
    //   * Icon dominates the upper 2/3 of the tab; text sits in the
    //     bottom 1/3 in a small font.
    void render(Renderer::c_renderer_core& r) override
    {
        if (!visible_) return;

        const int n_int = static_cast<int>(entries_.size());
        if (n_int <= 0) return;

        const Renderer::color strip_bg      {0x18, 0x18, 0x1A, 0xFF};
        const Renderer::color active_fill   {0x23, 0x23, 0x28, 0xFF};   // matches page bg
        const Renderer::color active_text   {0xFF, 0xFF, 0xFF, 0xFF};
        const Renderer::color inactive_text {0x7A, 0x7A, 0x7A, 0xFF};
        const Renderer::color border_outer  {0x3E, 0x3E, 0x42, 0xFF};   // lighter grey
        const Renderer::color border_inner  {0x20, 0x20, 0x24, 0xFF};   // darker grey

        const float btn_w = bounds_.w / static_cast<float>(n_int);
        const float btn_h = strip_h_ * style::gui_scale();
        const float y     = bounds_.y;

        // Pass 1: strip background. Use the full parent width — the
        // strip already overrode its bounds to the page's outer bounds
        // in update(), so this fills from the menu's left tab-strip
        // border to its right border with no gaps.
        const float strip_right = bounds_.x + bounds_.w;
        r.render_filled_rect({bounds_.x, y, bounds_.w, btn_h}, strip_bg);

        // Per-tab geometry. The LAST tab grows to absorb any pixel left
        // over from `bounds_.w / n_int` integer rounding so the active
        // fill + walls always meet the menu's right border exactly.
        auto tab_x = [&](int i) {
            return bounds_.x + static_cast<float>(i) * btn_w;
        };
        auto tab_w = [&](int i) {
            return (i == n_int - 1)
                ? (strip_right - tab_x(i))
                : btn_w;
        };

        // Pass 2: active tab fill (smoothly animated position & width)
        float ax = bounds_.x + anim_rel_x_;
        float aw = anim_w_;
        {
            const float r_edge = std::round(ax + aw);
            ax = std::round(ax);
            aw = r_edge - ax;
        }
        const float ay  = std::round(y);
        const float abh = std::round(y + btn_h) - ay;
        r.render_filled_rect({ax, ay, aw, abh}, active_fill);

        // Pass 3: active tab border. 2-px frame on top + left + right.
        // Bottom is OPEN.
        r.render_filled_rect({ax,        ay,        aw,        1.0f}, border_outer);
        r.render_filled_rect({ax + 1.0f, ay + 1.0f, aw - 2.0f, 1.0f}, border_inner);
        r.render_filled_rect({ax,        ay,        1.0f, abh - 1.0f}, border_outer);
        r.render_filled_rect({ax + 1.0f, ay + 1.0f, 1.0f, abh - 2.0f}, border_inner);
        r.render_filled_rect({ax + aw - 1.0f, ay,        1.0f, abh - 1.0f}, border_outer);
        r.render_filled_rect({ax + aw - 2.0f, ay + 1.0f, 1.0f, abh - 2.0f}, border_inner);

        // Pass 3b: horizontal divider at the strip baseline drawn
        // ACROSS all inactive tabs, BROKEN by the moving active tab.
        const float div_y = y + btn_h;
        if (ax > bounds_.x) {
            r.render_filled_rect(
                {bounds_.x, div_y - 2.0f, ax - bounds_.x, 1.0f}, border_outer);
            r.render_filled_rect(
                {bounds_.x, div_y - 1.0f, ax - bounds_.x, 1.0f}, border_inner);
        }
        const float right_seg_x = ax + aw;
        if (right_seg_x < strip_right) {
            r.render_filled_rect(
                {right_seg_x, div_y - 2.0f, strip_right - right_seg_x, 1.0f},
                border_outer);
            r.render_filled_rect(
                {right_seg_x, div_y - 1.0f, strip_right - right_seg_x, 1.0f},
                border_inner);
        }

        // Pass 4: icon + text per tab with smooth color lerp.
        for (int i = 0; i < n_int; ++i) {
            const float x   = tab_x(i);
            const float w_i = tab_w(i);
            const Renderer::rect br{x, y, w_i, btn_h};

            const float hv    = (i < static_cast<int>(hover_anim_.size())) ? hover_anim_[i] : 0.0f;
            const float act_w = (i < static_cast<int>(active_anim_.size())) ? active_anim_[i] : ((i == active_) ? 1.0f : 0.0f);
            const float t     = std::clamp(std::max(hv, act_w), 0.0f, 1.0f);
            const Renderer::color col = Renderer::color::lerp(inactive_text, active_text, t);

            // Icon — upper 2/3 of the tab, centered.
            const float icon_band_h = btn_h * (2.0f / 3.0f);
            const float label_h     = font_ ? font_->pixel_height() : 12.0f;
            float label_y = br.y + icon_band_h
                          + ((btn_h - icon_band_h) - label_h) * 0.5f;
            if (entries_[i].icon != tab_icon_e::none) {
                const bool is_weapon =
                    entries_[i].icon >= tab_icon_e::pistol &&
                    entries_[i].icon <= tab_icon_e::autosniper;
                const bool is_player =
                    entries_[i].icon >= tab_icon_e::enemy &&
                    entries_[i].icon <= tab_icon_e::self;
                if (is_weapon && icon_font_) {
                    const float ph = icon_font_->pixel_height();
                    const float group_h = ph + 4.0f + label_h;
                    const float group_top = br.y + (btn_h - group_h) * 0.5f;
                    const Renderer::rect gbox{br.x, group_top, br.w, ph};
                    render_glyph_icon(r, icon_font_,
                                      weapon_codepoint(entries_[i].icon),
                                      gbox, col);
                    label_y = group_top + ph + 4.0f;
                } else if (is_player && player_icon_font()) {
                    const std::uint32_t cp =
                        0x41 + (static_cast<int>(entries_[i].icon) -
                                static_cast<int>(tab_icon_e::enemy));
                    const float ph = player_icon_font()->pixel_height();
                    const float group_h = ph + 4.0f + label_h;
                    const float group_top = br.y + (btn_h - group_h) * 0.5f;
                    const Renderer::rect gbox{br.x, group_top, br.w, ph};
                    render_glyph_icon(r, player_icon_font(), cp, gbox, col);
                    label_y = group_top + ph + 4.0f;
                } else {
                    const float icon_sz =
                        std::min(icon_band_h * 0.85f, w_i * 0.7f);
                    const Renderer::rect icon_rect{
                        br.x + (br.w - icon_sz) * 0.5f,
                        br.y + (icon_band_h - icon_sz) * 0.5f + 2.0f,
                        icon_sz, icon_sz,
                    };
                    render_icon(r, entries_[i].icon, icon_rect, (act_w > 0.5f));
                }
            }

            // Label — centered horizontally, at the per-type label_y.
            if (font_) {
                const float text_w = font_->measure_text(entries_[i].label);
                const float text_x = br.x + (br.w - text_w) * 0.5f;
                r.render_text(*font_, {text_x, label_y}, col, entries_[i].label.c_str());
            }
        }

        // Render the active child page with smooth fade-in
        for (std::size_t i = 0; i < children_.size(); ++i) {
            auto& c = children_[i];
            if (!c) continue;
            if (static_cast<int>(i) == active_) {
                r.push_alpha(page_anim_alpha_);
                c->render(r);
                r.pop_alpha();
            }
        }
    }

private:
    static void render_icon(Renderer::c_renderer_core& r, tab_icon_e icon,
                            Renderer::rect rect, bool active)
    {
        // Tint = active ? team-color : muted grey
        Renderer::color tint;
        switch (icon) {
            case tab_icon_e::enemy: tint = active ? Renderer::color{0xE0, 0x4A, 0x4A, 0xFF}
                                                  : Renderer::color{0xAA, 0xAA, 0xAA, 0xFF}; break;
            case tab_icon_e::team:  tint = active ? Renderer::color{0x4A, 0xC0, 0x6A, 0xFF}
                                                  : Renderer::color{0xAA, 0xAA, 0xAA, 0xFF}; break;
            case tab_icon_e::self:  tint = active ? style::current().accent
                                                  : Renderer::color{0xAA, 0xAA, 0xAA, 0xFF}; break;
            // Weapon icons — accent when active, dim grey otherwise.
            case tab_icon_e::pistol:
            case tab_icon_e::heavy:
            case tab_icon_e::shotgun:
            case tab_icon_e::smg:
            case tab_icon_e::rifle:
            case tab_icon_e::scout:
            case tab_icon_e::awp:
            case tab_icon_e::autosniper:
                tint = active ? style::current().accent
                              : Renderer::color{0xAA, 0xAA, 0xAA, 0xFF};
                break;
            default:                tint = Renderer::color{0xBE, 0xBE, 0xBE, 0xFF}; break;
        }

        const float cx = rect.x + rect.w * 0.5f;
        const float cy = rect.y + rect.h * 0.5f;
        const float s  = rect.w;          // overall scale

        switch (icon) {
            case tab_icon_e::enemy: {
                // Silhouette + crosshair overlay.
                const float head_r = s * 0.14f;
                const float head_cy = cy - s * 0.22f;
                r.render_filled_circle({cx, head_cy}, head_r, tint);
                Renderer::vec2 body[4] = {
                    {cx - s * 0.26f, cy + s * 0.30f},
                    {cx + s * 0.26f, cy + s * 0.30f},
                    {cx + s * 0.18f, head_cy + head_r + 1.0f},
                    {cx - s * 0.18f, head_cy + head_r + 1.0f},
                };
                r.render_filled_polygon(body, 4, tint);
                // Crosshair lines
                Renderer::color xcol = active ? Renderer::color{0xFF, 0xFF, 0xFF, 0xC0}
                                              : Renderer::color{0xFF, 0xFF, 0xFF, 0x60};
                r.render_line({cx - s * 0.38f, cy + s * 0.05f},
                              {cx + s * 0.38f, cy + s * 0.05f}, xcol, 1.0f);
                r.render_line({cx,            cy - s * 0.30f},
                              {cx,            cy + s * 0.40f}, xcol, 1.0f);
                break;
            }
            case tab_icon_e::team: {
                // Two overlapping silhouettes.
                const float head_r = s * 0.12f;
                auto draw_fig = [&](float ox) {
                    r.render_filled_circle({cx + ox, cy - s * 0.22f}, head_r, tint);
                    Renderer::vec2 body[4] = {
                        {cx + ox - s * 0.22f, cy + s * 0.30f},
                        {cx + ox + s * 0.22f, cy + s * 0.30f},
                        {cx + ox + s * 0.15f, cy - s * 0.10f + 1.0f},
                        {cx + ox - s * 0.15f, cy - s * 0.10f + 1.0f},
                    };
                    r.render_filled_polygon(body, 4, tint);
                };
                draw_fig(-s * 0.14f);
                draw_fig( s * 0.14f);
                break;
            }
            case tab_icon_e::self: {
                // Single forward-facing figure.
                const float head_r = s * 0.16f;
                r.render_filled_circle({cx, cy - s * 0.18f}, head_r, tint);
                Renderer::vec2 body[4] = {
                    {cx - s * 0.28f, cy + s * 0.34f},
                    {cx + s * 0.28f, cy + s * 0.34f},
                    {cx + s * 0.20f, cy - s * 0.02f},
                    {cx - s * 0.20f, cy - s * 0.02f},
                };
                r.render_filled_polygon(body, 4, tint);
                break;
            }
            case tab_icon_e::pistol:
            case tab_icon_e::heavy:
            case tab_icon_e::shotgun:
            case tab_icon_e::smg:
            case tab_icon_e::rifle:
            case tab_icon_e::scout:
            case tab_icon_e::awp:
            case tab_icon_e::autosniper: {
                // Render the appropriate glyph from the icon font.
                // The actual rendering happens out of band (this static
                // helper can't see icon_font_/codepoints — see the
                // overriding call site below).
                break;
            }
            default:
                break;
        }
    }

    std::uint32_t weapon_codepoint(tab_icon_e icon) const {
        const int idx = static_cast<int>(icon) - static_cast<int>(tab_icon_e::pistol);
        return (idx >= 0 && idx < 8) ? weapon_codepoints_[idx] : 0;
    }

    // Render one font-glyph icon (weapon silhouettes from csgo_icons,
    // Enemy/Team/Self persons from the Glyphter font), centred in rect.
    static void render_glyph_icon(Renderer::c_renderer_core& r,
                                  const Renderer::font* fnt,
                                  std::uint32_t cp,
                                  Renderer::rect rect,
                                  Renderer::color tint)
    {
        if (!fnt || cp == 0) return;

        // Encode codepoint as UTF-8 so font.find_glyph picks it up.
        char utf8[5] = {};
        if (cp < 0x80) {
            utf8[0] = static_cast<char>(cp);
        } else if (cp < 0x800) {
            utf8[0] = static_cast<char>(0xC0 | (cp >> 6));
            utf8[1] = static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            utf8[0] = static_cast<char>(0xE0 |  (cp >> 12));
            utf8[1] = static_cast<char>(0x80 | ((cp >>  6) & 0x3F));
            utf8[2] = static_cast<char>(0x80 |  (cp        & 0x3F));
        } else {
            utf8[0] = static_cast<char>(0xF0 |  (cp >> 18));
            utf8[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            utf8[2] = static_cast<char>(0x80 | ((cp >>  6) & 0x3F));
            utf8[3] = static_cast<char>(0x80 |  (cp        & 0x3F));
        }

        // Centre the glyph in the icon rect. Use the font's own measure +
        // pixel_height to handle any glyph dimensions. Tint is supplied by
        // the caller so the glyph tracks the label's hover fade.
        const float tw = fnt->measure_text(utf8);
        const float th = fnt->pixel_height();
        r.render_text(*fnt,
                      Renderer::vec2{rect.x + (rect.w - tw) * 0.5f,
                                     rect.y + (rect.h - th) * 0.5f},
                      tint, utf8);
    }

    std::vector<entry_t>  entries_;
    std::vector<float>    hover_anim_;   // per-tab hover fade (inactive tabs)
    std::vector<float>    active_anim_;  // per-tab active transition
    int                   active_ = 0;
    float                 anim_rel_x_ = 0.0f;
    float                 anim_w_ = 0.0f;
    bool                  anim_init_ = false;
    float                 page_anim_alpha_ = 1.0f;
    // 60 px per user spec for Rage > Weapons strip (each tab 79 × 60).
    // Other strips (Visuals Enemy/Team/Self) can override via the
    // future set_strip_height() once a need comes up.
    float                 strip_h_ = 60.0f;
    const Renderer::font* font_      = nullptr;
    const Renderer::font* icon_font_ = nullptr;

    // Codepoints in the carved binary font (exported_textures/
    // csgo_icons.otf, extracted via scripts/extract_icon_font.py):
    // glyphs live at 0xE000 + CS:GO weapon item id. Defaults pick a
    // representative weapon per category matching weapons.png; override
    // per-glyph via set_weapon_glyph().
    std::uint32_t weapon_codepoints_[8] = {
        0xE004,  // Pistol  — Glock-18   (id 4)
        0xE001,  // Heavy   — Desert Eagle (id 1)
        0xE023,  // Shotgun — Nova       (id 35)
        0xE018,  // SMG     — UMP-45     (id 24)
        0xE007,  // Rifle   — AK-47      (id 7)
        0xE028,  // Scout   — SSG 08     (id 40)
        0xE009,  // AWP                  (id 9)
        0xE026,  // Auto    — SCAR-20    (id 38)
    };
};

} // namespace GUI
