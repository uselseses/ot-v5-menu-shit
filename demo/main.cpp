// =============================================================================
// demo/main.cpp --- WinMain. Exact reversal from GUI_64_DX11.exe binary.
// =============================================================================

#include <Windows.h>
#include <psapi.h>
#include <timeapi.h>
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "winmm.lib")
#include <initializer_list>
#include <vector>
#include <cmath>

#include <GUI/framework.hpp>
#include <GUI/core/element_factory.hpp>
#include <GUI/core/globals.hpp>
#include <GUI/core/layout_context.hpp>
#include <GUI/input/input_state.hpp>
#include <GUI/input/cursor.hpp>
#include <GUI/elements/c_slider.hpp>
#include <GUI/elements/c_dropdown.hpp>
#include <GUI/elements/c_colorpicker.hpp>
#include <GUI/elements/c_esp_row.hpp>
#include <GUI/elements/c_listbox.hpp>
#include <GUI/elements/c_keybinder.hpp>
#include <GUI/elements/c_textbox_new.hpp>
#include <GUI/elements/c_text_container.hpp>
#include <GUI/elements/c_button.hpp>
#include <GUI/elements/c_button_grid.hpp>
#include <GUI/elements/c_column.hpp>
#include <GUI/elements/c_icon_tab_strip.hpp>
#include <GUI/elements/c_separator.hpp>
#include <GUI/containers/c_dialog_window.hpp>
#include <GUI/pages/antiaim_groupbox_t.hpp>
#include <GUI/pages/legit_weapon_page_t.hpp>
#include <GUI/pages/rage_weapon_page_t.hpp>
#include <GUI/pages/dialog_window_builder_t.hpp>
#include <GUI/menu/cheat_menu_builder.hpp>
#include <GUI/pages/enemy_visuals_page_t.hpp>
#include <GUI/pages/team_visuals_page_t.hpp>
#include <GUI/pages/self_visuals_page_t.hpp>
#include <GUI/enums/antiaim_condition_e.hpp>
#include <GUI/style/rounding.hpp>
#include <GUI/style/element_sizes.hpp>
#include <GUI/style/colorscheme.hpp>

#include <Renderer/c_dx11_renderer.hpp>
#include <Renderer/c_renderer_core.hpp>
#include <Renderer/color.hpp>
#include <Renderer/font.hpp>
#include <Renderer/texture.hpp>
#include <GUI/style/textures.hpp>

#include "window.hpp"
#include "d3d_setup.hpp"
#include "embedded_assets.h"   // self-contained asset blobs (no asset dir)

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>

namespace {

using namespace GUI;

// =============================================================================
// EXACT ANIMATION FORMULAS FROM BINARY
// =============================================================================

// Exponential ease-in-out from binary @ 0x14000B210
float exponential_ease_in_out(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    if (t < 0.5f) return std::pow(2.0f, t * 20.0f - 10.0f) * 0.5f;
    return (2.0f - std::pow(2.0f, 10.0f - t * 20.0f)) * 0.5f;
}

// Circle demo easing from 0x14000D440 - cubic ease-in-out
float circle_ease(float t) {
    if (t < 0.5f) return 4.0f * t * t * t;
    return 1.0f - std::pow(2.0f - 2.0f * t, 3.0f) * 0.5f;
}

// 3 second period animation (polygon demo)
float get_polygon_anim(float t_secs) {
    float time_ms = std::fmod(t_secs * 1000.0f, 3000.0f);
    float t = time_ms / 3000.0f;
    t = (t <= 0.5f) ? t : (1.0f - t);
    t = t * 2.0f;
    return exponential_ease_in_out(t);
}

// 5 second period animation (circle demo) - from 0x14000D440
float get_circle_anim(float t_secs) {
    float time_mod = std::fmod(t_secs * 1000.0f, 5000.0f);
    float sine_input = time_mod * 0.00062831852f;  // 2*PI/10000
    float t = std::sin(sine_input);
    t = (t + 1.0f) * 0.5f;  // normalize to 0-1
    return circle_ease(t);
}

float get_bezier_anim(float t_secs) {
    const float time_ms = std::fmod(t_secs * 1000.0f, 3000.0f);
    const float s = std::sin(time_ms * 0.0010471976f);
    if (s >= 0.5f) {
        return 1.0f - std::pow(2.0f - (s + s), 4.0f) * 0.5f;
    }
    return s * ((s * 8.0f) * s) * s;
}

Renderer::color hsv_to_rgb(float hue_deg, float sat = 1.0f, float val = 1.0f,
                           std::uint8_t alpha = 255)
{
    hue_deg = std::fmod(hue_deg, 360.0f);
    if (hue_deg < 0.0f) hue_deg += 360.0f;

    const float c = val * sat;
    const float x = c * (1.0f - std::fabs(std::fmod(hue_deg / 60.0f, 2.0f) - 1.0f));
    const float m = val - c;

    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (hue_deg < 60.0f) {
        r = c; g = x;
    } else if (hue_deg < 120.0f) {
        r = x; g = c;
    } else if (hue_deg < 180.0f) {
        g = c; b = x;
    } else if (hue_deg < 240.0f) {
        g = x; b = c;
    } else if (hue_deg < 300.0f) {
        r = x; b = c;
    } else {
        r = c; b = x;
    }

    return Renderer::color{
        static_cast<std::uint8_t>((r + m) * 255.0f),
        static_cast<std::uint8_t>((g + m) * 255.0f),
        static_cast<std::uint8_t>((b + m) * 255.0f),
        alpha,
    };
}

void render_ring_segment(Renderer::c_renderer_core& core, Renderer::vec2 center,
                         float inner_r, float outer_r, float start, float sweep,
                         Renderer::color c, int segments = 40)
{
    if (inner_r <= 0.0f || outer_r <= inner_r || sweep == 0.0f) return;
    std::vector<Renderer::vec2> pts;
    pts.reserve(static_cast<std::size_t>(segments + 1) * 2);
    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float a = start + sweep * t;
        pts.push_back({center.x + std::cos(a) * outer_r,
                       center.y + std::sin(a) * outer_r});
    }
    for (int i = segments; i >= 0; --i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float a = start + sweep * t;
        pts.push_back({center.x + std::cos(a) * inner_r,
                       center.y + std::sin(a) * inner_r});
    }
    core.render_filled_polygon(pts.data(), static_cast<int>(pts.size()), c);
}

void render_demo_ring(Renderer::c_renderer_core& core, Renderer::vec2 center,
                      float radius, int type, float phase,
                      Renderer::color a, Renderer::color b)
{
    constexpr float pi = 3.14159265358979323846f;
    const float thick = type == 0 ? 4.0f : 12.0f;
    const float inner = radius - thick;
    switch (type) {
        case 0:
            render_ring_segment(core, center, radius - 4.0f, radius, 0.0f, 2.0f * pi, a, 64);
            break;
        case 1:
            render_ring_segment(core, center, radius - 8.0f, radius, 0.0f, 2.0f * pi, a, 64);
            break;
        case 2:
            render_ring_segment(core, center, inner, radius, 0.0f, 2.0f * pi, a, 64);
            break;
        case 3:
        default:
            render_ring_segment(core, center, inner, radius, 0.0f, pi, a, 32);
            render_ring_segment(core, center, inner, radius, pi, pi * 0.5f, b, 16);
            render_ring_segment(core, center, inner, radius, pi * 1.5f, pi * 0.5f,
                                Renderer::color::lerp(a, b, 0.5f), 16);
            render_ring_segment(core, center, inner, radius, phase, pi * 0.02f,
                                Renderer::color{255, 255, 255, 80}, 4);
            break;
    }
}

// =============================================================================
// EXACT COLORS FROM BINARY @ 0x1401D6CE8
// =============================================================================
const Renderer::color circle_colors[] = {
    {0xAC, 0x66, 0xE2, 0xFF},  // 0xFFE266AC - pink
    {0x66, 0x67, 0xFF, 0xFF},  // 0xFFFF6766 - red
    {0x34, 0x90, 0xFF, 0xFF},  // 0xFFFF9034 - orange
    {0x35, 0xBB, 0xFF, 0xFF},  // 0xFFFFBB35 - yellow
    {0xCE, 0x6B, 0xAA, 0xFF},  // 0xFFAA6BCE - purple
    {0xD0, 0x71, 0x8B, 0xFF},  // 0xFF8B71D0 - purple
    {0xAE, 0xAD, 0x34, 0xFF},  // 0xFF34ADAE - teal
    {0xAB, 0x41, 0x12, 0xFF},  // 0xFF1241AB - blue
};

// Hardcoded circle 5 colors
const Renderer::color circle5_inner{0x97, 0x58, 0xA0, 0xFF};  // 0xFFA05897
const Renderer::color circle5_outer{0x18, 0x35, 0xFA, 0xFF};  // 0xFFFA3518

bool g_request_unload = false;
// Changelogs are now a c_dialog_window (replaces the old custom
// `draw_changelog_popup` raw renderer). Build on first menu construction;
// "Display changelogs" button calls ->open() to surface it.
static std::shared_ptr<c_dialog_window> g_changelog_dialog;
// Live toggles wired into the GUI > Style > Theme groupbox.
bool g_show_watermark = true;
// Profile avatar texture handle — shared with the Spectators overlay
// rows (set in wWinMain after texture upload).
Renderer::texture_handle g_profile_tex{};

// FPS cap — bound to the demos-page "fps_limit" slider and enforced by
// the render loop's frame limiter. Original default: 300 FPS.
std::shared_ptr<c_slider<int>::state_t> g_fps_limit_state = [] {
    auto s = std::make_shared<c_slider<int>::state_t>();
    s->set(300);
    return s;
}();
std::shared_ptr<c_keybinder::state_t> g_watermark_kb_state =
    std::make_shared<c_keybinder::state_t>();

// Menu visibility + Open-GUI keybind. The keybinder lives on
// Settings > GUI. WinMain polls it edge-triggered so each press toggles
// g_menu_visible. Default to bound to INSERT (0x2D) for first run.
bool g_menu_visible = true;
std::shared_ptr<c_keybinder::state_t> g_menu_kb_state = [] {
    auto s = std::make_shared<c_keybinder::state_t>();
    s->set(GUI::keybind_state{0x2D, GUI::keybinder_mode_e::toggle}.pack());
    return s;
}();

struct demo_layout {
    std::vector<std::shared_ptr<c_menu_window>> windows;
    std::shared_ptr<c_tab_manager>              frame_tabs;
};

// =============================================================================
// Keybind indicator registry — user 2026-06-01:
// HUD indicators on the left of the screen render ONLY for keybinds that
// are currently active (Always always; Toggle when latched; Hold while
// the key is held). When a registered keybind goes inactive its row
// fades back out.
//
// Demo code calls `register_indicator_kb(kb, "DT", color{...})` after
// creating any keybinder it wants visible in the HUD.
// =============================================================================
struct indicator_entry {
    std::weak_ptr<GUI::c_keybinder> kb;
    std::string                     short_tag;
    Renderer::color                 tint;
    // Per-entry 0..1 visibility tween — eased toward 1 while active,
    // toward 0 while not.
    float                           anim = 0.0f;
};
std::vector<indicator_entry> g_indicator_kbs;

void register_indicator_kb(const std::shared_ptr<GUI::c_keybinder>& kb,
                           const char* short_tag,
                           Renderer::color tint)
{
    g_indicator_kbs.push_back({kb, short_tag, tint, 0.0f});
}

// Demo dialog — populated in build_framework_window, opened by the
// "Open dialog" button in the Framework > Elements > Dialog window
// groupbox. Used as a shared_ptr so the button lambda can capture it
// by value (avoids dangling pointer issues with the global).
static std::shared_ptr<c_dialog_window> g_demo_dialog;

// =============================================================================
// Renderer-demo toggles — user 2026-06-01: wire the framework demos
// groupbox (renderer subtab) to its render code. Each checkbox flips
// its shared state; the draw routine reads these to gate the
// corresponding render block. Defaults match the binary's initial
// state (all demos ON, control-points OFF except when bezier on).
// =============================================================================
auto make_cb_state(bool on) {
    return std::make_shared<GUI::c_checkbox::state_t>(on);
}
std::shared_ptr<GUI::c_checkbox::state_t> g_demo_unicode_text       = make_cb_state(true);
std::shared_ptr<GUI::c_checkbox::state_t> g_demo_text_drop_shadow   = make_cb_state(true);
std::shared_ptr<GUI::c_checkbox::state_t> g_demo_bezier_curve       = make_cb_state(true);
std::shared_ptr<GUI::c_checkbox::state_t> g_demo_bezier_ctrl_pts    = make_cb_state(true);
std::shared_ptr<GUI::c_checkbox::state_t> g_demo_shaders            = make_cb_state(false);
std::shared_ptr<GUI::c_checkbox::state_t> g_demo_polygon            = make_cb_state(true);
std::shared_ptr<GUI::c_checkbox::state_t> g_demo_texture            = make_cb_state(true);
std::shared_ptr<GUI::c_checkbox::state_t> g_demo_gradient_circle    = make_cb_state(true);
std::shared_ptr<GUI::c_slider<float>::state_t> g_demo_bezier_scale  =
    std::make_shared<GUI::c_slider<float>::state_t>(1.0f);

// Weapon-icon font (csgo_icons.otf — user drops it in
// OTV5_ASSET_DIR/csgo_icons.otf). When loaded, the Rage > Weapons icon
// strip uses it to render the per-weapon glyph; otherwise the strip
// falls back to the text label only.
static const Renderer::font* g_weapon_icon_font = nullptr;

// --- Build literal demo tab content --------------------------------------
static std::shared_ptr<c_column> add_columns(std::shared_ptr<c_page>& page,
                                             int count = 2)
{
    auto cols = element_factory::make<c_column>(count);
    page->add_child(cols);
    return cols;
}

static void add_label_line(std::shared_ptr<c_groupbox>& gb,
                           const Renderer::font& font, const std::string& s)
{
    auto l = element_factory::make<c_label>(s);
    l->set_font(&font);
    gb->add_child(l);
}

// ---- "demos" tab / "renderer" subtab ------------------------------------
// ---------------------------------------------------------------------------
// Helpers shared by the dev / Framework populators — all three mirror
// gui_build_menu (sub_14006F580):
//  * the binary builds ONE 20-entry "Item A".."Item T" vector up front and
//    reuses it for every dropdown/listbox in dev+Framework;
//  * dynamic labels are "<prefix><letter>" via the index→letter formatter
//    sub_140072FF0;
//  * showcase colorpickers get a random color (sub_14000DDE0) — we use a
//    fixed-seed LCG so screenshots stay reproducible.
// ---------------------------------------------------------------------------
static std::vector<std::string> demo_item_names()
{
    std::vector<std::string> v;
    for (int i = 0; i < 20; ++i)
        v.push_back(std::string{"Item "} + static_cast<char>('A' + i));
    return v;
}

static std::string letter_label(const char* prefix, int i)
{
    return std::string{prefix} + static_cast<char>('A' + i);
}

static Renderer::color demo_random_color()
{
    static std::uint32_t s = 0x18A6FA;          // fixed seed → stable captures
    auto next = [&] { s = s * 1664525u + 1013904223u; return (s >> 16) & 0xFF; };
    const std::uint8_t r = static_cast<std::uint8_t>(next());
    const std::uint8_t g = static_cast<std::uint8_t>(next());
    const std::uint8_t b = static_cast<std::uint8_t>(next());
    return Renderer::color{r, g, b, 0xFF};
}

// Set from WinMain after init_d3d11 so the dev > renderer "runtime info"
// groupbox can format the real pointers like the binary's
// "g_device addr: {}" labels do.
const void* g_rt_device_addr     = nullptr;
const void* g_rt_context_addr    = nullptr;
const void* g_rt_swap_chain_addr = nullptr;

static std::string fmt_addr(const char* fmt_prefix, const void* p)
{
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s0x%p", fmt_prefix, p);
    return std::string{buf};
}

void populate_renderer_subtab(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);

    // demos groupbox — element ORDER per the live dump (menu_dev.json):
    // unicode / drop shadow, ---, bezier + scale + control points, ---,
    // shaders / polygon / texture / gradient circle, ---, fps_limit.
    auto demos_gb = element_factory::make<c_groupbox>("demos");
    demos_gb->set_title_font(&font);
    demos_gb->set_bounds({0, 0, 0, 300});
    cols->add_child(demos_gb);

    auto add_demo_cb = [&](const char* label,
                           std::shared_ptr<c_checkbox::state_t>& st) {
        auto cb = std::make_shared<c_checkbox>(label, st);
        cb->set_font(&font);
        demos_gb->add_child(cb);
    };
    auto add_sep = [&] {
        auto sep = element_factory::make<c_separator>();
        sep->set_bounds({0, 0, 0, 8});
        demos_gb->add_child(sep);
    };

    add_demo_cb("unicode_text_demo",     g_demo_unicode_text);
    add_demo_cb("text_drop_shadow_demo", g_demo_text_drop_shadow);
    add_sep();
    add_demo_cb("bezier_curve_demo", g_demo_bezier_curve);
    // Bound to g_demo_bezier_scale — read by draw_demo_overlay.
    // Dump range: 0..2 step 0.01, suffix "x".
    {
        auto bcs = std::make_shared<c_slider<float>>(
            "bezier_curve_scale", 0.0f, 2.0f, g_demo_bezier_scale, "x");
        bcs->set_font(&font);
        bcs->set_bounds({0, 0, 0, 32});
        demos_gb->add_child(bcs);
    }
    add_demo_cb("render_bezier_curve_control_points", g_demo_bezier_ctrl_pts);
    add_sep();
    add_demo_cb("shaders_demo",         g_demo_shaders);
    add_demo_cb("polygon_demo",         g_demo_polygon);
    add_demo_cb("texture_demo",         g_demo_texture);
    add_demo_cb("gradient_circle_demo", g_demo_gradient_circle);
    add_sep();
    // BOUND + ENFORCED — the render loop reads g_fps_limit_state and caps
    // frame time. Dump range: 50..5000 step 50; the original displays 300.
    {
        auto fps = element_factory::make<c_slider<int>>(
            "fps_limit", 50, 5000, g_fps_limit_state, " FPS");
        fps->set_font(&font);
        fps->set_bounds({0, 0, 0, 32});
        demos_gb->add_child(fps);
    }

    // version groupbox — label order per dump: DX11 first, then the
    // dynamically-formatted build info lines, then Display changelogs.
    auto ver_gb = element_factory::make<c_groupbox>("version");
    ver_gb->set_title_font(&font);
    ver_gb->set_bounds({0, 0, 0, 180});
    cols->add_child(ver_gb);
    add_label_line(ver_gb, font, "DX11");
    add_label_line(ver_gb, font, "GUI_LIBRARY_VERSION: v0.4.0");
    add_label_line(ver_gb, font, "GUI_ARCH_NAME: 64-bit");
    add_label_line(ver_gb, font, "Compilation date: Sep 14 2022 22:59:49");
    add_label_line(ver_gb, font, "GUI_COMPILER_NAME: MSVC 16.0");
    {
        auto changelogs = element_factory::make<c_button>("Display changelogs", [] {
            if (g_changelog_dialog) g_changelog_dialog->open();
        });
        changelogs->set_font(&font);
        changelogs->set_bounds({0, 0, 0, 24});
        ver_gb->add_child(changelogs);
    }

    // runtime info groupbox — separator + the three pointer labels,
    // formatted from the live device like the binary's "g_device addr: {}".
    auto rt_gb = element_factory::make<c_groupbox>("runtime info");
    rt_gb->set_title_font(&font);
    rt_gb->set_bounds({0, 0, 0, 110});
    cols->add_child(rt_gb);
    {
        auto sep = element_factory::make<c_separator>();
        sep->set_bounds({0, 0, 0, 8});
        rt_gb->add_child(sep);
    }
    add_label_line(rt_gb, font, fmt_addr("g_device addr: ",         g_rt_device_addr));
    add_label_line(rt_gb, font, fmt_addr("g_device_context addr: ", g_rt_context_addr));
    add_label_line(rt_gb, font, fmt_addr("g_swap_chain addr: ",     g_rt_swap_chain_addr));
}

void populate_asdf_subtab(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);
    const auto items = demo_item_names();

    // c_textbox_new — TWO textboxes "c_textbox_new label A/B"; the first
    // is pre-filled with "Textbox value 123" (gui_build_menu loop j<2,
    // even index gets the value).
    auto tb_gb = element_factory::make<c_groupbox>("c_textbox_new");
    tb_gb->set_title_font(&font); tb_gb->set_bounds({0,0,0,110});
    cols->add_child(tb_gb);
    for (int j = 0; j < 2; ++j) {
        auto tb = element_factory::make<c_textbox_new>(
            letter_label("c_textbox_new label ", j), "Placeholder text");
        tb->set_font(&font);
        if ((j & 1) == 0) tb->set_value("Textbox value 123");
        tb_gb->add_child(tb);
    }

    // c_dropdown — TWO dropdowns "c_dropdown label A/B" over the shared
    // Item A..T vector, default selection index 1.
    auto dd_gb = element_factory::make<c_groupbox>("c_dropdown");
    dd_gb->set_title_font(&font); dd_gb->set_bounds({0,0,0,110});
    cols->add_child(dd_gb);
    for (int k = 0; k < 2; ++k) {
        auto dd = element_factory::make<c_dropdown>(
            letter_label("c_dropdown label ", k), items);
        dd->set_font(&font); dd->set_bounds({0,0,0,36}); dd->set(1);
        dd_gb->add_child(dd);
    }

    // c_colorpicker — SEVEN pickers "c_colorpicker label A..G", random
    // colors (binary sub_14000DDE0).
    auto cp_gb = element_factory::make<c_groupbox>("c_colorpicker");
    cp_gb->set_title_font(&font); cp_gb->set_bounds({0,0,0,220});
    cols->add_child(cp_gb);
    for (int m = 0; m < 7; ++m) {
        auto cp = element_factory::make<c_colorpicker>(
            letter_label("c_colorpicker label ", m), demo_random_color());
        cp->set_font(&font);
        cp_gb->add_child(cp);
    }

    // c_checkbox — SEVEN checkboxes "c_checkbox label A..G", even-indexed
    // (A/C/E/G) default-checked.
    auto cb_gb = element_factory::make<c_groupbox>("c_checkbox");
    cb_gb->set_title_font(&font); cb_gb->set_bounds({0,0,0,220});
    cols->add_child(cb_gb);
    for (int n = 0; n < 7; ++n) {
        auto cb = element_factory::make<c_checkbox>(
            letter_label("c_checkbox label ", n));
        cb->set_font(&font);
        if ((n & 1) == 0) cb->set(true);
        cb_gb->add_child(cb);
    }
}

// Per-frame visibility sync: the Checkbox showcase binds three labels'
// visibility to the "Checkbox" toggle (gui_build_menu loop ii<3 links each
// "Toggle element visibility test {A,B,C}" label to the checkbox state).
struct visibility_binder_t : c_base_element {
    std::shared_ptr<c_checkbox>                src;
    std::vector<std::shared_ptr<c_base_element>> targets;
    visibility_binder_t() : c_base_element{""} { set_bounds({0,0,0,0}); }
    std::string_view type_name() const override { return "visibility_binder"; }
    void update(float) override {
        if (!src) return;
        for (auto& t : targets) t->set_visible(src->get());
    }
    void render(Renderer::c_renderer_core&) override {}
};

// One showcase row — uses the shared c_esp_row composite (the same slot
// management as the cheat menu's label rows): checkbox LEFT of the text,
// keybinder chip + colorpicker swatches RIGHT-aligned on the same line,
// dropdown on its own full-width row beneath.
struct label_row_opts {
    bool checkbox  = false;
    bool checked   = false;   // dump default for the row checkbox
    bool keybinder = false;
    bool dropdown  = false;
    int  colorpickers = 0;
};

static std::shared_ptr<c_esp_row>
add_label_row(std::shared_ptr<c_groupbox>& gb, const Renderer::font& font,
              const char* text, const label_row_opts& o,
              const std::vector<std::string>& items)
{
    auto row = element_factory::make<c_esp_row>();
    row->set_label(text);
    row->set_font(&font);
    gb->add_child(row);
    if (o.checkbox) {
        auto cb = element_factory::make<c_checkbox>("");
        cb->set_font(&font);
        if (o.checked) cb->set(true);
        row->set_checkbox(cb);
    }
    if (o.keybinder) {
        auto kb = element_factory::make<c_keybinder>("");
        kb->set_font(&font);
        kb->set_bind(GUI::keybind_state{'A', GUI::keybinder_mode_e::toggle});
        row->set_keybinder(kb);
    }
    if (o.dropdown) {
        auto dd = element_factory::make<c_dropdown>(std::string{""}, items);
        dd->set_font(&font); dd->set(0);
        row->set_dropdown(dd);
    }
    for (int i = 0; i < o.colorpickers; ++i) {
        auto cp = element_factory::make<c_colorpicker>(
            letter_label("Hover tooltip demo (colorpicker ", i) + ")",
            demo_random_color());
        cp->set_font(&font);
        row->set_colorpicker(cp);
    }
    return row;
}

void populate_elements_subtab(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);
    const auto items = demo_item_names();

    // Groupbox ORDER per gui_build_menu: Checkbox, Textbox, Dialog window,
    // Slider, Colorpicker, Label, Button, Button grid, Keybinder, Dropdown,
    // Listbox.

    // ---- Checkbox ----------------------------------------------------------
    // checkbox "Checkbox" (default ON) + 3 labels whose visibility follows it.
    {
        auto gb = element_factory::make<c_groupbox>("Checkbox");
        gb->set_title_font(&font); gb->set_bounds({0,0,0,140});
        cols->add_child(gb);
        auto cb = element_factory::make<c_checkbox>("Checkbox");
        cb->set_font(&font); cb->set(true);
        gb->add_child(cb);
        auto binder = std::make_shared<visibility_binder_t>();
        binder->src = cb;
        for (int i = 0; i < 3; ++i) {
            auto l = element_factory::make<c_label>(
                letter_label("Toggle element visibility test ", i));
            l->set_font(&font);
            gb->add_child(l);
            binder->targets.push_back(l);
        }
        gb->add_child(binder);
    }

    // ---- Textbox -----------------------------------------------------------
    // FOUR textboxes "Textbox label A..D", placeholder "Placeholder text";
    // even-indexed (A, C) pre-filled with "Textbox value 123".
    {
        auto gb = element_factory::make<c_groupbox>("Textbox");
        gb->set_title_font(&font); gb->set_bounds({0,0,0,210});
        cols->add_child(gb);
        for (int j = 0; j < 4; ++j) {
            auto tb = element_factory::make<c_textbox_new>(
                letter_label("Textbox label ", j), "Placeholder text");
            tb->set_font(&font);
            if ((j & 1) == 0) tb->set_value("Textbox value 123");
            gb->add_child(tb);
        }
    }

    // ---- Dialog window -----------------------------------------------------
    // A 1-per-row button grid with two "Display a dialog window" buttons
    // (binary creates the grid then two button children; second carries the
    // modal flag).
    {
        auto gb = element_factory::make<c_groupbox>("Dialog window");
        gb->set_title_font(&font); gb->set_bounds({0,0,0,120});
        cols->add_child(gb);
        auto grid = element_factory::make<c_button_grid>(/*per_row=*/1);
        grid->set_bounds({0, 0, 0, 0});
        for (int i = 0; i < 2; ++i) {
            auto b = element_factory::make<c_button>("Display a dialog window", [] {
                if (g_demo_dialog) g_demo_dialog->open();
            });
            b->set_font(&font); b->set_bounds({0,0,0,24});
            grid->add_child(b);
        }
        gb->add_child(grid);
    }

    // ---- Slider ------------------------------------------------------------
    // FOUR sliders, all -1000..1000 step 50; defaults from the binary:
    // f32 = -750, f64 = -250, i32 = 250, i64 = 750.
    {
        auto gb = element_factory::make<c_groupbox>("Slider");
        gb->set_title_font(&font); gb->set_bounds({0,0,0,200});
        cols->add_child(gb);
        auto sf32 = element_factory::make<c_slider<float>>(
            "Slider<f32>", -1000.0f, 1000.0f, nullptr, "");
        sf32->set_font(&font); sf32->set_bounds({0,0,0,32}); sf32->set(-750.0f);
        gb->add_child(sf32);
        auto sf64 = element_factory::make<c_slider<double>>(
            "Slider<f64>", -1000.0, 1000.0, nullptr, "");
        sf64->set_font(&font); sf64->set_bounds({0,0,0,32}); sf64->set(-250.0);
        gb->add_child(sf64);
        auto si32 = element_factory::make<c_slider<int>>(
            "Slider<i32>", -1000, 1000, nullptr, "");
        si32->set_font(&font); si32->set_bounds({0,0,0,32}); si32->set(250);
        gb->add_child(si32);
        auto si64 = element_factory::make<c_slider<int>>(
            "Slider<i64>", -1000, 1000, nullptr, "");
        si64->set_font(&font); si64->set_bounds({0,0,0,32}); si64->set(750);
        gb->add_child(si64);
    }

    // ---- Colorpicker -------------------------------------------------------
    {
        auto gb = element_factory::make<c_groupbox>("Colorpicker");
        gb->set_title_font(&font); gb->set_bounds({0,0,0,100});
        cols->add_child(gb);
        auto cp1 = element_factory::make<c_colorpicker>(
            "Colorpicker", demo_random_color());
        cp1->set_font(&font); gb->add_child(cp1);
        auto cp2 = element_factory::make<c_colorpicker>(
            "Colorpicker (no alpha channel)", demo_random_color(),
            /*has_alpha=*/false);
        cp2->set_font(&font); gb->add_child(cp2);
    }

    // ---- Label -------------------------------------------------------------
    // Row sequence per gui_build_menu: plain "Label", separator,
    // "Label"+checkbox, "Label + checkbox + keybinder",
    // "Label + checkbox + dropdown", "Separators (new)", separator,
    // "Label + colorpicker(s)", "Label + checkbox + colorpicker(s)",
    // "Label + dropdown + colorpicker(s)",
    // "Label + checkbox + dropdown + colorpicker(s)".
    {
        auto gb = element_factory::make<c_groupbox>("Label");
        gb->set_title_font(&font); gb->set_bounds({0,0,0,360});
        cols->add_child(gb);
        auto add_sep = [&] {
            auto sep = element_factory::make<c_separator>();
            sep->set_bounds({0, 0, 0, 8});
            gb->add_child(sep);
        };
        add_label_row(gb, font, "Label", {}, items);
        add_sep();
        add_label_row(gb, font, "Label", {.checkbox=true, .checked=true}, items);
        add_label_row(gb, font, "Label + checkbox + keybinder",
                      {.checkbox=true, .keybinder=true}, items);
        add_label_row(gb, font, "Label + checkbox + dropdown",
                      {.checkbox=true, .dropdown=true}, items);
        add_label_row(gb, font, "Separators (new)", {}, items);
        add_sep();
        add_label_row(gb, font, "Label + colorpicker(s)",
                      {.colorpickers=2}, items);
        add_label_row(gb, font, "Label + checkbox + colorpicker(s)",
                      {.checkbox=true, .checked=true, .colorpickers=2}, items);
        add_label_row(gb, font, "Label + dropdown + colorpicker(s)",
                      {.dropdown=true, .colorpickers=2}, items);
        add_label_row(gb, font, "Label + checkbox + dropdown + colorpicker(s)",
                      {.checkbox=true, .dropdown=true, .colorpickers=2}, items);
    }

    // ---- Button ------------------------------------------------------------
    // ONE button — "Button (0)" (lazy static in the binary).
    {
        auto gb = element_factory::make<c_groupbox>("Button");
        gb->set_title_font(&font); gb->set_bounds({0,0,0,70});
        cols->add_child(gb);
        auto b = element_factory::make<c_button>("Button (0)");
        b->set_font(&font); b->set_bounds({0,0,0,24});
        gb->add_child(b);
    }

    // ---- Button grid -------------------------------------------------------
    // 2-per-row grid: "Button A".."Button F" plus a 7th full-width
    // "odd_nth_occupy_full_width (click)" (odd child count → last row
    // button spans the grid).
    {
        auto gb = element_factory::make<c_groupbox>("Button grid");
        gb->set_title_font(&font); gb->set_bounds({0,0,0,170});
        cols->add_child(gb);
        auto grid = element_factory::make<c_button_grid>(/*per_row=*/2);
        grid->set_bounds({0, 0, 0, 0});  // auto-size from children
        for (int i = 0; i < 6; ++i) {
            auto b = element_factory::make<c_button>(letter_label("Button ", i));
            b->set_font(&font); b->set_bounds({0, 0, 0, 24});
            grid->add_child(b);
        }
        auto wide = element_factory::make<c_button>(
            "odd_nth_occupy_full_width (click)");
        wide->set_font(&font); wide->set_bounds({0, 0, 0, 24});
        grid->add_child(wide);
        gb->add_child(grid);
    }

    // ---- Keybinder ---------------------------------------------------------
    // "Keybinder" = M4 hold (binary key 5 mode 2); "Keybinder (strict)" =
    // M5 toggle, strict (key 6 mode 1).
    {
        auto gb = element_factory::make<c_groupbox>("Keybinder");
        gb->set_title_font(&font); gb->set_bounds({0,0,0,90});
        cols->add_child(gb);
        auto kb = element_factory::make<c_keybinder>("Keybinder");
        kb->set_font(&font);
        kb->set_bind(GUI::keybind_state{VK_XBUTTON1,
                                        GUI::keybinder_mode_e::hold});
        gb->add_child(kb);
        auto kbs = element_factory::make<c_keybinder>("Keybinder (strict)",
                                                      nullptr, /*strict=*/true);
        kbs->set_font(&font);
        kbs->set_bind(GUI::keybind_state{VK_XBUTTON2,
                                         GUI::keybinder_mode_e::toggle});
        gb->add_child(kbs);
    }

    // ---- Dropdown ----------------------------------------------------------
    // THREE dropdowns over Item A..T: required (def E), searchable (def B),
    // multi (def C).
    {
        auto gb = element_factory::make<c_groupbox>("Dropdown");
        gb->set_title_font(&font); gb->set_bounds({0,0,0,150});
        cols->add_child(gb);
        auto d1 = element_factory::make<c_dropdown>("Dropdown (required)", items);
        d1->set_font(&font); d1->set_bounds({0,0,0,36}); d1->set(4);
        gb->add_child(d1);
        auto d2 = element_factory::make<c_dropdown>("Dropdown (searchable)", items);
        d2->set_font(&font); d2->set_bounds({0,0,0,36}); d2->set(1);
        gb->add_child(d2);
        auto d3 = element_factory::make<c_dropdown>("Dropdown (multi)", items);
        d3->set_font(&font); d3->set_bounds({0,0,0,36}); d3->set(2);
        d3->set_multi_select(true);
        gb->add_child(d3);
    }

    // ---- Listbox -----------------------------------------------------------
    // THREE listboxes over Item A..T: required/4, multi/5, searchable/6.
    {
        auto gb = element_factory::make<c_groupbox>("Listbox");
        gb->set_title_font(&font); gb->set_bounds({0,0,0,440});
        cols->add_child(gb);
        auto lb1 = element_factory::make<c_listbox>(
            "Listbox (required, show_n_items == 4)", items, 4, true);
        lb1->set_font(&font); gb->add_child(lb1);
        auto lb2 = element_factory::make<c_listbox>(
            "Listbox (multi, show_n_items == 5)", items, 5, false);
        lb2->set_font(&font); gb->add_child(lb2);
        auto lb3 = element_factory::make<c_listbox>(
            "Listbox (searchable, show_n_items == 6)", items, 6, false);
        lb3->set_font(&font); gb->add_child(lb3);
    }
}

void populate_debug_subtab(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);

    // BINARY (gui_build_menu.c:1357-1365): the Framework > Debug subtab
    // creates a single, EMPTY "GUI" groupbox — nothing else. The previous
    // Demos/Version/Unload content here was improvised and is gone.
    auto gb = element_factory::make<c_groupbox>("GUI");
    gb->set_title_font(&font);
    gb->set_bounds({0, 0, 0, 60});
    cols->add_child(gb);
}

// ============================================================================
// VISUALS — split into Players / World / Extra subtabs per user spec.
// ============================================================================
// Per-role helper: populate a sub-page with Players + ESP + Chams groupboxes
// for one role (Enemy / Team / Self). Used by the icon-strip below.
static void populate_visuals_role_subpage(std::shared_ptr<c_page>& page,
                                          const Renderer::font& font,
                                          const char* role)
{
    auto cols = add_columns(page);

    // Role-specific chams compound widget (4 colorpickers wired in ctor).
    {
        std::shared_ptr<c_groupbox> role_widget;
        const std::string r = role;
        if      (r == "Enemy") role_widget = std::make_shared<enemy_visuals_page_t>(&font);
        else if (r == "Team")  role_widget = std::make_shared<team_visuals_page_t>(&font);
        else                   role_widget = std::make_shared<self_visuals_page_t>(&font);
        cols->add_child(role_widget);
    }

    auto pl_gb = element_factory::make<c_groupbox>("Players");
    pl_gb->set_title_font(&font);
    pl_gb->set_bounds({0, 0, 0, 160});
    cols->add_child(pl_gb);
    for (auto* n : {"Visible", "Hidden", "Attachments", "History"}) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); pl_gb->add_child(cb);
    }

    auto esp_gb = element_factory::make<c_groupbox>("ESP");
    esp_gb->set_title_font(&font);
    esp_gb->set_bounds({0, 0, 0, 220});
    cols->add_child(esp_gb);
    for (auto* n : {"Enabled", "Box", "Name", "Health bar", "Weapon", "Distance"}) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); esp_gb->add_child(cb);
    }
}

void populate_visuals_players(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    // Horizontal icon strip at the top swaps Enemy/Team/Self role pages.
    auto strip = std::make_shared<c_icon_tab_strip>(
        std::vector<c_icon_tab_strip::entry_t>{
            {"Enemy", tab_icon_e::enemy},
            {"Team",  tab_icon_e::team},
            {"Self",  tab_icon_e::self},
        });
    strip->set_font(&font);

    static const char* roles[] = { "Enemy", "Team", "Self" };
    for (auto* role : roles) {
        auto sub = element_factory::make<c_page>("role");
        populate_visuals_role_subpage(sub, font, role);
        strip->add_child(sub);
    }
    strip->set_active_index(0);
    page->add_child(strip);
}

void populate_visuals_world(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);

    auto wld_gb = element_factory::make<c_groupbox>("World");
    wld_gb->set_title_font(&font);
    wld_gb->set_bounds({0, 0, 0, 300});
    cols->add_child(wld_gb);
    for (auto* n : {
            "Fullbright", "Nightmode", "No flash", "No smoke",
            "No scope", "No recoil", "No visual kick", "No visual punch",
        }) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); wld_gb->add_child(cb);
    }
    {
        auto wt = element_factory::make<c_slider<int>>("Wall transparency", 0, 100, nullptr, " %");
        wt->set_font(&font); wt->set_bounds({0,0,0,32}); wt->set(50);
        wld_gb->add_child(wt);
        auto pt = element_factory::make<c_slider<int>>("Prop transparency", 0, 100, nullptr, " %");
        pt->set_font(&font); pt->set_bounds({0,0,0,32}); pt->set(100);
        wld_gb->add_child(pt);
    }

    auto wt_gb = element_factory::make<c_groupbox>("Weather");
    wt_gb->set_title_font(&font);
    wt_gb->set_bounds({0, 0, 0, 100});
    cols->add_child(wt_gb);
    {
        auto dd = element_factory::make<c_dropdown>("Skybox",
            std::vector<std::string>{"Default", "Day", "Dusk", "Night", "Storm"});
        dd->set_font(&font); dd->set_bounds({0,0,0,36}); wt_gb->add_child(dd);
    }

    auto rm_gb = element_factory::make<c_groupbox>("Removals");
    rm_gb->set_title_font(&font);
    rm_gb->set_bounds({0, 0, 0, 200});
    cols->add_child(rm_gb);
    for (auto* n : {
            "Disable post processing", "Disable fog", "Disable cascade shadows",
            "Disable blood splatter", "Disable teammate rendering",
        }) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); rm_gb->add_child(cb);
    }
}

void populate_visuals_extra(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);

    auto view_gb = element_factory::make<c_groupbox>("View");
    view_gb->set_title_font(&font);
    view_gb->set_bounds({0, 0, 0, 380});
    cols->add_child(view_gb);
    for (auto* n : {
            "Thirdperson while spectating",
            "Disable thirdperson with grenades",
            "Freecam", "Flip knife hand", "Visualize silent angles",
            "Visual interpolation",
        }) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); view_gb->add_child(cb);
    }
    {
        auto fov = element_factory::make<c_slider<int>>("Field of view", 60, 120, nullptr, "");
        fov->set_font(&font); fov->set_bounds({0,0,0,32}); fov->set(90);
        view_gb->add_child(fov);
        auto sfov = element_factory::make<c_slider<int>>("Scoped field of view", 60, 120, nullptr, "");
        sfov->set_font(&font); sfov->set_bounds({0,0,0,32}); sfov->set(90);
        view_gb->add_child(sfov);
        auto td = element_factory::make<c_slider<int>>("Thirdperson distance", 50, 200, nullptr, " units");
        td->set_font(&font); td->set_bounds({0,0,0,32}); td->set(100);
        view_gb->add_child(td);
        auto zf = element_factory::make<c_slider<float>>("Zoom factor", 0.5f, 2.0f, nullptr, "x");
        zf->set_font(&font); zf->set_bounds({0,0,0,32}); zf->set(1.0f);
        view_gb->add_child(zf);
    }

    auto vm_gb = element_factory::make<c_groupbox>("Viewmodel");
    vm_gb->set_title_font(&font);
    vm_gb->set_bounds({0, 0, 0, 200});
    cols->add_child(vm_gb);
    {
        for (auto* n : {"X offset", "Y offset", "Z offset", "Roll"}) {
            auto s = element_factory::make<c_slider<int>>(n, -180, 180, nullptr, "");
            s->set_font(&font); s->set_bounds({0,0,0,32}); s->set(0);
            vm_gb->add_child(s);
        }
    }

    auto hp_gb = element_factory::make<c_groupbox>("Helpers");
    hp_gb->set_title_font(&font);
    hp_gb->set_bounds({0, 0, 0, 220});
    cols->add_child(hp_gb);
    for (auto* n : {
            "Build name", "Frames per second", "Latency to server",
            "System time", "Server map", "Server type", "Server tickrate",
            "Username", "Show watermark", "Show keybind states",
        }) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); hp_gb->add_child(cb);
    }
}

// Build one of the three Visuals pages (Enemy / Team / Self). Each page has
// the same shape but with role-appropriate groupbox labels — the per-team
// flag in c_player_visuals lives elsewhere; here we just populate the UI.
static void populate_visuals_role_page(std::shared_ptr<c_page>& page,
                                       const Renderer::font& font,
                                       const char* role)
{
    auto cols = add_columns(page);

    // Role-specific visuals widget per binary (sub_140085580/6B0/7E0).
    // Each one is a c_groupbox subclass with Visible/Attachments/Hidden/
    // History colorpickers wired in the constructor.
    {
        std::shared_ptr<c_groupbox> role_widget;
        const std::string r = role;
        if      (r == "Enemy") role_widget = std::make_shared<enemy_visuals_page_t>(&font);
        else if (r == "Team")  role_widget = std::make_shared<team_visuals_page_t>(&font);
        else                   role_widget = std::make_shared<self_visuals_page_t>(&font);
        cols->add_child(role_widget);
    }

    auto esp_gb = element_factory::make<c_groupbox>(std::string{"ESP — "} + role);
    esp_gb->set_title_font(&font);
    esp_gb->set_bounds({0, 0, 0, 200});
    cols->add_child(esp_gb);

    static const char* esp_options[] = {
        "Enabled",
        "Box",
        "Name",
        "Health bar",
        "Weapon",
        "Distance",
    };
    for (auto* n : esp_options) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font);
        esp_gb->add_child(cb);
    }

    auto chams_gb = element_factory::make<c_groupbox>(std::string{"Chams — "} + role);
    chams_gb->set_title_font(&font);
    chams_gb->set_bounds({0, 0, 0, 160});
    cols->add_child(chams_gb);
    {
        auto enable = element_factory::make<c_checkbox>("Enabled");
        enable->set_font(&font);
        chams_gb->add_child(enable);

        auto style = element_factory::make<c_dropdown>("Style",
            std::vector<std::string>{"Flat", "Textured", "Glow", "Wireframe"});
        style->set_font(&font);
        style->set_bounds({0, 0, 0, 36});
        chams_gb->add_child(style);

        auto vis_color = element_factory::make<c_colorpicker>(
            "Visible color", style::current().accent);
        vis_color->set_font(&font);
        chams_gb->add_child(vis_color);
    }

    // ---- Players (Visible/Hidden/Attachments/History) per binary tree ----
    auto pl_gb = element_factory::make<c_groupbox>(std::string{"Players — "} + role);
    pl_gb->set_title_font(&font);
    pl_gb->set_bounds({0, 0, 0, 130});
    cols->add_child(pl_gb);
    for (auto* n : {"Visible", "Hidden", "Attachments", "History"}) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); pl_gb->add_child(cb);
    }

    // ---- World (only shown on Enemy/Team, not Self — but here uniform) ----
    auto wld_gb = element_factory::make<c_groupbox>("World");
    wld_gb->set_title_font(&font);
    wld_gb->set_bounds({0, 0, 0, 240});
    cols->add_child(wld_gb);
    for (auto* n : {
            "Fullbright", "Nightmode", "No flash", "No smoke",
            "No scope", "No recoil", "No visual kick", "No visual punch",
        }) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); wld_gb->add_child(cb);
    }
    {
        auto wt = element_factory::make<c_slider<int>>("Wall transparency", 0, 100, nullptr, " %");
        wt->set_font(&font); wt->set_bounds({0,0,0,32}); wt->set(50);
        wld_gb->add_child(wt);
    }

    // ---- Crosshair (binary lines 116-120) ---------------------------------
    auto xh_gb = element_factory::make<c_groupbox>("Crosshair");
    xh_gb->set_title_font(&font);
    xh_gb->set_bounds({0, 0, 0, 120});
    cols->add_child(xh_gb);
    for (auto* n : {"Force crosshair", "Autowall crosshair", "Autowall dot"}) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); xh_gb->add_child(cb);
    }

    // ---- Removals (binary lines 122-138) ----------------------------------
    auto rm_gb = element_factory::make<c_groupbox>("Removals");
    rm_gb->set_title_font(&font);
    rm_gb->set_bounds({0, 0, 0, 240});
    cols->add_child(rm_gb);
    for (auto* n : {
            "Disable post processing", "Disable fog", "Disable cascade shadows",
            "Disable blood splatter", "Disable teammate rendering",
            "No landing bob", "No fakeduck bob",
        }) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); rm_gb->add_child(cb);
    }

    // ---- Custom model properties (binary lines 140-143) -------------------
    auto cmp_gb = element_factory::make<c_groupbox>("Custom model properties");
    cmp_gb->set_title_font(&font);
    cmp_gb->set_bounds({0, 0, 0, 160});
    cols->add_child(cmp_gb);
    for (auto* n : {"Pearlescent", "Phong"}) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); cmp_gb->add_child(cb);
    }
    {
        auto refl = element_factory::make<c_slider<float>>(
            "Reflectivity", 0.0f, 1.0f, nullptr, "");
        refl->set_font(&font); refl->set_bounds({0,0,0,32}); refl->set(0.5f);
        cmp_gb->add_child(refl);
    }

    // ---- View (binary lines 145-160) --------------------------------------
    auto view_gb = element_factory::make<c_groupbox>("View");
    view_gb->set_title_font(&font);
    view_gb->set_bounds({0, 0, 0, 320});
    cols->add_child(view_gb);
    for (auto* n : {
            "Thirdperson while spectating",
            "Disable thirdperson with grenades",
            "Freecam", "Flip knife hand", "Visualize silent angles",
            "Visual interpolation",
        }) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); view_gb->add_child(cb);
    }
    {
        auto fov = element_factory::make<c_slider<int>>("Field of view", 60, 120, nullptr, "");
        fov->set_font(&font); fov->set_bounds({0,0,0,32}); fov->set(90);
        view_gb->add_child(fov);
        auto sfov = element_factory::make<c_slider<int>>("Scoped field of view", 60, 120, nullptr, "");
        sfov->set_font(&font); sfov->set_bounds({0,0,0,32}); sfov->set(90);
        view_gb->add_child(sfov);
        auto td = element_factory::make<c_slider<int>>("Thirdperson distance", 50, 200, nullptr, " units");
        td->set_font(&font); td->set_bounds({0,0,0,32}); td->set(100);
        view_gb->add_child(td);
        auto zf = element_factory::make<c_slider<float>>("Zoom factor", 0.5f, 2.0f, nullptr, "x");
        zf->set_font(&font); zf->set_bounds({0,0,0,32}); zf->set(1.0f);
        view_gb->add_child(zf);
    }

    // ---- Viewmodel (binary lines 161-166) ---------------------------------
    auto vm_gb = element_factory::make<c_groupbox>("Viewmodel");
    vm_gb->set_title_font(&font);
    vm_gb->set_bounds({0, 0, 0, 180});
    cols->add_child(vm_gb);
    {
        auto xo = element_factory::make<c_slider<int>>("X offset", -10, 10, nullptr, "");
        xo->set_font(&font); xo->set_bounds({0,0,0,32}); xo->set(0);
        vm_gb->add_child(xo);
        auto yo = element_factory::make<c_slider<int>>("Y offset", -10, 10, nullptr, "");
        yo->set_font(&font); yo->set_bounds({0,0,0,32}); yo->set(0);
        vm_gb->add_child(yo);
        auto zo = element_factory::make<c_slider<int>>("Z offset", -10, 10, nullptr, "");
        zo->set_font(&font); zo->set_bounds({0,0,0,32}); zo->set(0);
        vm_gb->add_child(zo);
        auto roll = element_factory::make<c_slider<int>>("Roll", -180, 180, nullptr, "");
        roll->set_font(&font); roll->set_bounds({0,0,0,32}); roll->set(0);
        vm_gb->add_child(roll);
    }

    // ---- Helpers (binary lines 168-177) -----------------------------------
    auto hp_gb = element_factory::make<c_groupbox>("Helpers");
    hp_gb->set_title_font(&font);
    hp_gb->set_bounds({0, 0, 0, 220});
    cols->add_child(hp_gb);
    for (auto* n : {
            "Build name", "Frames per second", "Latency to server",
            "System time", "Server map", "Server type", "Server tickrate",
            "Username", "Show watermark", "Show keybind states",
        }) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); hp_gb->add_child(cb);
    }
}

// Visuals page: a horizontal icon-tab strip at the top with Enemy / Team /
// Self, each switching to a different sub-page. Cursor turns to hand when
// hovering the tabs.
void populate_visuals_page(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto strip = std::make_shared<c_icon_tab_strip>(std::vector<c_icon_tab_strip::entry_t>{
        {"Enemy", tab_icon_e::enemy},
        {"Team",  tab_icon_e::team},
        {"Self",  tab_icon_e::self},
    });
    strip->set_font(&font);

    static const char* roles[] = { "Enemy", "Team", "Self" };
    for (auto* role : roles) {
        auto sub = element_factory::make<c_page>("role");
        populate_visuals_role_page(sub, font, role);
        strip->add_child(sub);
    }

    strip->set_active_index(0);
    page->add_child(strip);
}

void populate_style_subtab(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);

    // BINARY: gui_build_menu.c:1374+ — "Element rounding" is an "Enable all"
    // checkbox plus per-widget CHECKBOXES bound to thread_safe_t<bool>
    // globals (g_style_button_rounded_enabled, ...). Radius itself is the
    // shared floor(gui_scale * 2.75) — see GUI/style/rounding.hpp.
    auto er = element_factory::make<c_groupbox>("Element rounding");
    er->set_title_font(&font);
    er->set_bounds({0, 0, 0, 320});
    cols->add_child(er);

    auto& rnd = style::rounding();
    auto widget_states =
        std::make_shared<std::vector<std::shared_ptr<c_checkbox::state_t>>>();

    auto add_rounding_checkbox = [&](const char* name, bool* target) {
        auto st = std::make_shared<c_checkbox::state_t>(*target);
        auto cb = element_factory::make<c_checkbox>(name, st);
        cb->set_font(&font);
        cb->add_on_change([cb, target] {
            *target = cb->get();
        });
        widget_states->push_back(st);
        er->add_child(cb);
    };

    // BINARY: a 2-per-row button grid with "Enable all" / "Disable all"
    // buttons sits at the top of the groupbox (gui_build_menu.c:1376-1388),
    // not a checkbox.
    {
        auto grid = element_factory::make<c_button_grid>(/*per_row=*/2);
        grid->set_bounds({0, 0, 0, 0});
        auto set_all = [widget_states](bool on) {
            style::rounding().set_all(on);
            for (auto& st : *widget_states) st->set(on);
        };
        auto en = element_factory::make<c_button>("Enable all",
                                                  [set_all] { set_all(true); });
        en->set_font(&font); en->set_bounds({0,0,0,24});
        grid->add_child(en);
        auto dis = element_factory::make<c_button>("Disable all",
                                                   [set_all] { set_all(false); });
        dis->set_font(&font); dis->set_bounds({0,0,0,24});
        grid->add_child(dis);
        er->add_child(grid);
    }

    // Checkbox order follows the dumped builder sequence in
    // gui_build_menu.c:1389-1430.
    add_rounding_checkbox("Button rounding", &rnd.button);
    add_rounding_checkbox("Checkbox rounding", &rnd.checkbox);
    add_rounding_checkbox("Colorpicker rounding", &rnd.colorpicker);
    add_rounding_checkbox("Context menu rounding", &rnd.context_menu);
    add_rounding_checkbox("Dialog window rounding", &rnd.dialog_window);
    add_rounding_checkbox("Dropdown rounding", &rnd.dropdown);
    add_rounding_checkbox("Groupbox rounding", &rnd.groupbox);
    add_rounding_checkbox("Keybinder rounding", &rnd.keybinder);
    add_rounding_checkbox("Table rounding", &rnd.table);
    add_rounding_checkbox("Listbox rounding", &rnd.listbox);
    add_rounding_checkbox("Menu window rounding", &rnd.menu_window);
    add_rounding_checkbox("Page manager rounding", &rnd.page_manager);
    add_rounding_checkbox("Text container rounding", &rnd.text_container);
    add_rounding_checkbox("Scrollbar rounding", &rnd.scrollbar);
    add_rounding_checkbox("Slider rounding", &rnd.slider);
    add_rounding_checkbox("Tab manager rounding", &rnd.tab_manager);
    add_rounding_checkbox("Textbox rounding", &rnd.textbox);
    add_rounding_checkbox("Spectator list rounding", &rnd.spectator_list);
    add_rounding_checkbox("Watermark rounding", &rnd.watermark);
    add_rounding_checkbox("Keybind list rounding", &rnd.keybind_list);

    // BINARY (gui_build_menu.c:1429-1462): "Element shading" groupbox —
    // Enable/Disable-all button grid, "Shading color" colorpicker (no
    // alpha), "Shading factor" slider 0..0.2 step 0.005, then SEVEN
    // shading CHECKBOXES (Colorpicker first — its creation is the
    // encrypted call right after the slider).
    auto sh = element_factory::make<c_groupbox>("Element shading");
    sh->set_title_font(&font);
    sh->set_bounds({0, 0, 0, 320});
    cols->add_child(sh);

    auto shading_states =
        std::make_shared<std::vector<std::shared_ptr<c_checkbox::state_t>>>();
    {
        auto grid = element_factory::make<c_button_grid>(/*per_row=*/2);
        grid->set_bounds({0, 0, 0, 0});
        auto set_all = [shading_states](bool on) {
            for (auto& st : *shading_states) st->set(on);
        };
        auto en = element_factory::make<c_button>("Enable all",
                                                  [set_all] { set_all(true); });
        en->set_font(&font); en->set_bounds({0,0,0,24});
        grid->add_child(en);
        auto dis = element_factory::make<c_button>("Disable all",
                                                   [set_all] { set_all(false); });
        dis->set_font(&font); dis->set_bounds({0,0,0,24});
        grid->add_child(dis);
        sh->add_child(grid);
    }
    {
        auto cp = element_factory::make<c_colorpicker>(
            "Shading color", Renderer::color{0x00, 0x00, 0x00, 0xFF},
            /*has_alpha=*/false);
        cp->set_font(&font);
        sh->add_child(cp);
    }
    {
        auto s = element_factory::make<c_slider<float>>(
            "Shading factor", 0.0f, 0.2f, nullptr, "");
        s->set_font(&font);
        s->set_bounds({0, 0, 0, 32});
        sh->add_child(s);
    }
    static const char* shading[] = {
        "Colorpicker shading", "Dropdown shading", "Keybinder shading",
        "Checkbox shading", "Textbox shading", "Button shading",
        "Slider shading",
    };
    for (auto* n : shading) {
        auto st = std::make_shared<c_checkbox::state_t>(false);
        auto cb = element_factory::make<c_checkbox>(n, st);
        cb->set_font(&font);
        shading_states->push_back(st);
        sh->add_child(cb);
    }
}

struct subtab_spec { const char* name; void (*populate)(std::shared_ptr<c_page>&, const Renderer::font&); };

std::shared_ptr<c_tab> build_demo_tab_with_subtabs(
    const char* tab_label, const Renderer::font& font,
    std::initializer_list<subtab_spec> subtabs)
{
    auto tab = element_factory::make<c_tab>(tab_label);
    tab->set_subtab_font(&font);
    std::shared_ptr<c_subtab> first_st;
    for (auto& s : subtabs) {
        auto st   = element_factory::make<c_subtab>(s.name);
        auto pm   = element_factory::make<c_page_manager>();
        auto page = element_factory::make<c_page>("page");
        s.populate(page, font);
        pm->add_child(page);
        pm->set_active_page(page);
        st->add_child(pm);
        tab->add_child(st);
        if (!first_st) first_st = st;
    }
    tab->set_active_subtab(first_st);
    return tab;
}

// ============================================================================
// MISCELLANEOUS — split into View / Helpers / Movement / Skins subtabs
// ============================================================================
// Each populate_misc_*_into_column appends its groupboxes to an existing
// column so the unified Misc page can stack them all in one lane pair
// matching the binary tree (Spectators → Server region → Clantag →
// Bypass → Auto-actions → Buybot → Sounds → Movement → Skins → Paintkit).
void populate_misc_movement_into_column(std::shared_ptr<c_column>& cols,
                                        const Renderer::font& font)
{
    auto mv_gb = element_factory::make<c_groupbox>("Movement");
    mv_gb->set_title_font(&font);
    mv_gb->set_bounds({0, 0, 0, 360});
    cols->add_child(mv_gb);

    // Toggles
    for (auto* n : {
            "Fast crouch", "Fast stop", "Crouch in air",
            "Accurate walk", "Leg movement",
            "Strafe assistance", "Auto strafe",
        }) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font);
        mv_gb->add_child(cb);
    }
    // Keybinds (binary labels at .rdata 0x1401FB440 "Auto bunnyhop",
    // 0x1401FB420 "Edge jump", 0x1401FB430 "Auto peek",
    // 0x1401FAC20 "Fakeduck", 0x1401FAC30 "Slow walk").
    struct {
        const char*     label;
        const char*     tag;
        Renderer::color tint;
    } mv_kbs[] = {
        {"Auto bunnyhop", "BHOP", Renderer::color{0x4A, 0xC0, 0x6A, 0xFF}},
        {"Edge jump",     "EJ",   Renderer::color{0x6A, 0xB0, 0xE0, 0xFF}},
        {"Auto peek",     "PEEK", Renderer::color{0xFA, 0xA6, 0x18, 0xFF}},
        {"Fakeduck",      "FD",   Renderer::color{0xD0, 0x7A, 0xE0, 0xFF}},
        {"Slow walk",     "SW",   Renderer::color{0xD2, 0xD2, 0xD2, 0xFF}},
    };
    for (const auto& e : mv_kbs) {
        auto kb = element_factory::make<c_keybinder>(e.label);
        kb->set_font(&font);
        mv_gb->add_child(kb);
        register_indicator_kb(kb, e.tag, e.tint);
    }
}

void populate_misc_movement(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);
    populate_misc_movement_into_column(cols, font);
}

// Legacy: populate_misc_page kept temporarily for compatibility (unused).
void populate_misc_page(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    populate_misc_movement(page, font);
}

void populate_misc_helpers_into_column(std::shared_ptr<c_column>& cols,
                                       const Renderer::font& font)
{
    // ---- Sounds ------------------------------------------------------------
    auto snd_gb = element_factory::make<c_groupbox>("Sounds");
    snd_gb->set_title_font(&font);
    snd_gb->set_bounds({0, 0, 0, 180});
    cols->add_child(snd_gb);
    for (auto* n : {"On kill mic spam", "Hit sound", "Kill sound"}) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); snd_gb->add_child(cb);
    }
    {
        auto wv = element_factory::make<c_slider<int>>("Weapon volume", 0, 100, nullptr, " %");
        wv->set_font(&font); wv->set_bounds({0,0,0,32}); wv->set(100);
        snd_gb->add_child(wv);
        auto fv = element_factory::make<c_slider<int>>("Footstep volume", 0, 100, nullptr, " %");
        fv->set_font(&font); fv->set_bounds({0,0,0,32}); fv->set(100);
        snd_gb->add_child(fv);
        auto ov = element_factory::make<c_slider<int>>("Other sound volume", 0, 100, nullptr, " %");
        ov->set_font(&font); ov->set_bounds({0,0,0,32}); ov->set(100);
        snd_gb->add_child(ov);
    }

    // ---- Buybot ------------------------------------------------------------
    auto bb_gb = element_factory::make<c_groupbox>("Buybot");
    bb_gb->set_title_font(&font);
    bb_gb->set_bounds({0, 0, 0, 200});
    cols->add_child(bb_gb);
    {
        auto en = element_factory::make<c_checkbox>("Enable");
        en->set_font(&font); bb_gb->add_child(en);
        auto pri = element_factory::make<c_dropdown>("Primary",
            std::vector<std::string>{"None", "AK-47", "M4A4", "M4A1-S", "AWP"});
        pri->set_font(&font); pri->set_bounds({0,0,0,36}); bb_gb->add_child(pri);
        auto sec = element_factory::make<c_dropdown>("Secondary",
            std::vector<std::string>{"None", "USP-S", "P2000", "Glock", "Deagle"});
        sec->set_font(&font); sec->set_bounds({0,0,0,36}); bb_gb->add_child(sec);
        auto util = element_factory::make<c_dropdown>("Utility",
            std::vector<std::string>{"None", "HE", "Flashbang", "Smoke", "Molotov"});
        util->set_font(&font); util->set_bounds({0,0,0,36}); bb_gb->add_child(util);
    }

    // ---- Auto-actions ------------------------------------------------------
    auto aa_gb = element_factory::make<c_groupbox>("Auto-actions");
    aa_gb->set_title_font(&font);
    aa_gb->set_bounds({0, 0, 0, 220});
    cols->add_child(aa_gb);
    // Auto-actions per binary 0x1400800f7..0x1400801d3 + 0x1400802e6.
    for (auto* n : {
            "Auto bomb defuse", "Aim at bomb while defusing",
            "Auto match accept", "Preserve killfeed", "Auto fire",
        }) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); aa_gb->add_child(cb);
    }
    {
        auto afd = element_factory::make<c_slider<int>>("Auto fire delay", 0, 500, nullptr, " ms");
        afd->set_font(&font); afd->set_bounds({0,0,0,32}); afd->set(50);
        aa_gb->add_child(afd);
    }
}

void populate_misc_helpers(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);
    populate_misc_helpers_into_column(cols, font);
}

void populate_misc_view_into_column(std::shared_ptr<c_column>& cols,
                                    const Renderer::font& font)
{
    // ---- Spectators (binary 0x140080089..0x1400800c0) ---------------------
    // Binary: Show spectators + Extended backtracking.
    auto sp_gb = element_factory::make<c_groupbox>("Spectators");
    sp_gb->set_title_font(&font);
    sp_gb->set_bounds({0, 0, 0, 100});
    cols->add_child(sp_gb);
    for (auto* n : {"Show spectators", "Extended backtracking"}) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); sp_gb->add_child(cb);
    }

    // ---- Server region -----------------------------------------------------
    auto sv_gb = element_factory::make<c_groupbox>("Server region");
    sv_gb->set_title_font(&font);
    sv_gb->set_bounds({0, 0, 0, 80});
    cols->add_child(sv_gb);
    {
        auto dd = element_factory::make<c_dropdown>("Server region",
            std::vector<std::string>{"Custom", "Onetap", "Backwards"});
        dd->set_font(&font); dd->set_bounds({0,0,0,36}); sv_gb->add_child(dd);
    }

    // ---- Clantag -----------------------------------------------------------
    auto ct_gb = element_factory::make<c_groupbox>("Clantag changer");
    ct_gb->set_title_font(&font);
    ct_gb->set_bounds({0, 0, 0, 100});
    cols->add_child(ct_gb);
    {
        auto en = element_factory::make<c_checkbox>("Clantag changer");
        en->set_font(&font); ct_gb->add_child(en);
        auto tb = element_factory::make<c_textbox_new>("Custom clantag ", "");
        tb->set_font(&font); ct_gb->add_child(tb);
    }

    // ---- Bypass ------------------------------------------------------------
    auto bp_gb = element_factory::make<c_groupbox>("Bypass");
    bp_gb->set_title_font(&font);
    bp_gb->set_bounds({0, 0, 0, 160});
    cols->add_child(bp_gb);
    for (auto* n : {
            "Reveal hidden cvars", "Reveal matchmaking ranks",
            "Bypass inventory lock", "Bypass sv_pure", "Force sv_cheats",
            "Extended backtracking",
        }) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); bp_gb->add_child(cb);
    }
}

void populate_misc_view(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);
    populate_misc_view_into_column(cols, font);
}

void populate_misc_skins_into_column(std::shared_ptr<c_column>& cols,
                                     const Renderer::font& font)
{
    auto sk_gb = element_factory::make<c_groupbox>("Skins");
    sk_gb->set_title_font(&font);
    sk_gb->set_bounds({0, 0, 0, 280});
    cols->add_child(sk_gb);
    for (auto* n : {
            "Models", "Force rare animations", "Knife model",
            "Glove model", "Player model", "StatTrak",
            "Show for current weapon only",
        }) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); sk_gb->add_child(cb);
    }
    {
        auto wear = element_factory::make<c_slider<float>>("Wear", 0.0f, 1.0f, nullptr, "");
        wear->set_font(&font); wear->set_bounds({0,0,0,32}); wear->set(0.0f);
        sk_gb->add_child(wear);
        auto seed = element_factory::make<c_slider<int>>("Seed", 0, 1000, nullptr, "");
        seed->set_font(&font); seed->set_bounds({0,0,0,32}); seed->set(0);
        sk_gb->add_child(seed);
    }

    auto pk_gb = element_factory::make<c_groupbox>("Paintkit");
    pk_gb->set_title_font(&font);
    pk_gb->set_bounds({0, 0, 0, 200});
    cols->add_child(pk_gb);
    {
        auto lb = element_factory::make<c_listbox>(
            "Paintkit list",
            std::vector<std::string>{
                "Vanilla", "Asiimov", "Fade", "Dragon Lore", "Howl", "Crimson Web",
            }, 5, true);
        lb->set_font(&font); pk_gb->add_child(lb);
    }
}

void populate_misc_skins(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);
    populate_misc_skins_into_column(cols, font);
}

// Unified Misc page — matches `binary_tab_tree.md` Miscellaneous side
// tab: a SINGLE flat page (no subtabs) with all groupboxes in binary
// order. Groupboxes flow into one shared c_column(2) so they tile in
// two lanes correctly.
void populate_misc_main(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);
    populate_misc_view_into_column(cols, font);      // Spectators / Server region / Clantag / Bypass
    populate_misc_helpers_into_column(cols, font);   // Sounds / Buybot / Auto-actions
    populate_misc_movement_into_column(cols, font);  // Movement
    populate_misc_skins_into_column(cols, font);     // Skins / Paintkit
}

// ============================================================================
// SETTINGS — split into Cheat / GUI / Cloud / Debug subtabs
// ============================================================================
void populate_settings_cheat(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);

    auto cheat_gb = element_factory::make<c_groupbox>("Cheat");
    cheat_gb->set_title_font(&font);
    cheat_gb->set_bounds({0, 0, 0, 160});
    cols->add_child(cheat_gb);
    {
        auto unload = element_factory::make<c_button>("Unload", [] {
            g_request_unload = true;
        });
        unload->set_font(&font);
        unload->set_bounds({0,0,0,24});
        cheat_gb->add_child(unload);
        auto au = element_factory::make<c_checkbox>("Anti-untrusted");
        au->set_font(&font); cheat_gb->add_child(au);
        auto sm = element_factory::make<c_checkbox>("Anti-SMAC");
        sm->set_font(&font); cheat_gb->add_child(sm);
    }

    auto rest_gb = element_factory::make<c_groupbox>("Restrictions");
    rest_gb->set_title_font(&font);
    rest_gb->set_bounds({0, 0, 0, 100});
    cols->add_child(rest_gb);
    for (auto* n : {"Enable restrictions"}) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); rest_gb->add_child(cb);
    }
}

void populate_settings_gui(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);

    auto gui_gb = element_factory::make<c_groupbox>("GUI");
    gui_gb->set_title_font(&font);
    gui_gb->set_bounds({0, 0, 0, 280});
    cols->add_child(gui_gb);
    {
        // Open GUI keybinder — toggles g_menu_visible (polled in WinMain).
        auto open_kb = std::make_shared<c_keybinder>("Open GUI", g_menu_kb_state);
        open_kb->set_font(&font);
        gui_gb->add_child(open_kb);

        // Accent color picker — writes live to style::current().accent.
        auto cp = element_factory::make<c_colorpicker>(
            "GUI accent color", style::current().accent);
        cp->set_font(&font);
        cp->add_on_change([cp] { style::current().accent = cp->color(); });
        gui_gb->add_child(cp);

        auto scl = element_factory::make<c_dropdown>(
            "GUI scaling",
            std::vector<std::string>{"100%", "125%", "150%", "175%", "200%"});
        scl->set_font(&font);
        scl->set(0);  // default 100%
        scl->add_on_change([scl] {
            // Index → multiplier: 0=1.0, 1=1.25, 2=1.5, 3=1.75, 4=2.0
            static const float k_table[5] = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
            const int idx = scl->get();
            if (idx >= 0 && idx < 5) {
                GUI::style::set_gui_scale(k_table[idx]);
            }
        });
        gui_gb->add_child(scl);

        // Sensible defaults — GUI animations + Allow resize + Smooth
        // scrolling on by default; others off.
        struct gui_cb_spec { const char* name; bool on; };
        const gui_cb_spec gui_cbs[] = {
            {"GUI animations",                true},
            {"Allow resize",                  true},
            {"Ignore strict keybind warning", false},
            {"Smooth scrolling",              true},
        };
        for (const auto& s : gui_cbs) {
            auto st = std::make_shared<c_checkbox::state_t>(s.on);
            auto cb = std::make_shared<c_checkbox>(s.name, st);
            cb->set_font(&font);
            gui_gb->add_child(cb);
        }

        auto center = element_factory::make<c_button>("Center GUI", [] {});
        center->set_font(&font);
        gui_gb->add_child(center);
    }

    // ---- Smooth scrolling sub-group (binary lines 263-274) -----------------
    auto smooth_gb = element_factory::make<c_groupbox>("Smooth scrolling");
    smooth_gb->set_title_font(&font);
    smooth_gb->set_bounds({0, 0, 0, 240});
    cols->add_child(smooth_gb);
    {
        auto at = element_factory::make<c_slider<float>>(
            "Accelerate threshold", 0.0f, 1.0f, nullptr, "");
        at->set_font(&font); at->set_bounds({0,0,0,32}); at->set(0.5f);
        smooth_gb->add_child(at);

        auto acc = element_factory::make<c_dropdown>(
            "Accelerator", std::vector<std::string>{"Slow", "Normal", "Fastest"});
        acc->set_font(&font); acc->set_bounds({0,0,0,36}); acc->set(1);
        smooth_gb->add_child(acc);

        auto dr = element_factory::make<c_slider<float>>(
            "Distance ratio", 0.0f, 2.0f, nullptr, "");
        dr->set_font(&font); dr->set_bounds({0,0,0,32}); dr->set(1.0f);
        smooth_gb->add_child(dr);

        auto sm = element_factory::make<c_slider<float>>(
            "Smoothing", 0.0f, 1.0f, nullptr, "");
        sm->set_font(&font); sm->set_bounds({0,0,0,32}); sm->set(0.15f);
        smooth_gb->add_child(sm);

        auto rd = element_factory::make<c_slider<float>>(
            "Rounding", 0.0f, 16.0f, nullptr, "");
        rd->set_font(&font); rd->set_bounds({0,0,0,32}); rd->set(4.0f);
        smooth_gb->add_child(rd);

        auto fc = element_factory::make<c_slider<int>>(
            "Factor", 0, 100, nullptr, " px");
        fc->set_font(&font); fc->set_bounds({0,0,0,32}); fc->set(10);
        smooth_gb->add_child(fc);
    }

}

// User 2026-06-01: configs live in ./ot/*.cfg and scripts in
// ./ot/scripts/*.lua. The listboxes populate from those directories
// on page build, and the Refresh button (configs) / Reload all
// (scripts) re-scan on click.
static std::vector<std::string> scan_directory(const std::filesystem::path& dir,
                                               const std::string& ext)
{
    std::vector<std::string> out;
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        std::filesystem::create_directories(dir, ec);
    }
    for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ext) continue;
        out.push_back(entry.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

void populate_settings_cloud(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);

    auto cloud_gb = element_factory::make<c_groupbox>("Cloud");
    cloud_gb->set_title_font(&font);
    cloud_gb->set_bounds({0, 0, 0, 360});
    cols->add_child(cloud_gb);

    const std::filesystem::path cfg_dir = "ot";
    auto cfg_list = std::make_shared<c_listbox>(
        std::string{"Configurations"},
        scan_directory(cfg_dir, ".cfg"),
        /*show_n_items*/ 5,
        /*required*/    false);
    cfg_list->set_font(&font);
    cloud_gb->add_child(cfg_list);

    auto refresh_cfg = [cfg_list, cfg_dir] {
        cfg_list->set_options(scan_directory(cfg_dir, ".cfg"));
    };
    auto on_create = [cfg_list, cfg_dir, refresh_cfg] {
        // Pick a non-clashing default name "configN.cfg".
        int n = 1;
        std::filesystem::path p;
        do {
            p = cfg_dir / ("config" + std::to_string(n++) + ".cfg");
        } while (std::filesystem::exists(p) && n < 1000);
        std::error_code ec;
        std::filesystem::create_directories(cfg_dir, ec);
        std::ofstream(p) << "// new config\n";
        refresh_cfg();
    };
    auto on_delete = [cfg_list, cfg_dir, refresh_cfg] {
        const int sel = cfg_list->get();
        if (sel < 0 || sel >= static_cast<int>(cfg_list->options().size())) return;
        std::error_code ec;
        std::filesystem::remove(cfg_dir / cfg_list->options()[sel], ec);
        refresh_cfg();
    };

    struct btn_spec { const char* name; std::function<void()> cb; };
    const btn_spec cfg_btns[] = {
        {"Create",  on_create},
        {"Save",    [] {} },
        {"Load",    [] {} },
        {"Copy",    [] {} },
        {"Paste",   [] {} },
        {"Refresh", refresh_cfg},
        {"Delete",  on_delete},
    };
    for (const auto& b : cfg_btns) {
        auto btn = std::make_shared<c_button>(b.name, b.cb);
        btn->set_font(&font); btn->set_bounds({0,0,0,24});
        cloud_gb->add_child(btn);
    }
}

void populate_settings_debug(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);

    // ---- Console -----------------------------------------------------------
    auto cons_gb = element_factory::make<c_groupbox>("Console");
    cons_gb->set_title_font(&font);
    cons_gb->set_bounds({0, 0, 0, 180});
    cols->add_child(cons_gb);
    {
        auto dd = element_factory::make<c_dropdown>(
            "Log output", std::vector<std::string>{"Console", "Screen"});
        dd->set_font(&font); dd->set_bounds({0,0,0,36});
        cons_gb->add_child(dd);
        for (auto* n : {"Log events", "Kill", "Fire"}) {
            auto cb = element_factory::make<c_checkbox>(n);
            cb->set_font(&font); cons_gb->add_child(cb);
        }
        auto cc = element_factory::make<c_colorpicker>(
            "Log color", Renderer::color{0xFF, 0xFF, 0xFF, 0xFF});
        cc->set_font(&font); cons_gb->add_child(cc);
    }

    // ---- Scripts (./ot/scripts/*.lua) -------------------------------------
    auto scr_gb = element_factory::make<c_groupbox>("Scripts");
    scr_gb->set_title_font(&font);
    scr_gb->set_bounds({0, 0, 0, 280});
    cols->add_child(scr_gb);

    const std::filesystem::path scr_dir = std::filesystem::path("ot") / "scripts";
    auto scr_list = std::make_shared<c_listbox>(
        std::string{"Loaded scripts"},
        scan_directory(scr_dir, ".lua"),
        /*show_n_items*/ 4,
        /*required*/    false);
    scr_list->set_font(&font);
    scr_gb->add_child(scr_list);

    auto refresh_scr = [scr_list, scr_dir] {
        scr_list->set_options(scan_directory(scr_dir, ".lua"));
    };

    struct scr_btn { const char* name; std::function<void()> cb; };
    const scr_btn scr_btns[] = {
        {"Reload",     refresh_scr},
        {"Reload all", refresh_scr},
        {"Debug",      [] {} },
        {"Save",       [] {} },
        {"Load",       [] {} },
    };
    for (const auto& b : scr_btns) {
        auto btn = std::make_shared<c_button>(b.name, b.cb);
        btn->set_font(&font); btn->set_bounds({0,0,0,24});
        scr_gb->add_child(btn);
    }
}

// Combined kept for backwards-compat callers (none after restructure).
void populate_settings_page(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    populate_settings_cheat(page, font);
}

// ============================================================================
// LEGIT  — General groupbox content (binary: lines 79-92 menu_layout_order)
// ============================================================================
void populate_legit_general(std::shared_ptr<c_groupbox>& gb,
                            const Renderer::font& font)
{
    // Master switch defaults ON so the rest of legit is reachable.
    auto ms_state = std::make_shared<c_checkbox::state_t>(true);
    auto ms = std::make_shared<c_checkbox>("Master switch", ms_state);
    ms->set_font(&font); gb->add_child(ms);

    auto sc = element_factory::make<c_checkbox>("Smoke check");
    sc->set_font(&font); gb->add_child(sc);
    auto isf = element_factory::make<c_checkbox>("Ignore Steam friends");
    isf->set_font(&font); gb->add_child(isf);

    // Default reaction time = 100 ms (mid-range human response).
    auto rt = element_factory::make<c_slider<int>>("Reaction time", 0, 500, nullptr, " ms");
    rt->set_font(&font); rt->set_bounds({0,0,0,32}); rt->set(100);
    gb->add_child(rt);
}

// ============================================================================
// RAGE — extras beyond the per-condition antiaim groupboxes.
// Mirrors menu_layout_order.txt entries 35-75: Aimbot, Exploits, Force*,
// override sliders, Triggers.
// ============================================================================
void populate_rage_extras(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cols = add_columns(page);

    // ---- Aimbot (binary lines 47-56) --------------------------------------
    auto aim_gb = element_factory::make<c_groupbox>("Aimbot");
    aim_gb->set_title_font(&font);
    aim_gb->set_bounds({0, 0, 0, 220});
    cols->add_child(aim_gb);
    for (auto* n : {
            "Silent aim", "Team check", "Friendly fire", "Dormant aimbot",
        }) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); aim_gb->add_child(cb);
    }
    // Keybinds (binary labels at .rdata 0x1401FAB40 "Hide shots",
    // 0x1401FAB78 "Double tap", 0x1401FAB60 "Ragebot activation").
    struct {
        const char* label; const char* tag; Renderer::color tint;
    } aim_kbs[] = {
        {"Hide shots",        "HS",  Renderer::color{0x4A, 0xC0, 0x6A, 0xFF}},
        {"Double tap",        "DT",  Renderer::color{0xFA, 0xA6, 0x18, 0xFF}},
        {"Ragebot activation","RAGE",Renderer::color{0xE0, 0x4A, 0x4A, 0xFF}},
    };
    for (const auto& e : aim_kbs) {
        auto kb = element_factory::make<c_keybinder>(e.label);
        kb->set_font(&font); aim_gb->add_child(kb);
        register_indicator_kb(kb, e.tag, e.tint);
    }

    // ---- Exploits (binary lines 58-62) ------------------------------------
    auto ex_gb = element_factory::make<c_groupbox>("Exploits");
    ex_gb->set_title_font(&font);
    ex_gb->set_bounds({0, 0, 0, 140});
    cols->add_child(ex_gb);
    {
        auto exm = element_factory::make<c_dropdown>(
            "Exploits mode",
            std::vector<std::string>{"Reliable", "Fast", "Unreliable"});
        exm->set_font(&font); exm->set_bounds({0,0,0,36}); exm->set(0);
        ex_gb->add_child(exm);
        auto es = element_factory::make<c_slider<int>>("Exploit speed", 0, 100, nullptr, " %");
        es->set_font(&font); es->set_bounds({0,0,0,32}); es->set(80);
        ex_gb->add_child(es);
    }

    // ---- Force overrides (binary lines 64-68) -----------------------------
    auto fo_gb = element_factory::make<c_groupbox>("Force overrides");
    fo_gb->set_title_font(&font);
    fo_gb->set_bounds({0, 0, 0, 180});
    cols->add_child(fo_gb);
    // Resolver override stays a checkbox; the three "Force *" are
    // keybinds in the binary (labels at .rdata 0x1401FAB88 "Force on
    // shot", 0x1401FABC8 "Force body aim", 0x1401FABB0 "Force safe
    // point").
    {
        auto cb = element_factory::make<c_checkbox>("Resolver override");
        cb->set_font(&font); fo_gb->add_child(cb);
    }
    struct {
        const char* label; const char* tag; Renderer::color tint;
    } fo_kbs[] = {
        {"Force on shot",   "OS",   Renderer::color{0xFA, 0x70, 0x18, 0xFF}},
        {"Force body aim",  "BAIM", Renderer::color{0xE0, 0x4A, 0x4A, 0xFF}},
        {"Force safe point","SAFE", Renderer::color{0x6A, 0xB0, 0xE0, 0xFF}},
    };
    for (const auto& e : fo_kbs) {
        auto kb = element_factory::make<c_keybinder>(e.label);
        kb->set_font(&font); fo_gb->add_child(kb);
        register_indicator_kb(kb, e.tag, e.tint);
    }

    // ---- Override sliders (binary lines 69-72) ----------------------------
    auto ov_gb = element_factory::make<c_groupbox>("Overrides");
    ov_gb->set_title_font(&font);
    ov_gb->set_bounds({0, 0, 0, 200});
    cols->add_child(ov_gb);
    {
        auto dmg = element_factory::make<c_slider<int>>("Damage override", 0, 130, nullptr, " hp");
        dmg->set_font(&font); dmg->set_bounds({0,0,0,32}); dmg->set(100);
        ov_gb->add_child(dmg);
        auto hb = element_factory::make<c_slider<int>>("Hitbox override", 0, 7, nullptr, "");
        hb->set_font(&font); hb->set_bounds({0,0,0,32}); hb->set(0);
        ov_gb->add_child(hb);
        auto aw = element_factory::make<c_slider<int>>("Autowall override", 0, 100, nullptr, " %");
        aw->set_font(&font); aw->set_bounds({0,0,0,32}); aw->set(20);
        ov_gb->add_child(aw);
    }

    // ---- Triggers (binary lines 30-34) ------------------------------------
    auto tr_gb = element_factory::make<c_groupbox>("Triggers");
    tr_gb->set_title_font(&font);
    tr_gb->set_bounds({0, 0, 0, 100});
    cols->add_child(tr_gb);
    {
        auto cl = element_factory::make<c_slider<int>>("Choke limit", 1, 15, nullptr, " ticks");
        cl->set_font(&font); cl->set_bounds({0,0,0,32}); cl->set(14);
        tr_gb->add_child(cl);
        auto cv = element_factory::make<c_slider<int>>("Choke variance", 0, 14, nullptr, " ticks");
        cv->set_font(&font); cv->set_bounds({0,0,0,32}); cv->set(0);
        tr_gb->add_child(cv);
    }
}

// ============================================================================
// Subtab populators per user spec (Legit/Rage Main+Weapons)
// ============================================================================
void populate_legit_main(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    // Per [[authoritative-binary-menu]] (verified IDA MCP decompile of
    // sub_14007BD60 this session): Legit Main has THREE groupboxes —
    // General, Fakelag, Antiaim.
    auto cols = add_columns(page);

    auto gen_gb = element_factory::make<c_groupbox>("General");
    gen_gb->set_title_font(&font);
    gen_gb->set_bounds({0, 0, 0, 180});
    cols->add_child(gen_gb);
    populate_legit_general(gen_gb, font);

    // ---- Fakelag (binary 0x14007c5ef..0x14007c84b) ---------------------
    auto fl_gb = element_factory::make<c_groupbox>("Fakelag");
    fl_gb->set_title_font(&font);
    fl_gb->set_bounds({0, 0, 0, 320});
    cols->add_child(fl_gb);
    {
        auto en = element_factory::make<c_checkbox>("Enabled");
        en->set_font(&font); fl_gb->add_child(en);
        // "Triggers" sub-section — binary lays these out as a contiguous
        // group of checkboxes under the Enabled toggle; no nested
        // groupbox in the binary, so a label row delineates them.
        auto trig_label = element_factory::make<c_label>("Triggers");
        trig_label->set_font(&font); fl_gb->add_child(trig_label);
        for (auto* n : {
                "In air", "On peek", "On shot", "On land",
                "While reloading", "On weapon switch",
                "On velocity change", "Break lag compensation",
            }) {
            auto cb = element_factory::make<c_checkbox>(n);
            cb->set_font(&font); fl_gb->add_child(cb);
        }
        auto cl = element_factory::make<c_slider<int>>("Choke limit", 1, 15, nullptr, " ticks");
        cl->set_font(&font); cl->set_bounds({0,0,0,32}); cl->set(14);
        fl_gb->add_child(cl);
        auto cv = element_factory::make<c_slider<int>>("Choke variance", 0, 14, nullptr, " ticks");
        cv->set_font(&font); cv->set_bounds({0,0,0,32}); cv->set(0);
        fl_gb->add_child(cv);
    }

    // ---- Antiaim (binary 0x14007c968..0x14007cc41) ---------------------
    auto aa_gb = element_factory::make<c_groupbox>("Antiaim");
    aa_gb->set_title_font(&font);
    aa_gb->set_bounds({0, 0, 0, 180});
    cols->add_child(aa_gb);
    {
        auto da = element_factory::make<c_checkbox>("Direction arrow");
        da->set_font(&font); aa_gb->add_child(da);
        auto inv = element_factory::make<c_checkbox>("Extended angle inverter");
        inv->set_font(&font); aa_gb->add_child(inv);
        auto keys_label = element_factory::make<c_label>("Keys");
        keys_label->set_font(&font); aa_gb->add_child(keys_label);
        for (auto* n : {"Aimbot", "Triggerbot"}) {
            auto kb = element_factory::make<c_keybinder>(n);
            kb->set_font(&font); aa_gb->add_child(kb);
        }
    }
}

void populate_legit_weapons(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    // User 2026-06-01: Legit > Weapons mirrors the Rage > Weapons
    // layout — a horizontal icon strip per weapon class, with each
    // weapon's controls living on its own sub-page.
    auto strip = std::make_shared<c_icon_tab_strip>(std::vector<c_icon_tab_strip::entry_t>{
        {"Pistol",  tab_icon_e::pistol},
        {"Heavy",   tab_icon_e::heavy},
        {"Shotgun", tab_icon_e::shotgun},
        {"SMG",     tab_icon_e::smg},
        {"Rifle",   tab_icon_e::rifle},
        {"Scout",   tab_icon_e::scout},
        {"AWP",     tab_icon_e::awp},
        {"Auto",    tab_icon_e::autosniper},
    });
    strip->set_font(&font);
    if (g_weapon_icon_font) strip->set_icon_font(g_weapon_icon_font);

    static const char* weapons[] = {
        "Pistol", "Heavy", "Shotgun", "SMG", "Rifle", "Scout", "AWP", "Auto",
    };
    for (auto* w : weapons) {
        auto sub = element_factory::make<c_page>(std::string{w});
        auto cols = add_columns(sub);

        // Aimbot groupbox
        auto aim_gb = element_factory::make<c_groupbox>("Aimbot");
        aim_gb->set_title_font(&font);
        aim_gb->set_bounds({0, 0, 0, 260});
        cols->add_child(aim_gb);
        for (auto* n : {
                "Enabled", "Aim assistance", "Silent aim", "Auto scope",
                "Auto fire", "Auto stop",
            }) {
            auto cb = element_factory::make<c_checkbox>(n);
            cb->set_font(&font); aim_gb->add_child(cb);
        }
        {
            auto hc = element_factory::make<c_slider<int>>("Hitchance", 0, 100, nullptr, " %");
            hc->set_font(&font); hc->set_bounds({0,0,0,32}); hc->set(72);
            aim_gb->add_child(hc);
            auto fov = element_factory::make<c_slider<float>>("FOV", 0.0f, 30.0f, nullptr, "");
            fov->set_font(&font); fov->set_bounds({0,0,0,32}); fov->set(5.0f);
            aim_gb->add_child(fov);
        }

        // Trigger groupbox
        auto trg_gb = element_factory::make<c_groupbox>("Trigger");
        trg_gb->set_title_font(&font);
        trg_gb->set_bounds({0, 0, 0, 180});
        cols->add_child(trg_gb);
        for (auto* n : {"Trigger bot", "Hitchance check", "Smoke check"}) {
            auto cb = element_factory::make<c_checkbox>(n);
            cb->set_font(&font); trg_gb->add_child(cb);
        }
        {
            auto dly = element_factory::make<c_slider<int>>("Trigger delay", 0, 500, nullptr, " ms");
            dly->set_font(&font); dly->set_bounds({0,0,0,32}); dly->set(50);
            trg_gb->add_child(dly);
            auto bt = element_factory::make<c_slider<int>>("Burst time", 0, 1000, nullptr, " ms");
            bt->set_font(&font); bt->set_bounds({0,0,0,32}); bt->set(150);
            trg_gb->add_child(bt);
        }

        strip->add_child(sub);
    }

    page->add_child(strip);
}

void populate_rage_main(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto cond = element_factory::make<c_dropdown>("Move condition",
        std::vector<std::string>{
            "Global", "Stand", "Move", "Duck",
            "Air", "Slow walk", "Fakeduck",
        });
    cond->set_font(&font);
    cond->set_bounds({0, 0, 0, 36});
    cond->set(0);
    page->add_child(cond);

    const std::pair<const char*, antiaim_condition_e> conds[] = {
        {"Antiaim (Global)",    antiaim_condition_e::global},
        {"Antiaim (Stand)",     antiaim_condition_e::stand},
        {"Antiaim (Move)",      antiaim_condition_e::move},
        {"Antiaim (Duck)",      antiaim_condition_e::duck},
        {"Antiaim (Air)",       antiaim_condition_e::air},
        {"Antiaim (Slow walk)", antiaim_condition_e::slow_walk},
        {"Antiaim (Fakeduck)",  antiaim_condition_e::fakeduck},
    };
    for (auto& [label, ce] : conds) {
        auto box = element_factory::make<antiaim_groupbox_t>(
            label, page, cond, ce, &font);
        page->add_child(box);
    }
    populate_rage_extras(page, font);
}

void populate_rage_weapons(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    auto strip = std::make_shared<c_icon_tab_strip>(std::vector<c_icon_tab_strip::entry_t>{
        {"Pistol",  tab_icon_e::pistol},
        {"Heavy",   tab_icon_e::heavy},
        {"Shotgun", tab_icon_e::shotgun},
        {"SMG",     tab_icon_e::smg},
        {"Rifle",   tab_icon_e::rifle},
        {"Scout",   tab_icon_e::scout},
        {"AWP",     tab_icon_e::awp},
        {"Auto",    tab_icon_e::autosniper},
    });
    strip->set_font(&font);
    if (g_weapon_icon_font) strip->set_icon_font(g_weapon_icon_font);

    static const char* weapons[] = {
        "Pistol", "Heavy", "Shotgun", "SMG", "Rifle", "Scout", "AWP", "Auto",
    };
    int weapon_id = 0;
    for (auto* weapon : weapons) {
        auto weapon_page = std::make_shared<rage_weapon_page_t>(
            nullptr, weapon, "", weapon_id++, &font);
        strip->add_child(weapon_page);
    }

    page->add_child(strip);
}

std::shared_ptr<c_menu_window>
build_framework_window(const Renderer::font& body_font, demo_layout& out)
{
    auto win = element_factory::make<c_menu_window>("Framework", &body_font);
    win->set_bounds({0, 0, 800, 600});  // Exact binary size

    auto tabs = element_factory::make<c_tab_manager>(c_tab_manager::vertical);
    tabs->set_tab_font(&body_font);
    win->add_child(tabs);
    out.frame_tabs = tabs;

    std::shared_ptr<c_tab> first;

    // Tab order matches the binary EXACTLY (dump menu__global.json):
    //   dev, Legit, Rage, Visuals, Miscellaneous, Settings.
    // The "dev" tab comes FIRST (user 2026-06-11: "dev is first in order before
    // legit"). Its subtabs mirror the binary's dev tab: Framework (the
    // Elements/Debug/Style showcase) + the renderer dev subtab.
    {
        // TWO separate dev tabs, matching the binary (user 2026-06-11):
        //   dev       -> subtabs: renderer, asdf
        //   Framework -> subtabs: Elements, Debug, Style
        auto dev_tab = build_demo_tab_with_subtabs("dev", body_font, {
            {"renderer", &populate_renderer_subtab},
            {"asdf",     &populate_asdf_subtab},
        });
        tabs->add_child(dev_tab);
        first = dev_tab;

        auto framework_tab = build_demo_tab_with_subtabs("Framework", body_font, {
            {"Elements", &populate_elements_subtab},
            {"Debug",    &populate_debug_subtab},
            {"Style",    &populate_style_subtab},
        });
        tabs->add_child(framework_tab);
    }

    // Cheat-side tabs (Legit/Rage/Visuals/Misc/Settings) AFTER dev.
    // `GUI::menu::build_cheat_menu` builds all five from the binary-verified
    // structure in one call.
    {
        GUI::menu::build_ctx ctx{};
        ctx.body_font   = &body_font;
        ctx.title_font  = &body_font;
        ctx.weapon_icon_font = g_weapon_icon_font;
        ctx.menu_kb_state = g_menu_kb_state;
        ctx.register_indicator_kb = [](const std::shared_ptr<GUI::c_keybinder>& kb,
                                       const char* tag,
                                       std::uint8_t r, std::uint8_t g,
                                       std::uint8_t b, std::uint8_t a) {
            register_indicator_kb(kb, tag, Renderer::color{r,g,b,a});
        };
        GUI::menu::build_cheat_menu(*tabs, ctx);
    }

    if (!first && !tabs->children().empty()) {
        first = std::dynamic_pointer_cast<c_tab>(tabs->children()[0]);
    }
    tabs->set_active_tab(first);

    // Demo dialog — created hidden, opened by the "Open dialog" button
    // in Framework > Elements > Dialog window. Bound rect centred over
    // the menu so the user can see the Confirm/Dismiss row clearly.
    g_demo_dialog = std::make_shared<c_dialog_window>("Confirm action");
    g_demo_dialog->set_title_font(&body_font);
    g_demo_dialog->set_button_font(&body_font);
    g_demo_dialog->set_modal(true);
    g_demo_dialog->set_visible(false);
    g_demo_dialog->set_bounds({240.0f, 180.0f, 320.0f, 160.0f});
    g_demo_dialog->set_on_confirm([] {
        // Demo handler — could trigger a real action here.
    });
    g_demo_dialog->set_on_dismiss([] {
        // Demo handler — user cancelled.
    });
    win->add_child(g_demo_dialog);

    // Changelog dialog — small, fixed size. Content sits inside a
    // c_listbox so it scrolls instead of stretching the dialog. User
    // 2026-06-02: "render like inside of something like a listbox …
    // smaller, way smaller like height not too big, width too".
    g_changelog_dialog = std::make_shared<c_dialog_window>("Changelogs:");
    g_changelog_dialog->set_title_font(&body_font);
    g_changelog_dialog->set_button_font(&body_font);
    g_changelog_dialog->set_modal(true);
    g_changelog_dialog->set_show_confirm(false);
    g_changelog_dialog->set_visible(false);
    // Auto-size so the dialog hugs the text container — the Dismiss
    // button then sits directly beneath it instead of after a big gap
    // (user 2026-06-11). No fixed set_size().
    g_changelog_dialog->set_auto_size(true);
    {
        // Changelog body is a SCROLLABLE TEXT BLOCK, not a listbox — the
        // user must not be able to hover/select lines like a dropdown
        // (user 2026-06-11). c_text_container wraps + scrolls plain text
        // with no per-line highlight.
        const std::string changelog_text =
            "//\n"
            "August 10-28:\n"
            "- likely lots of other changes that aren't listed\n"
            "- stronger xor string encryption (multiple keys)\n"
            "- extended \"build_options.hpp\" (compile-time config)\n"
            "- isolate framework specific config values to\n"
            "  their own file, separate from cheat settings\n"
            "- reduced binary size from 5 KB -> ~1.7 KB\n"
            "- switched font from \"Noto Sans\" to \"Segoe UI\"\n"
            "- added dropdown to select options for\n"
            "  watermark (see \"watermark_flag_e\" enum)";
        auto tc = element_factory::make<c_text_container>("");
        tc->set_font(&body_font);
        tc->set_text(changelog_text);
        // Bordered, scrollable, RECESSED text box — measured from the
        // original changelogs.png (user 2026-06-11): box interior 0xFF19191D
        // inset on the 0xFF202024 dialog body, 1-px 0xFF3E3E42 border, a
        // fixed ~239 px viewport (taller than the text, with a scrollbar)
        // and the Dismiss button just beneath it. Height scales with the
        // font. Width 306 → the auto-sized dialog lands near the 338 px
        // original.
        tc->set_recessed(true);
        const float lh = body_font.line_advance();
        tc->set_bounds({0.0f, 0.0f, 306.0f, lh * 15.0f});
        g_changelog_dialog->add_child(tc);
    }
    win->add_child(g_changelog_dialog);

    return win;
}

demo_layout build_demo(const Renderer::font& body_font,
                       std::uint32_t screen_w, std::uint32_t screen_h)
{
    demo_layout out;

    auto frame = build_framework_window(body_font, out);
    // Binary centering: pos = screen/2 - window/2
    frame->set_bounds({
        (screen_w - 800.0f) * 0.5f,
        (screen_h - 600.0f) * 0.5f,
        800.0f, 600.0f,
    });

    out.windows = { frame };
    return out;
}

// =============================================================================
// BACKDROP RENDERING - EXACT VALUES FROM BINARY
// =============================================================================
void draw_backdrop(Renderer::c_renderer_core& core, const Renderer::font& f,
                   const Renderer::font& unicode_font,
                   const Renderer::font& indicator_font,
                   std::uint32_t w, std::uint32_t h, float t_secs)
{
    using namespace Renderer;
    core.reset(rect{0, 0, static_cast<float>(w), static_cast<float>(h)});

    // Background color
    core.render_filled_rect(rect{0, 0, static_cast<float>(w), static_cast<float>(h)},
                            color{60, 60, 60, 255});

    constexpr float demo_scale = 1.0f; // 25 * (anim_speed + 3) * 0.01, default anim_speed=1

    // Top-left readout — matches the original (og-topleft capture):
    //   mem:   <n> mb      (label gray, value accent)
    //   fps:   <n>
    //
    //   focused elements:
    if (g_demo_text_drop_shadow->get()) {
        const auto  accent = GUI::style::current().accent;
        const color lbl{200, 200, 200, 255};

        // FPS — smoothed from the frame delta (draw_backdrop only gets
        // the elapsed time, so derive dt from a static last-time).
        static float s_last_t = 0.0f, s_fps = 0.0f;
        const float frame_dt = t_secs - s_last_t;
        s_last_t = t_secs;
        if (frame_dt > 0.0f)
            s_fps += ((1.0f / frame_dt) - s_fps) * 0.1f;

        // Working-set memory in MB.
        PROCESS_MEMORY_COUNTERS pmc{};
        unsigned mem_mb = 0;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
            mem_mb = static_cast<unsigned>(pmc.WorkingSetSize / (1024u * 1024u));

        char mem_buf[32], fps_buf[32];
        std::snprintf(mem_buf, sizeof(mem_buf), "%u mb", mem_mb);
        std::snprintf(fps_buf, sizeof(fps_buf), "%d",
                      static_cast<int>(s_fps + 0.5f));

        const float lx = 20.0f, vx = 120.0f;
        core.render_text(f, vec2{lx, 20.0f}, lbl,    "mem:");
        core.render_text(f, vec2{vx, 20.0f}, accent, mem_buf);
        core.render_text(f, vec2{lx, 36.0f}, lbl,    "fps:");
        core.render_text(f, vec2{vx, 36.0f}, accent, fps_buf);
        core.render_text(f, vec2{lx, 62.0f}, lbl,    "focused elements:");
    }

    // Unicode text demo, reconstructed from sub_1400077C0/sub_14000BFB0.
    if (g_demo_unicode_text->get()) {
        float text_x = std::floor(demo_scale * 20.0f);
        float text_y = std::floor(demo_scale * 175.0f);
        auto draw_unicode_line = [&](const char* s, bool gap = false) {
            text_y += std::floor(demo_scale * (gap ? 12.0f : 24.0f));
            core.render_text_drop_shadow(unicode_font, vec2{text_x, text_y},
                                         color{255, 255, 255, 255},
                                         color{0, 0, 0, 150},
                                         s);
        };

        draw_unicode_line("Unicode Text Rendering:");
        // Accent wavy underline directly beneath the heading (matches
        // the original — og-unicode-head capture, user 2026-06-10).
        {
            const float head_w = unicode_font.measure_text("Unicode Text Rendering:");
            const float wave_y = text_y + std::floor(demo_scale * 14.0f);
            const float amp    = std::max(1.0f, std::floor(demo_scale * 2.0f));
            const auto  acc    = GUI::style::current().accent;
            vec2 prev{text_x, wave_y};
            const int steps = 40;
            for (int i = 1; i <= steps; ++i) {
                const float u = static_cast<float>(i) / steps;
                const vec2 p{text_x + u * head_w,
                             wave_y + std::sin(u * 9.4247f) * amp};
                core.render_line(prev, p, color{acc.r, acc.g, acc.b, 255}, 1.5f);
                prev = p;
            }
        }
        draw_unicode_line("{ x | x ≤ ¾, x ≠ 0 }");
        draw_unicode_line("(-∞, 0) ∪ (0, ¾]");
        draw_unicode_line("π ∑ ≡ ≠ ≈ ∞ √ x ÷ ¹²³");
        draw_unicode_line("± Δ Π ∑ −/÷ ∟ ∩ ∫");
        draw_unicode_line("← ↑ ↓ → ● ▬");
        draw_unicode_line("μ ¶ » ¥ ₩ ₽ § ™ ✓");
        draw_unicode_line("½ ⅓ ¼ ⅕ ⅙ ⅞ ⅜ ⅝ ⅘");
        draw_unicode_line("₿ € ¢ ‰ !! :: •");
    }

    // =======================================================================
    // WATERMARK (gated by g_show_watermark — live checkbox in Theme groupbox)
    // =======================================================================
    if (g_show_watermark) {
        const char* s_brand  = "onetap.com";
        const char* s_build  = " [Build Name]";
        const char* s_sep1   = " | ";
        const char* s_user   = "anonteam.net";
        const char* s_sep2   = " | ";
        char s_time[16] = "00:00:00";
        {
            SYSTEMTIME st{};
            GetLocalTime(&st);
            std::snprintf(s_time, sizeof(s_time), "%02u:%02u:%02u",
                          st.wHour, st.wMinute, st.wSecond);
        }
        const char* s_sep3   = " | ";
        const char* s_ping   = "0 ms";

        const float pad_x = 10.0f;
        const float text_h = f.pixel_height();
        const float wr_h = 24.0f;

        const float w_brand  = f.measure_text(s_brand);
        const float w_build  = f.measure_text(s_build);
        const float w_sep1   = f.measure_text(s_sep1);
        const float w_user   = f.measure_text(s_user);
        const float w_sep2   = f.measure_text(s_sep2);
        const float w_time   = f.measure_text(s_time);
        const float w_sep3   = f.measure_text(s_sep3);
        const float w_ping   = f.measure_text(s_ping);

        const float content_w = w_brand + w_build + w_sep1 + w_user + w_sep2 + w_time + w_sep3 + w_ping;
        const float wr_w = content_w + 2.0f * pad_x;
        const rect wr{w - wr_w - 12.0f, 12.0f, wr_w, wr_h};

        const float wm_r = GUI::style::rounding().of(GUI::style::rounding().watermark);
        auto acc = GUI::style::current().accent;
        acc.a = 255;

        // Card body fill matching groupbox body (#202024 / alpha 240)
        core.render_filled_rounded_rect(wr, color{32, 32, 36, 240}, wm_r);

        // Concentric outer and inner borders matching menu window
        core.render_rounded_rect(rect{wr.x - 1.0f, wr.y - 1.0f, wr.w + 2.0f, wr.h + 2.0f},
                                 color{25, 25, 29, 255}, wm_r + 1.0f, 1.0f);
        core.render_rounded_rect(wr, color{62, 62, 66, 255}, wm_r, 1.0f);

        // Text typography
        const color c_brand {222, 222, 222, 255};
        const color c_build {135, 135, 142, 255}; // darker build name text
        const color c_user  = acc; // user segment highlighted in theme accent
        const color c_info  {195, 195, 200, 255};
        const color c_dim   {85, 85, 92, 255};

        float tx = wr.x + (wr.w - content_w) * 0.5f;
        const float ty = wr.y + (wr_h - text_h) * 0.5f - 0.5f;
        core.render_text(f, vec2{tx, ty}, c_brand, s_brand); tx += w_brand;
        core.render_text(f, vec2{tx, ty}, c_build, s_build); tx += w_build;
        core.render_text(f, vec2{tx, ty}, c_dim,   s_sep1);   tx += w_sep1;
        core.render_text(f, vec2{tx, ty}, c_user,  s_user);   tx += w_user;
        core.render_text(f, vec2{tx, ty}, c_dim,   s_sep2);   tx += w_sep2;
        core.render_text(f, vec2{tx, ty}, c_info,  s_time);   tx += w_time;
        core.render_text(f, vec2{tx, ty}, c_dim,   s_sep3);   tx += w_sep3;
        core.render_text(f, vec2{tx, ty}, c_info,  s_ping);
    }

    // =======================================================================
    // CIRCLE DEMO - Exact from binary @ 0x14000D440
    // Scale: 25 * (anim_speed + 3) * 0.01 = 1.0 with default anim_speed=1.
    // =======================================================================
    float scale = demo_scale;
    float radius_base = std::floor(scale * 75.0f);
    float x_spacing = std::floor(scale * 250.0f);
    float y_row1 = std::floor(scale * 175.0f);
    float x_offset = std::floor(scale * 50.0f);
    float x_base = x_offset + 2.0f * radius_base;

    // Animation
    float circ_anim = get_circle_anim(t_secs);

    // ROW 1: Gradient circles at y=175*scale — gated by gradient_circle_demo.
    float y1 = y_row1;
    float circle_xs[] = {
        x_base + x_spacing,
        x_spacing + 2.0f * x_base,
        x_base + x_spacing + 2.0f * x_base,
        x_spacing + 4.0f * x_base,
    };
    if (g_demo_gradient_circle->get()) {
        for (int i = 0; i < 4; ++i) {
            core.render_gradient_circle(vec2{circle_xs[i], y1}, radius_base,
                                        circle_colors[i * 2],
                                        circle_colors[i * 2 + 1]);
        }
    }

    // ROW 2: engine slot +352 ring/circle primitive from sub_14000D440.
    // Shaders demo OWNS this ring band (the binary calls the ring shader
    // here). Gate on shaders_demo.
    float y2 = y1 + x_base;
    float phase_offsets[] = {25.0f, 12.5f, 3.125f, 6.25f};
    int ring_types[] = {3, 2, 0, 1};
    if (g_demo_shaders->get()) {
        for (int i = 0; i < 4; ++i) {
            float phase = circ_anim * phase_offsets[i];
            const vec2 c{circle_xs[i], y2};
            render_demo_ring(core, c, radius_base, ring_types[i], phase,
                             circle_colors[(i + 4) % 8],
                             circle_colors[(i + 5) % 8]);
        }
    }

    const float fh = static_cast<float>(h);

    // =======================================================================
    // POLYGON DEMO - Exact coordinate basis from binary @ 0x14000B210
    // Gated by polygon_demo checkbox (user 2026-06-01).
    // =======================================================================
    if (g_demo_polygon->get()) {
    float poly_anim = get_polygon_anim(t_secs);
    color poly_hsv = hsv_to_rgb(poly_anim * 240.0f);
    color poly_outline = color::lerp(color{0, 0, 0, 255}, color{255, 255, 255, 255}, poly_anim);
    float poly_radius = std::floor(demo_scale * 20.0f);
    float poly_base_x = std::floor(demo_scale * 350.0f);
    float poly_y = std::floor(demo_scale * 60.0f);
    float poly_offset = std::floor(demo_scale * 60.0f);
    float poly_angle = poly_anim * 2.0f * 3.14159265f;
    float line_dx = std::floor(demo_scale * static_cast<float>(static_cast<int>(poly_anim * 60.0f)));
    float line_dy = 1.0f + std::floor(demo_scale * static_cast<float>(static_cast<int>(poly_anim * 50.0f)));

    core.render_text(f, vec2{poly_base_x + poly_offset, std::floor(demo_scale * 20.0f)},
                     poly_hsv, "Polygon and line rendering test");
    core.render_circle(vec2{poly_base_x + poly_offset, std::floor(demo_scale * 20.0f)},
                       poly_radius, poly_hsv, std::floor(demo_scale * 2.0f));
    // Outer ring animates BLACK↔WHITE (opaque), same as the polygon
    // outlines. User 2026-06-10: it was white with animated alpha, so it
    // faded to TRANSPARENT instead of turning black.
    core.render_circle(vec2{poly_base_x + poly_offset, std::floor(demo_scale * 20.0f)},
                       poly_radius + std::floor(demo_scale * 2.0f),
                       poly_outline, 1.0f);

    // Triangle (Red) - 0xFFFF0000
    vec2 tri[3];
    for (int i = 0; i < 3; ++i) {
        float ang = (static_cast<float>(i * 2) * 3.14159265f) / 3.0f + poly_angle;
        tri[i] = {std::round(poly_base_x + poly_radius * std::cos(ang)),
                  std::round(poly_y + poly_radius * std::sin(ang))};
    }
    core.render_filled_polygon(tri, 3, color{255, 0, 0, 255});
    core.render_polygon(tri, 3, poly_outline, 1.0f);
    vec2 tri_target{poly_base_x + line_dx, poly_y + line_dy};
    core.render_line(vec2{poly_base_x, poly_y}, tri_target, color{255, 0, 0, 255}, 2.0f);

    // N-gon (Green) - 0xFF008000, dynamic vertex count
    int ngon_verts = static_cast<int>(std::round(poly_anim * 59.0f)) + 5;
    float ng_cx = poly_base_x + poly_offset;
    std::vector<vec2> ngon(ngon_verts);
    for (int i = 0; i < ngon_verts; ++i) {
        float ang = (static_cast<float>(i * 2) * 3.14159265f) / static_cast<float>(ngon_verts) + poly_angle;
        ngon[i] = {std::round(ng_cx + poly_radius * std::cos(ang)),
                   std::round(poly_y + poly_radius * std::sin(ang))};
    }
    core.render_filled_polygon(ngon.data(), ngon_verts, color{0, 128, 0, 255});
    core.render_polygon(ngon.data(), ngon_verts, poly_outline, 1.0f);
    core.render_line(vec2{ng_cx, poly_y}, vec2{ng_cx, poly_y + line_dy},
                     color{0, 128, 0, 255}, 2.0f);
    char ngon_label[32]{};
    std::snprintf(ngon_label, sizeof(ngon_label), "%d", ngon_verts);
    core.render_text(f, vec2{ng_cx + 10.0f, poly_y + 10.0f},
                     color{255, 255, 255, 255}, ngon_label);

    // Square (Blue) - 0xFF0000FF, offset right
    float sq_cx = poly_base_x + std::floor(demo_scale * 120.0f);
    vec2 sq[4];
    for (int i = 0; i < 4; ++i) {
        float ang = (static_cast<float>(i * 2) * 3.14159265f) * 0.25f + poly_angle;
        sq[i] = {std::round(sq_cx + poly_radius * std::cos(ang)),
                 std::round(poly_y + poly_radius * std::sin(ang))};
    }
    core.render_filled_polygon(sq, 4, color{0, 0, 255, 255});
    core.render_polygon(sq, 4, poly_outline, 1.0f);
    core.render_line(vec2{sq_cx, poly_y}, vec2{sq_cx - line_dx, poly_y + line_dy},
                     color{0, 0, 255, 255}, 2.0f);
    } // end polygon_demo gate

    // =======================================================================
    // BEZIER CURVES + CTRL POINTS - From binary with animation
    // (sub_14000C400/CD30). Gated by bezier_curve_demo. The "ctrl pts"
    // label + dots/lines only render when render_bezier_curve_control_points
    // is ALSO on. bezier_curve_scale slider drives `bezier_scale`.
    // =======================================================================
    if (g_demo_bezier_curve->get()) {
    float bez_anim = get_bezier_anim(t_secs);
    float bez_scale = demo_scale;
    float bezier_scale = g_demo_bezier_scale->get();
    float bez_base_x = std::floor(bez_scale * 150.0f);
    float bez_base_y = std::floor(bez_scale * 800.0f);
    if (bez_base_y > fh - 210.0f) {
        bez_base_y = fh - 210.0f;
    }
    float bez_w_small = ((bez_anim * 20.0f) + 175.0f) * bezier_scale;
    float bez_w_large = ((bez_anim * 40.0f) + 350.0f) * bezier_scale;
    float bez_shift_x = std::floor((bez_anim * 140.0f) * bez_scale);
    float bez_shift_y = std::floor((bez_anim * 60.000004f) * bez_scale);
    float pt_radius = std::floor(bez_scale * 4.0f);

    const bool show_ctrl_pts = g_demo_bezier_ctrl_pts->get();

    // ORIGINAL (orig-now-1 capture): the demo curves render as FILLED
    // red/blue lobes (curve sampled + chord closure), not just strokes.
    auto fill_bezier_quad = [&](vec2 a, vec2 b, vec2 c, color col) {
        vec2 pts[33];
        for (int i = 0; i <= 32; ++i) {
            const float t = i / 32.0f, u = 1.0f - t;
            pts[i] = {u*u*a.x + 2.0f*u*t*b.x + t*t*c.x,
                      u*u*a.y + 2.0f*u*t*b.y + t*t*c.y};
        }
        core.render_filled_polygon(pts, 33, col);
    };
    auto fill_bezier_cubic = [&](vec2 a, vec2 b, vec2 c, vec2 d, color col) {
        vec2 pts[33];
        for (int i = 0; i <= 32; ++i) {
            const float t = i / 32.0f, u = 1.0f - t;
            const float w0 = u*u*u, w1 = 3.0f*u*u*t, w2 = 3.0f*u*t*t, w3 = t*t*t;
            pts[i] = {w0*a.x + w1*b.x + w2*c.x + w3*d.x,
                      w0*a.y + w1*b.y + w2*c.y + w3*d.y};
        }
        core.render_filled_polygon(pts, 33, col);
    };

    auto render_ctrl_pt = [&](vec2 p, color fill, const char* prefix, int index) {
        if (!show_ctrl_pts) return;
        core.render_filled_circle(p, pt_radius, fill);
        core.render_circle(p, pt_radius + 0.5f, color{0, 0, 0, 255}, 1.0f);

        char label[32]{};
        std::snprintf(label, sizeof(label), "%s: ctrl pt #%d", prefix, index);
        core.render_text(f, vec2{p.x + 10.0f, p.y + 10.0f},
                         color{255, 255, 255, 255}, label);
    };

    // Cubic bezier
    float bez_start = std::floor(bez_scale * 200.0f);
    vec2 cubic_p0{bez_start + bez_base_x, bez_start + bez_base_y};
    vec2 cubic_p1{bez_start + bez_base_x + bez_shift_x, bez_start + bez_base_y + bez_shift_y};
    vec2 cubic_p2{bez_start + bez_base_x + bez_shift_y, bez_start + bez_base_y + bez_shift_x};
    vec2 cubic_p3{std::floor(bez_scale * 400.0f) + bez_base_x, std::floor(bez_scale * 400.0f) + bez_base_y};
    // Original palette: blue + red, curves FILLED.
    fill_bezier_cubic(cubic_p0, cubic_p1, cubic_p2, cubic_p3,
                      color{74, 130, 220, 170});
    core.render_bezier_cubic(cubic_p0, cubic_p1, cubic_p2, cubic_p3,
                             color{74, 130, 220, 255}, 2.0f);
    if (show_ctrl_pts) {
        core.render_line(cubic_p0, cubic_p1, color{74, 130, 220, 80}, 1.0f);
        core.render_line(cubic_p2, cubic_p3, color{74, 130, 220, 80}, 1.0f);
    }
    render_ctrl_pt(cubic_p0, color{255, 255, 255, 255}, "1", 0);
    render_ctrl_pt(cubic_p1, color{0, 96, 255, 255}, "1", 1);
    render_ctrl_pt(cubic_p2, color{255, 48, 48, 255}, "1", 2);
    render_ctrl_pt(cubic_p3, color{255, 255, 255, 255}, "1", 3);

    // Quadratic bezier
    vec2 quad_p0{bez_base_x, bez_base_y + (bez_anim * 0.3f * 0.8f) * bez_w_large};
    vec2 quad_p1{bez_base_x + (bez_anim * 0.8f * 0.2f) * bez_w_small,
                 bez_base_y - (bez_anim * 0.5f * 0.4f) * bez_w_large};
    vec2 quad_p2{bez_base_x + bez_w_small,
                 bez_base_y + (bez_anim * 1.2f * 0.6f) * bez_w_large};
    fill_bezier_quad(quad_p0, quad_p1, quad_p2, color{74, 130, 220, 170});
    core.render_bezier_quadratic(quad_p0, quad_p1, quad_p2,
                                 color{74, 130, 220, 255}, 2.0f);
    if (show_ctrl_pts) {
        core.render_line(quad_p0, quad_p1, color{74, 130, 220, 80}, 1.0f);
        core.render_line(quad_p1, quad_p2, color{74, 130, 220, 80}, 1.0f);
    }
    render_ctrl_pt(quad_p0, color{255, 255, 255, 255}, "2", 0);
    render_ctrl_pt(quad_p1, color{0, 96, 255, 255}, "2", 1);
    render_ctrl_pt(quad_p2, color{255, 255, 255, 255}, "2", 2);

    vec2 cubic2_p0{quad_p2.x, quad_p2.y};
    vec2 cubic2_p1{bez_base_x + (bez_anim * 1.2f * 0.5f) * bez_w_small,
                   bez_base_y + (bez_anim * 0.5f * 0.5f) * bez_w_large};
    vec2 cubic2_p2{bez_base_x + (bez_anim * 0.8f * 3.0f) * bez_w_small,
                   bez_base_y - (bez_anim * 0.3f * 1.5f) * bez_w_large};
    vec2 cubic2_p3{cubic_p0.x, cubic_p0.y};
    fill_bezier_cubic(cubic2_p0, cubic2_p1, cubic2_p2, cubic2_p3,
                      color{229, 53, 44, 170});
    core.render_bezier_cubic(cubic2_p0, cubic2_p1, cubic2_p2, cubic2_p3,
                             color{229, 53, 44, 255}, 2.0f);
    if (show_ctrl_pts) {
        core.render_line(cubic2_p0, cubic2_p1, color{229, 53, 44, 80}, 1.0f);
        core.render_line(cubic2_p2, cubic2_p3, color{229, 53, 44, 80}, 1.0f);
    }
    render_ctrl_pt(cubic2_p0, color{255, 255, 255, 255}, "3", 0);
    render_ctrl_pt(cubic2_p1, color{255, 170, 40, 255}, "3", 1);
    render_ctrl_pt(cubic2_p2, color{0, 96, 255, 255}, "3", 2);
    render_ctrl_pt(cubic2_p3, color{255, 255, 255, 255}, "3", 3);

    // "Ctrl pts" label belongs to the control-points overlay — only
    // show when bezier curve demo is on AND render_bezier_curve_control_points.
    if (show_ctrl_pts) {
        core.render_text(f, vec2{bez_base_x, bez_base_y - 26.0f},
                         color{200, 200, 200, 255}, "Ctrl pts");
    }
    } // end bezier_curve_demo gate

    // =======================================================================
    // SPECTATORS/KEYS PANELS — binary @ 0x140068880, 0x140061620.
    // Look matched to the ORIGINAL exe (orig-now-panels2.png):
    //   * chrome = TITLE BAR ONLY — 0xBF12121D fill, 0xBF3E3E42 1px
    //     border, title 0xFFDFD59E centered. The Keys bar additionally
    //     gets a 1px ACCENT underline; Spectators does not.
    //   * rows are FRAMELESS text under the bar (no body fill/border);
    //     spectator rows carry the profile avatar at 12x12.
    //   * Keys rows are LIVE — one row per currently-active keybind
    //     ("Open GUI" while the menu is toggled on, plus any registered
    //     cheat bind whose mode currently fires).
    //   * both panels DRAG by their title bar while the menu is open
    //     (user 2026-06-10).
    // =======================================================================
    {
        const float k = GUI::style::gui_scale();
        const float panel_w   = 190.0f * k;
        const float bar_h     = 22.0f * k;
        const float row_h     = 22.0f * k;
        const float icon_sz   = 14.0f * k;   // avatar square
        const float row_inset = 8.0f * k;    // left/right padding
        const float card_r    = GUI::style::rounding().of(GUI::style::rounding().groupbox);
        auto acc = GUI::style::current().accent;
        acc.a = 255;

        static vec2 spec_pos{20.0f, 0.0f};
        static vec2 keys_pos{20.0f, 0.0f};
        static bool pos_init = false;
        if (!pos_init) {
            spec_pos.y = fh * 0.5f - 140.0f;
            keys_pos.y = fh * 0.5f + 20.0f;
            pos_init = true;
        }
        static int  drag_target = -1;   // 0 = spectators, 1 = keys
        static vec2 drag_off{};

        auto& kin = GUI::input::current();
        auto drag_panel = [&](vec2& pos, int id) {
            const rect bar{pos.x, pos.y, panel_w, bar_h};
            if (g_menu_visible && drag_target == -1 &&
                kin.mouse_pressed[GUI::input::mb_left] &&
                bar.contains(kin.mouse_pos)) {
                drag_target = id;
                drag_off = {kin.mouse_pos.x - pos.x, kin.mouse_pos.y - pos.y};
                kin.consume_mouse_press();
            }
            if (drag_target == id) {
                if (kin.mouse_down[GUI::input::mb_left]) {
                    pos.x = kin.mouse_pos.x - drag_off.x;
                    pos.y = kin.mouse_pos.y - drag_off.y;
                } else {
                    drag_target = -1;
                }
            }
        };
        drag_panel(spec_pos, 0);
        drag_panel(keys_pos, 1);

        auto render_panel_card = [&](vec2 pos, float total_h, const char* title) {
            const rect card_rect{pos.x, pos.y, panel_w, total_h};
            const rect header_rect{pos.x, pos.y, panel_w, bar_h};

            // Full card body fill (#202024)
            core.render_filled_rounded_rect(card_rect, color{32, 32, 36, 240}, card_r);

            // Header top fill with top-only rounded corners (#19191D)
            const float header_radii[4] = {card_r, card_r, 0.0f, 0.0f};
            core.render_filled_rounded_rect_ex(header_rect, color{25, 25, 29, 240}, header_radii);

            // Accent underline separating header bar from body
            core.render_filled_rect(rect{pos.x + 1.0f, pos.y + bar_h - 1.0f, panel_w - 2.0f, 1.0f}, acc);

            // Double concentric border rings matching groupbox / menu (outer + inner)
            core.render_rounded_rect(rect{card_rect.x - 1.0f, card_rect.y - 1.0f,
                                          card_rect.w + 2.0f, card_rect.h + 2.0f},
                                     color{25, 25, 29, 255}, card_r + 1.0f, 1.0f);
            core.render_rounded_rect(card_rect, color{62, 62, 66, 255}, card_r, 1.0f);

            // Title text centered (#DEDEDE)
            const float tw = f.measure_text(title);
            core.render_text(f,
                vec2{pos.x + (panel_w - tw) * 0.5f,
                     pos.y + (bar_h - f.pixel_height()) * 0.5f},
                color{222, 222, 222, 255}, title);
        };

        const color row_text{220, 220, 220, 255};
        const color badge_text{140, 140, 148, 255};

        // --- Spectators Panel ---
        const std::vector<const char*> spec_players = {
            "Player 1", "Player 2", "Player 3", "Player 4", "Player 5"
        };
        const float spec_total_h = bar_h + static_cast<float>(spec_players.size()) * row_h + 6.0f * k;
        render_panel_card(spec_pos, spec_total_h, "Spectators");
        {
            float ry = spec_pos.y + bar_h + 4.0f * k;
            for (auto* s : spec_players) {
                const float text_ty = ry + (row_h - f.pixel_height()) * 0.5f;
                const rect icon{spec_pos.x + row_inset, ry + (row_h - icon_sz) * 0.5f,
                                icon_sz, icon_sz};

                // Avatar with 1px border
                if (g_profile_tex.srv) {
                    core.render_texture(icon, g_profile_tex, color{255, 255, 255, 255});
                } else {
                    core.render_filled_rect(icon, color{58, 68, 96, 255});
                }
                core.render_rect(icon, color{62, 62, 66, 255}, 1.0f);

                // Player name
                core.render_text(f, vec2{icon.x + icon_sz + 6.0f * k, text_ty}, row_text, s);

                // Spectating mode tag on right side
                const char* tag = "[1st]";
                const float tag_w = f.measure_text(tag);
                core.render_text(f, vec2{spec_pos.x + panel_w - row_inset - tag_w, text_ty}, badge_text, tag);

                ry += row_h;
            }
        }

        // --- Keys (Keybinds) Panel ---
        struct key_item_t {
            std::string label;
            std::string mode;
        };
        std::vector<key_item_t> active_keys;
        if (g_menu_visible) {
            active_keys.push_back({"Open GUI", "[toggle]"});
        }
        for (auto& e : g_indicator_kbs) {
            auto kb = e.kb.lock();
            if (!kb) continue;
            const auto b = kb->bind();
            if (b.key == 0 || !kb->active(kin)) continue;
            const char* mode_str =
                b.mode == GUI::keybinder_mode_e::always ? "[always]" :
                b.mode == GUI::keybinder_mode_e::toggle ? "[toggle]"
                                                        : "[hold]";
            active_keys.push_back({kb->label(), mode_str});
        }

        const int num_keys = static_cast<int>(active_keys.size());
        const float keys_total_h = bar_h + (num_keys > 0 ? (static_cast<float>(num_keys) * row_h + 6.0f * k) : (row_h + 6.0f * k));
        render_panel_card(keys_pos, keys_total_h, "Keys");
        {
            float ry = keys_pos.y + bar_h + 4.0f * k;
            if (num_keys == 0) {
                const float text_ty = ry + (row_h - f.pixel_height()) * 0.5f;
                const char* none_str = "[None active]";
                const float none_w = f.measure_text(none_str);
                core.render_text(f, vec2{keys_pos.x + (panel_w - none_w) * 0.5f, text_ty}, badge_text, none_str);
            } else {
                for (const auto& item : active_keys) {
                    const float text_ty = ry + (row_h - f.pixel_height()) * 0.5f;
                    // Bind name on left
                    core.render_text(f, vec2{keys_pos.x + row_inset, text_ty}, row_text, item.label.c_str());
                    // Mode badge on right
                    const float mode_w = f.measure_text(item.mode.c_str());
                    core.render_text(f, vec2{keys_pos.x + panel_w - row_inset - mode_w, text_ty}, acc, item.mode.c_str());
                    ry += row_h;
                }
            }
        }
    }

    // ===================================================================
    // CUSTOM KEYBIND INDICATORS — user 2026-06-01:
    // Render ONLY the registered keybinders that are currently active
    // (Always always; Toggle when latched; Hold while held). Per-entry
    // fade tween (anim) lerps 0→1 when active, 1→0 when not so rows
    // slide in / out smoothly. Whole panel hides when no rows visible.
    // ===================================================================
    // HUD indicator strip — pixel-precise port of
    // `inspiration-indicators/ANALYSIS.md`.
    //
    // The analysis document is rendered at 2× scale; the in-game capture
    // (`cs-background.png`) is the true reference scale. All geometry
    // below is the 1× in-game pixel size (analysis values / 2):
    //
    //   band height       = 26 px         (analysis: 52)
    //     top bar         = 1 px, peak α = 69/255   (pure black + envelope)
    //     body            = 24 px, peak α = 40/255  (pure black + envelope)
    //     bot bar         = 1 px, peak α = 73/255   (pure black + envelope)
    //   body envelope     = 25-px LINEAR ramp (analysis: 50)
    //   bar envelope      = 32-px LINEAR ramp (analysis: 64) — bars fade
    //                       slower than body, that's the "shadow density"
    //                       difference (user 2026-06-02).
    //   inter-band gap    = 4 px (analysis: 7)
    //   icon slot cx      = chrome_left + 20 px (analysis: +40)
    //   text x            = chrome_left + 35 px (analysis: +69)
    //   font              = Verdana Bold ~14 pt → cap-height ~13 px
    //
    // DISABLED per user 2026-06-10 ("disable indicators, comment them").
    // The registration plumbing stays so re-enabling is a one-line flip.
#if 0
    {
        constexpr float band_h         = 26.0f;
        constexpr float body_ramp_w    = 25.0f;
        constexpr float bar_ramp_w     = 32.0f;
        constexpr float gap_y          = 4.0f;
        constexpr float stride_y       = band_h + gap_y;
        constexpr float chrome_left    = 15.0f;       // screen offset to left ramp start
        constexpr float icon_slot_cx   = 20.0f;       // from chrome_left
        constexpr float text_dx        = 35.0f;       // from chrome_left
        const float strip_y_anchor     = fh * 0.30f;

        auto& kin = GUI::input::current();
        const float lerp_k = 0.20f;
        std::size_t visible_row = 0;
        for (auto& e : g_indicator_kbs) {
            auto kb = e.kb.lock();
            const bool live = kb && kb->active(kin);
            const float target = live ? 1.0f : 0.0f;
            e.anim += (target - e.anim) * lerp_k;
            if (e.anim < 0.005f) continue;

            const float t   = std::min(1.0f, std::max(0.0f, e.anim));
            const float eo  = 1.0f - std::pow(1.0f - t, 3.0f);
            const float slide_off = (1.0f - eo) * -8.0f;
            const float anim_a    = eo;

            // ── Content widths (measure with the indicator font) ──────────
            const float tag_w_px   = indicator_font.measure_text(e.short_tag.c_str());
            const char* label_cstr = kb ? kb->label().c_str() : "(?)";
            const float label_w_px = indicator_font.measure_text(label_cstr);
            const float content_w  = (text_dx - chrome_left)
                                   + label_w_px + 10.0f;
            // Plateau width MUST cover the longest envelope's plateau
            // (bars use a wider ramp, so the plateau they need is the
            // same or narrower than the body's). Use the wider of the
            // two ramps when computing the minimum plateau so the bars
            // still have a flat region in the middle.
            const float plateau_w  = std::max(20.0f, content_w);

            const float bx0 = chrome_left + slide_off;
            const float by  = strip_y_anchor
                + static_cast<float>(visible_row) * stride_y;

            // Three Y rows + per-layer α peak AND ramp width.
            struct layer { float yo, h; float peak_a; float ramp_w; };
            const layer layers[3] = {
                { 0.0f,         1.0f, 69.0f, bar_ramp_w  },  // top bar
                { 1.0f,        24.0f, 40.0f, body_ramp_w },  // body (24, NOT 50!)
                { 25.0f,        1.0f, 73.0f, bar_ramp_w  },  // bottom bar
            };

            for (const auto& L : layers) {
                const float ly = by + L.yo;
                const std::uint8_t peak_a =
                    static_cast<std::uint8_t>(L.peak_a * anim_a);
                if (peak_a == 0) continue;
                const color zero { 0, 0, 0, 0 };
                const color peak { 0, 0, 0, peak_a };

                // Left ramp: α 0 → peak over `L.ramp_w` px.
                core.render_gradient_rect(
                    rect{bx0, ly, L.ramp_w, L.h},
                    zero, peak, peak, zero);
                // Plateau: solid peak.
                core.render_filled_rect(
                    rect{bx0 + L.ramp_w, ly, plateau_w, L.h}, peak);
                // Right ramp: peak → 0 over `L.ramp_w` px.
                core.render_gradient_rect(
                    rect{bx0 + L.ramp_w + plateau_w, ly, L.ramp_w, L.h},
                    peak, zero, zero, peak);
            }

            // ── Foreground tint (icon + text share the row's colour) ─────
            const color tint_full{ e.tint.r, e.tint.g, e.tint.b,
                static_cast<std::uint8_t>(255.0f * anim_a) };
            const float band_mid_y  = by + band_h * 0.5f;
            const float text_glyph_h = indicator_font.pixel_height();

            // Icon — no texture available, render the short-tag in the
            // icon slot (centred). Real impl would draw the SVG glyph.
            const float icon_cx = bx0 + (icon_slot_cx - chrome_left);
            core.render_text(indicator_font,
                vec2{icon_cx - tag_w_px * 0.5f,
                     band_mid_y - text_glyph_h * 0.5f},
                tint_full, e.short_tag.c_str());

            // Label text at chrome_left + 35.
            core.render_text(indicator_font,
                vec2{bx0 + (text_dx - chrome_left),
                     band_mid_y - text_glyph_h * 0.5f},
                tint_full, label_cstr);

            ++visible_row;
        }
    }
#endif  // indicators disabled
    (void)indicator_font;

    // (the "Ctrl pts" label moved INSIDE the bezier_curve_demo +
    // ctrl-pts gates above so it no longer renders when those toggles
    // are off — user 2026-06-01.)
}

// (Custom `draw_changelog_popup` removed 2026-06-02: changelogs are
// now built as a c_dialog_window in build_framework_window(), opened
// by the Display changelogs button. The dialog handles its own
// title-centring, modal blocking, and overlay-layer rendering.)

} // anon

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    // Raise the OS timer resolution to 1ms. Without this, Sleep()
    // rounds up to the default ~15.6ms tick → the fps limiter could
    // never exceed ~64fps regardless of the slider (user 2026-06-10:
    // "fps is 64 max by default not 300").
    timeBeginPeriod(1);

    HWND hWnd = demo::open_window(hInstance);
    if (!hWnd) return 1;

    // Seed GUI scale from the host monitor's DPI BEFORE building any
    // widgets so initial sizes are correct. PER_MONITOR_AWARE_V2 means
    // Windows hands us physical pixels with no auto-scaling, so we have
    // to do this ourselves. 96 DPI is the 1.0x baseline.
    {
        const UINT dpi = GetDpiForWindow(hWnd);
        if (dpi > 0) {
            const float k = static_cast<float>(dpi) / 96.0f;
            GUI::style::set_gui_scale(k);
        }
    }

    demo::d3d_handles d3d{};
    if (!demo::init_d3d11(hWnd, d3d)) { demo::shutdown_d3d11(d3d); return 1; }

    // dev > renderer > "runtime info" labels format these (binary:
    // "g_device addr: {}" / "g_device_context addr: {}" / "g_swap_chain addr: {}").
    g_rt_device_addr     = d3d.device;
    g_rt_context_addr    = d3d.context;
    g_rt_swap_chain_addr = d3d.swap_chain;

    RECT cr{}; GetClientRect(hWnd, &cr);
    const std::uint32_t width  = static_cast<std::uint32_t>(cr.right  - cr.left);
    const std::uint32_t height = static_cast<std::uint32_t>(cr.bottom - cr.top );
    globals::g_screen_w = width;
    globals::g_screen_h = height;

    Renderer::c_dx11_renderer renderer;
    renderer.attach(d3d.device, d3d.context, d3d.swap_chain, d3d.rtv, width, height);
    if (!renderer.initialize()) { demo::shutdown_d3d11(d3d); return 1; }

    Renderer::c_renderer_core core{renderer.draws(), renderer.overlay_draws()};

    // Fonts must load at SCALED pixel heights so they grow with the
    // chrome on high-DPI monitors. set_gui_scale() ran above so
    // gui_scale() returns the monitor's DPI-derived factor here.
    const float font_k = GUI::style::gui_scale();
    Renderer::font body_font, title_font, tab_font, unicode_font,
                   weapon_icon_font, indicator_font, player_icon_font_obj;
    // Font sizes bumped ~1-2 px (user 2026-06-10: original text is a
    // touch larger). body 13→14, title 14→15, tab 17→19.
    body_font .load_from_file(d3d.device, GUI_DEFAULT_FONT_PATH, 14.0f * font_k);
    title_font.load_from_file(d3d.device, GUI_DEFAULT_FONT_PATH, 15.0f * font_k);
    // Tab font — larger and used ONLY for tab/subtab labels.
    tab_font  .load_from_file(d3d.device, GUI_DEFAULT_FONT_PATH, 19.0f * font_k);
    if (!unicode_font.load_from_file(d3d.device, L"C:\\Windows\\Fonts\\seguisym.ttf", 18.0f * font_k)) {
        unicode_font.load_from_file(d3d.device, GUI_DEFAULT_FONT_PATH, 18.0f * font_k);
    }
    // Indicator font — `inspiration-indicators/ANALYSIS.md §4b`:
    // Verdana Bold ~14 pt in-game (~18 px em, cap-height ~13-14 px).
    // Fallback chain: Verdana Bold → Tahoma Bold (closest Windows
    // bundled) → default font.
    if (!indicator_font.load_from_file(
            d3d.device, L"C:\\Windows\\Fonts\\verdanab.ttf", 14.0f * font_k)) {
        if (!indicator_font.load_from_file(
                d3d.device, L"C:\\Windows\\Fonts\\tahomabd.ttf", 14.0f * font_k)) {
            indicator_font.load_from_file(
                d3d.device, GUI_DEFAULT_FONT_PATH, 14.0f * font_k);
        }
    }

    // Player-icon font (Visuals > Players strip: Enemy/Team/Self person
    // glyphs at 'A'/'B'/'C') — the binary's embedded Glyphter TTF.
    // Both icon fonts are EMBEDDED now (demo/embedded_assets.h) and baked
    // from memory — no asset-dir probe, always available.
    {
        const std::vector<Renderer::font::unicode_range> abc = {
            {0x0041, 0x0043},
        };
        if (player_icon_font_obj.load_from_memory_with_ranges(
                d3d.device, demo::embedded::g_font_player_icons_otf,
                demo::embedded::g_font_player_icons_otf_size,
                /*pixel_height=*/15.0f * font_k, /*aliased=*/false, abc)) {
            GUI::player_icon_font() = &player_icon_font_obj;
        }
    }

    // Weapon-icon font (Rage > Weapons strip) — Private Use Area glyphs.
    {
        const std::vector<Renderer::font::unicode_range> pua_ranges = {
            {0xE000, 0xF8FF},
        };
        // ~15 px so the Rage/Legit weapon glyphs read small like the
        // Visuals Enemy/Team/Self icons (user 2026-06-11). Scales w/ DPI.
        if (weapon_icon_font.load_from_memory_with_ranges(
                d3d.device, demo::embedded::g_font_csgo_icons_otf,
                demo::embedded::g_font_csgo_icons_otf_size,
                /*pixel_height=*/15.0f * font_k, /*aliased=*/false,
                pua_ranges)) {
            g_weapon_icon_font = &weapon_icon_font;
        }
    }

    auto layout = build_demo(body_font, width, height);

    // User 2026-06-02: DPI scaling must NOT change the screen
    // resolution. The OS window + DXGI swapchain stay put; we only
    // rebake fonts at new pixel heights and resize the menu_window
    // internally so its chrome lays out at the new scale. The
    // menu can overflow the screen at high scales — user can drag
    // it back to centre. This matches the binary's
    // style_scale_value-based scalable_size_t registry (every size
    // re-derives from its base, nothing touches the host window).
    GUI::style::register_scale_callback(
        [dev = d3d.device, &body_font, &title_font, &tab_font,
         &unicode_font, &indicator_font, &weapon_icon_font,
         &player_icon_font_obj, &layout](float k) {
            body_font .release(); body_font .load_from_file(dev, GUI_DEFAULT_FONT_PATH, 14.0f * k);
            title_font.release(); title_font.load_from_file(dev, GUI_DEFAULT_FONT_PATH, 15.0f * k);
            tab_font  .release(); tab_font  .load_from_file(dev, GUI_DEFAULT_FONT_PATH, 19.0f * k);
            unicode_font.release();
            if (!unicode_font.load_from_file(dev,
                    L"C:\\Windows\\Fonts\\seguisym.ttf", 18.0f * k))
                unicode_font.load_from_file(dev, GUI_DEFAULT_FONT_PATH, 18.0f * k);
            indicator_font.release();
            if (!indicator_font.load_from_file(dev,
                    L"C:\\Windows\\Fonts\\verdanab.ttf", 14.0f * k)) {
                if (!indicator_font.load_from_file(dev,
                        L"C:\\Windows\\Fonts\\tahomabd.ttf", 14.0f * k)) {
                    indicator_font.load_from_file(dev,
                        GUI_DEFAULT_FONT_PATH, 14.0f * k);
                }
            }
            // Weapon icon font must rebake too or the strip glyphs keep
            // the previous scale (user 2026-06-09: "icon sizes don't
            // reset back" after a DPI/scale change).
            {
                const std::vector<Renderer::font::unicode_range> pua = {
                    {0xE000, 0xF8FF},
                };
                weapon_icon_font.release();
                if (weapon_icon_font.load_from_memory_with_ranges(
                        dev, demo::embedded::g_font_csgo_icons_otf,
                        demo::embedded::g_font_csgo_icons_otf_size,
                        15.0f * k, /*aliased=*/false, pua)) {
                    g_weapon_icon_font = &weapon_icon_font;
                } else {
                    g_weapon_icon_font = nullptr;
                }
                const std::vector<Renderer::font::unicode_range> abc = {
                    {0x0041, 0x0043},
                };
                player_icon_font_obj.release();
                if (player_icon_font_obj.load_from_memory_with_ranges(
                        dev, demo::embedded::g_font_player_icons_otf,
                        demo::embedded::g_font_player_icons_otf_size,
                        15.0f * k, /*aliased=*/false, abc)) {
                    GUI::player_icon_font() = &player_icon_font_obj;
                } else {
                    GUI::player_icon_font() = nullptr;
                }
            }

            const float new_w = 800.0f * k;
            const float new_h = 600.0f * k;

            // Resize each menu window — its width_/height_ are
            // independent of style::sizes() so set_gui_scale alone
            // doesn't reach them. Position is preserved.
            for (auto& win : layout.windows) {
                if (!win) continue;
                const auto b = win->bounds();
                win->set_bounds(GUI::c_base_element::rect_t{
                    b.x, b.y, new_w, new_h});
            }
        });
    // Override the tab manager's font to use the bigger tab font.
    if (layout.frame_tabs) layout.frame_tabs->set_tab_font(&tab_font);
    Renderer::c_texture menu_logo;
    Renderer::c_texture menu_logo_gray;
    Renderer::c_texture william_profile;
    Renderer::c_texture chevron_arrow;
    // Procedural V chevron — mirrors binary `g_texture_arrow` (84x48,
    // qword_140245F90). Tinted at draw time via the colour passed to
    // render_rotated_texture, so the alpha mask is what matters here.
    {
        const auto chevron_px = Renderer::generate_chevron_pixels(84, 48);
        if (chevron_arrow.create_from_argb(d3d.device, 84, 48,
                                           chevron_px.data())) {
            GUI::style::set_chevron_texture(chevron_arrow.handle());
        }
    }
    // Logo + avatar are now EMBEDDED (demo/embedded_assets.h) so the exe
    // is self-contained — no OTV5_ASSET_DIR lookup at runtime. c_texture::
    // create() takes the raw RGBA bytes directly; the blobs are the same
    // pixels the asset dir held (gen_embedded_assets.py packed them).
    menu_logo.create(d3d.device, 320, 200, demo::embedded::g_menu_logo_rgba);
    menu_logo_gray.create(d3d.device, 320, 200,
                          demo::embedded::g_menu_logo_gray_rgba);
    william_profile.create(d3d.device, 48, 48, demo::embedded::g_avatar_rgba);
    g_profile_tex = william_profile.handle();   // shared with the Spectators overlay
    for (auto& win : layout.windows) {
        win->set_logo_texture(menu_logo.handle());
        win->set_logo_gray_texture(menu_logo_gray.handle());
        win->set_profile_texture(william_profile.handle());
    }
    (void)title_font;

    LARGE_INTEGER freq{}, prev{}, now{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);
    float elapsed = 0;

    bool should_quit = false; MSG msg{};
    while (!should_quit) {
        GUI::input::current().begin_frame();
        GUI::input::cursor().reset();

        // Initialize layout context for this frame (binary architecture)
        {
            auto& in = GUI::input::current();
            GUI::core::current_layout().begin_frame(
                in.mouse_pos,
                in.mouse_down[GUI::input::mb_left],
                in.mouse_pressed[GUI::input::mb_left],
                in.mouse_released[GUI::input::mb_left]
            );
        }

        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) { should_quit = true; break; }
        }
        if (should_quit) break;
        if (g_request_unload) break;

        // Apply a pending host-window resize (WM_SIZE) before the frame:
        // ResizeBuffers + fresh RTV, then re-point the renderer and the
        // framework's screen globals at the new resolution. Without this
        // the projection/viewport stayed at the startup size, so
        // maximizing/fullscreening stretched the GUI.
        if (demo::g_resize_pending) {
            demo::g_resize_pending = false;
            if (demo::resize_swapchain(d3d, demo::g_resize_w, demo::g_resize_h)) {
                renderer.attach(d3d.device, d3d.context, d3d.swap_chain,
                                d3d.rtv, demo::g_resize_w, demo::g_resize_h);
                renderer.on_resize(demo::g_resize_w, demo::g_resize_h);
                globals::g_screen_w = demo::g_resize_w;
                globals::g_screen_h = demo::g_resize_h;
            }
        }

        // Poll the Open-GUI keybinder. Edge-triggered: every NEW press
        // flips g_menu_visible (toggle mode) or holds it (hold mode).
        {
            auto bind = GUI::keybind_state::unpack(g_menu_kb_state->get());
            if (bind.key != 0) {
                auto& in = GUI::input::current();
                static bool s_prev_menu = false;
                bool active = false;
                switch (bind.mode) {
                    case GUI::keybinder_mode_e::always: active = true;         break;
                    case GUI::keybinder_mode_e::hold:   active = in.key_down[bind.key]; break;
                    case GUI::keybinder_mode_e::toggle:
                        active = in.key_pressed[bind.key]; break;
                }
                if (bind.mode == GUI::keybinder_mode_e::toggle && active) {
                    g_menu_visible = !g_menu_visible;
                } else if (bind.mode == GUI::keybinder_mode_e::hold) {
                    if (active && !s_prev_menu) g_menu_visible = !g_menu_visible;
                    s_prev_menu = active;
                }
            }
        }

        // Poll the watermark keybinder — flips g_show_watermark each time
        // the bound key fires (always / hold / toggle modes all work).
        {
            auto bind = GUI::keybind_state::unpack(g_watermark_kb_state->get());
            if (bind.key != 0) {
                auto& in = GUI::input::current();
                static bool s_prev_active = false;
                bool active = false;
                switch (bind.mode) {
                    case GUI::keybinder_mode_e::always: active = true;         break;
                    case GUI::keybinder_mode_e::hold:   active = in.key_down[bind.key]; break;
                    case GUI::keybinder_mode_e::toggle:
                        active = in.key_pressed[bind.key]; break;
                }
                if (bind.mode == GUI::keybinder_mode_e::toggle && active) {
                    g_show_watermark = !g_show_watermark;
                } else if (bind.mode == GUI::keybinder_mode_e::hold) {
                    // Edge-triggered hold: flip on press, restore on release.
                    if (active && !s_prev_active) g_show_watermark = !g_show_watermark;
                    s_prev_active = active;
                }
            }
        }

        QueryPerformanceCounter(&now);
        const float dt = static_cast<float>(static_cast<double>(now.QuadPart - prev.QuadPart)
                                            / freq.QuadPart);
        prev = now;
        elapsed += dt;

        try {
            renderer.begin_frame();
            draw_backdrop(core, body_font, unicode_font, indicator_font,
                          width, height, elapsed);
            // Menu render & update with smooth open/close transitions
            for (auto& win : layout.windows) {
                if (!win) continue;
                win->set_open(g_menu_visible);
                win->update(dt);
                win->render(core);
            }
            renderer.end_frame();
            GUI::input::cursor().apply();
        } catch (...) {
            continue;
        }

        // FPS limiter — cap frame time to the fps_limit slider value.
        // Sleep the bulk of the remaining budget, spin the last ~1 ms
        // for accuracy (Sleep granularity is too coarse alone).
        if (const int cap = g_fps_limit_state->get(); cap > 0) {
            const double budget = 1.0 / static_cast<double>(cap);
            LARGE_INTEGER fnow;
            QueryPerformanceCounter(&fnow);
            double spent = static_cast<double>(fnow.QuadPart - now.QuadPart)
                         / static_cast<double>(freq.QuadPart);
            while (spent < budget) {
                const double rem = budget - spent;
                if (rem > 0.002)
                    ::Sleep(static_cast<DWORD>((rem - 0.001) * 1000.0));
                QueryPerformanceCounter(&fnow);
                spent = static_cast<double>(fnow.QuadPart - now.QuadPart)
                      / static_cast<double>(freq.QuadPart);
            }
        }
    }

    title_font.release();
    body_font.release();
    renderer.shutdown();
    demo::shutdown_d3d11(d3d);
    timeEndPeriod(1);
    return 0;
}
