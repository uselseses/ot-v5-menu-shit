// =============================================================================
// GUI::menu::build_cheat_menu — see cheat_menu_builder.hpp.
//
// Structure cross-referenced against `menu_layout_order.txt` decoded
// from cheat_menu_build @ sub_14007BD60 via IDA MCP. Per-label binary
// address noted next to each control so a future port (D3D9 sandbox,
// production cheat, …) can re-verify against the same IDB.
// =============================================================================

#include <GUI/menu/cheat_menu_builder.hpp>
#include <GUI/menu/c_settings.hpp>
#include <Cloud/config_system.hpp>

#include <GUI/core/element_factory.hpp>
#include <GUI/containers/c_dialog_window.hpp>
#include <GUI/containers/c_page.hpp>
#include <GUI/containers/c_page_manager.hpp>
#include <GUI/containers/c_subtab.hpp>
#include <GUI/containers/c_tab.hpp>
#include <GUI/containers/c_tab_manager.hpp>
#include <GUI/elements/c_button.hpp>
#include <GUI/elements/c_button_grid.hpp>
#include <GUI/elements/c_checkbox.hpp>
#include <GUI/elements/c_colorpicker.hpp>
#include <GUI/elements/c_column.hpp>
#include <GUI/elements/c_esp_row.hpp>
#include <GUI/elements/c_dropdown.hpp>
#include <GUI/elements/c_groupbox.hpp>
#include <GUI/elements/c_icon_tab_strip.hpp>
#include <GUI/elements/c_keybinder.hpp>
#include <GUI/elements/c_label.hpp>
#include <GUI/elements/c_listbox.hpp>
#include <GUI/elements/c_separator.hpp>
#include <GUI/elements/c_slider.hpp>
#include <GUI/elements/c_textbox_new.hpp>
#include <GUI/enums/antiaim_condition_e.hpp>
#include <GUI/pages/antiaim_groupbox_t.hpp>
#include <GUI/pages/legit_weapon_page_t.hpp>
#include <GUI/pages/rage_weapon_page_t.hpp>
#include <GUI/style/element_sizes.hpp>
#include <GUI/style/rounding.hpp>
#include <Renderer/font.hpp>
#include <Renderer/geometry.hpp>

#include <cctype>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace GUI::menu {

namespace {

// ---------------------------------------------------------------------------
// Small builder helpers used throughout the cheat tabs.
// ---------------------------------------------------------------------------

std::shared_ptr<c_column> add_columns(std::shared_ptr<c_page>& page, int count = 2)
{
    auto cols = element_factory::make<c_column>(count);
    page->add_child(cols);
    return cols;
}

struct subtab_spec {
    const char* name;
    std::function<void(std::shared_ptr<c_page>&, const Renderer::font&)> populate;
};

// Thread-local pointer to the c_settings registry for the
// currently-building menu. Set at the top of build_cheat_menu and
// cleared on exit. Path-binding helpers read through this.
thread_local c_settings* g_current_settings = nullptr;

inline c_settings& settings() {
    return g_current_settings ? *g_current_settings : default_settings();
}

// -------- Path-binding helpers (state lives in c_settings) ------------------

std::shared_ptr<c_checkbox> add_check_p(
    std::shared_ptr<c_groupbox>& gb,
    const char* path, const char* label, bool default_v,
    const Renderer::font& font)
{
    auto state = settings().bool_state(path, default_v);
    auto cb = element_factory::make<c_checkbox>(label, state);
    cb->set_font(&font); gb->add_child(cb);
    return cb;
}

std::shared_ptr<c_slider<int>> add_int_p(
    std::shared_ptr<c_groupbox>& gb,
    const char* path, const char* label, int lo, int hi, int default_v,
    const char* suffix, const Renderer::font& font)
{
    auto state = settings().int_state(path, default_v);
    auto s = element_factory::make<c_slider<int>>(label, lo, hi, state, suffix);
    s->set_font(&font); s->set_bounds({0,0,0,32});
    gb->add_child(s);
    return s;
}

std::shared_ptr<c_slider<float>> add_float_p(
    std::shared_ptr<c_groupbox>& gb,
    const char* path, const char* label, float lo, float hi, float default_v,
    const char* suffix, const Renderer::font& font)
{
    auto state = settings().float_state(path, default_v);
    auto s = element_factory::make<c_slider<float>>(label, lo, hi, state, suffix);
    s->set_font(&font); s->set_bounds({0,0,0,32});
    gb->add_child(s);
    return s;
}

std::shared_ptr<c_dropdown> add_dd_p(
    std::shared_ptr<c_groupbox>& gb,
    const char* path, const char* label, std::vector<std::string> opts,
    int default_idx, const Renderer::font& font)
{
    auto state = settings().int_state(path, default_idx);
    auto d = element_factory::make<c_dropdown>(label, std::move(opts), state);
    d->set_font(&font); d->set_bounds({0,0,0,36});
    gb->add_child(d);
    return d;
}

std::shared_ptr<c_keybinder> add_kb_p(
    std::shared_ptr<c_groupbox>& gb,
    const char* path, const char* label, const Renderer::font& font)
{
    auto state = settings().kb_state(path, 0);
    auto kb = element_factory::make<c_keybinder>(label, state);
    kb->set_font(&font); gb->add_child(kb);
    return kb;
}

std::shared_ptr<c_colorpicker> add_col_p(
    std::shared_ptr<c_groupbox>& gb,
    const char* path, const char* label, Renderer::color default_v,
    const Renderer::font& font)
{
    auto state = settings().color_state(path, default_v);
    auto cp = element_factory::make<c_colorpicker>(label, default_v, true, state);
    cp->set_font(&font); gb->add_child(cp);
    return cp;
}

// ESP-style label row — shared composite (see c_esp_row.hpp; promoted
// out of this file so the Framework showcase rows reuse the same slot
// management).

// Build an ESP row (label + checkbox + colorpicker) into `gb`.
std::shared_ptr<c_esp_row> add_esp_row(
    std::shared_ptr<c_groupbox>& gb,
    const char* path_prefix, const char* label,
    bool default_check, Renderer::color default_color,
    const Renderer::font& font)
{
    auto row = element_factory::make<c_esp_row>();
    row->set_label(label);
    row->set_font(&font);
    gb->add_child(row);

    const std::string p_chk = std::string{path_prefix} + ".enabled";
    const std::string p_col = std::string{path_prefix} + ".color";
    auto cb = element_factory::make<c_checkbox>(
        std::string{}, settings().bool_state(p_chk, default_check));
    cb->set_font(&font);
    row->set_checkbox(cb);

    auto cp = element_factory::make<c_colorpicker>(
        std::string{}, default_color, true,
        settings().color_state(p_col, default_color));
    cp->set_font(&font);
    row->set_colorpicker(cp);
    return row;
}

// Build an ESP row with an additional multi-select dropdown
// (Weapons / Flags / Style).
std::shared_ptr<c_esp_row> add_esp_row_dd(
    std::shared_ptr<c_groupbox>& gb,
    const char* path_prefix, const char* label,
    bool default_check, Renderer::color default_color,
    std::vector<std::string> dd_options, std::uint64_t default_mask,
    const Renderer::font& font, bool multi_select = true)
{
    auto row = add_esp_row(gb, path_prefix, label,
                           default_check, default_color, font);
    const std::string p_dd = std::string{path_prefix} + ".filter";
    auto dd = element_factory::make<c_dropdown>(
        std::string{}, std::move(dd_options),
        settings().int_state(p_dd, 0));
    dd->set_font(&font);
    dd->set_multi_select(multi_select);
    if (multi_select) {
        for (int i = 0; i < 64; ++i) {
            if (default_mask & (1ULL << i)) dd->toggle_multi(i);
        }
    }
    row->set_dropdown(dd);
    return row;
}

std::shared_ptr<c_tab> tab_with_subtabs(
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

void add_checkboxes(std::shared_ptr<c_groupbox>& gb,
                    std::initializer_list<const char*> names,
                    const Renderer::font& font)
{
    for (auto* n : names) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(&font); gb->add_child(cb);
    }
}

std::shared_ptr<c_slider<int>> add_slider_i(
    std::shared_ptr<c_groupbox>& gb, const char* label, int lo, int hi,
    int init, const char* suffix, const Renderer::font& font)
{
    auto s = element_factory::make<c_slider<int>>(label, lo, hi, nullptr, suffix);
    s->set_font(&font); s->set_bounds({0,0,0,32}); s->set(init);
    gb->add_child(s);
    return s;
}

std::shared_ptr<c_slider<float>> add_slider_f(
    std::shared_ptr<c_groupbox>& gb, const char* label,
    float lo, float hi, float init, const char* suffix,
    const Renderer::font& font)
{
    auto s = element_factory::make<c_slider<float>>(label, lo, hi, nullptr, suffix);
    s->set_font(&font); s->set_bounds({0,0,0,32}); s->set(init);
    gb->add_child(s);
    return s;
}

std::shared_ptr<c_dropdown> add_dropdown(
    std::shared_ptr<c_groupbox>& gb, const char* label,
    std::vector<std::string> opts, int default_idx,
    const Renderer::font& font)
{
    auto d = element_factory::make<c_dropdown>(label, std::move(opts));
    d->set_font(&font); d->set_bounds({0,0,0,36}); d->set(default_idx);
    gb->add_child(d);
    return d;
}

std::shared_ptr<c_keybinder> add_keybinder(
    std::shared_ptr<c_groupbox>& gb, const char* label,
    const Renderer::font& font)
{
    auto kb = element_factory::make<c_keybinder>(label);
    kb->set_font(&font); gb->add_child(kb);
    return kb;
}

void register_kb(const build_ctx& ctx,
                 const std::shared_ptr<c_keybinder>& kb,
                 const char* tag,
                 std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    if (ctx.register_indicator_kb) ctx.register_indicator_kb(kb, tag, r, g, b, 0xFF);
}

// ---------------------------------------------------------------------------
// Legit tab — sub_14007BD60 strings 0x14007BE30 ("Legit") + Main/Weapons
// ---------------------------------------------------------------------------

void populate_legit_main(std::shared_ptr<c_page>& page, const Renderer::font& font)
{
    // EXACT match to the otv5-hook dump (menu_Legit.json > Main), verified
    // against decompiled sub_14007BD60. Groupboxes: General / Fakelag / Antiaim
    // / Keys. "Enabled" is a reveal-master in Fakelag & Antiaim.
    auto cols = add_columns(page);

    // ---- General ------------------------------------------------------
    auto gen_gb = element_factory::make<c_groupbox>("General");
    gen_gb->set_title_font(&font); gen_gb->set_bounds({0,0,0,140});
    cols->add_child(gen_gb);
    // Defaults aligned to demo (2026-06-11): master_switch true, reaction_time 100/0-500.
    add_check_p(gen_gb, "legit.general.master_switch",        "Master switch",        true,  font);
    add_check_p(gen_gb, "legit.general.smoke_check",          "Smoke check",          false, font);
    add_check_p(gen_gb, "legit.general.ignore_steam_friends", "Ignore Steam friends", false, font);
    add_int_p  (gen_gb, "legit.general.reaction_time",        "Reaction time", 0, 500, 100, " ms", font);

    // ---- Fakelag (Enabled = reveal master) ----------------------------
    auto fl_gb = element_factory::make<c_groupbox>("Fakelag");
    fl_gb->set_title_font(&font); fl_gb->set_bounds({0,0,0,170});
    cols->add_child(fl_gb);
    auto fl_en = add_check_p(fl_gb, "legit.fakelag.enabled", "Enabled", false, font);
    // Demo (2026-06-11): individual trigger checkboxes, not a multi-dropdown.
    // Paths chosen to match the already-existing keys in config_bridge.cpp
    // (legit.fakelag.<name>) so the cheat-side feature flags fire.
    add_check_p(fl_gb, "legit.fakelag.in_air",                 "In air",                  true,  font);
    add_check_p(fl_gb, "legit.fakelag.on_peek",                "On peek",                 false, font);
    add_check_p(fl_gb, "legit.fakelag.on_shot",                "On shot",                 false, font);
    add_check_p(fl_gb, "legit.fakelag.on_land",                "On land",                 false, font);
    add_check_p(fl_gb, "legit.fakelag.while_reloading",        "While reloading",         false, font);
    add_check_p(fl_gb, "legit.fakelag.on_weapon_switch",       "On weapon switch",        false, font);
    add_check_p(fl_gb, "legit.fakelag.on_velocity_change",     "On velocity change",      false, font);
    add_check_p(fl_gb, "legit.fakelag.break_lag_compensation", "Break lag compensation",  false, font);
    // Demo default: 1-15 range, default 14.
    add_int_p  (fl_gb, "legit.fakelag.choke_limit",    "Choke limit",    1, 15, 14, " ticks", font);
    add_float_p(fl_gb, "legit.fakelag.choke_variance", "Choke variance", 0.0f, 1.0f, 0.0f, "%", font);
    fl_gb->set_reveal_master(fl_en, [fl_en]{ return fl_en->get(); });

    // ---- Antiaim (Enabled = reveal master). Demo (2026-06-11): keybinders
    // nest INSIDE the Antiaim groupbox, no separate "Keys" groupbox.
    auto aa_gb = element_factory::make<c_groupbox>("Antiaim");
    aa_gb->set_title_font(&font); aa_gb->set_bounds({0,0,0,220});
    cols->add_child(aa_gb);
    auto aa_en = add_check_p(aa_gb, "legit.antiaim.enabled", "Enabled", false, font);
    add_check_p(aa_gb, "legit.antiaim.direction_arrow",         "Direction arrow",         false, font);
    add_check_p(aa_gb, "legit.antiaim.ext_angle_inverter",      "Extended angle inverter", false, font);
    add_kb_p   (aa_gb, "legit.antiaim.inverter",                "Antiaim inverter",               font);
    add_kb_p   (aa_gb, "legit.antiaim.keys.aimbot",             "Aimbot",                         font);
    add_kb_p   (aa_gb, "legit.antiaim.keys.triggerbot",         "Triggerbot",                     font);
    // Manual anti-aim directions. c_anti_aim::manual_aa (antiaim.cpp:401)
    // reads these three binds; without controls here they had no path to
    // bind to, so manual AA was unreachable from the menu entirely.
    add_kb_p   (aa_gb, "legit.antiaim.keys.manual_left",        "Manual left",                    font);
    add_kb_p   (aa_gb, "legit.antiaim.keys.manual_right",       "Manual right",                   font);
    add_kb_p   (aa_gb, "legit.antiaim.keys.manual_back",        "Manual back",                    font);
    aa_gb->set_reveal_master(aa_en, [aa_en]{ return aa_en->get(); });
}

void populate_legit_weapons(std::shared_ptr<c_page>& page,
                            const Renderer::font& font,
                            const Renderer::font* weapon_icon_font)
{
    auto strip = std::make_shared<c_icon_tab_strip>(
        std::vector<c_icon_tab_strip::entry_t>{
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
    if (weapon_icon_font) strip->set_icon_font(weapon_icon_font);

    static const char* weapons[] = {
        "Pistol", "Heavy", "Shotgun", "SMG", "Rifle", "Scout", "AWP", "Auto",
    };
    // EXACT match to menu_Legit.json > Weapons (per-weapon page). Each weapon:
    // Aimbot(Enabled,Silent aim) / Backtracking(Enabled) /
    // Triggerbot(Enabled,Magnet,Autowall) / Recoil control(Enabled,Standalone).
    // "Enabled" is the reveal-master of each multi-control groupbox.
    for (auto* w : weapons) {
        const std::string base = std::string{"legit.weapons."} + w + ".";
        auto sub = element_factory::make<c_page>(std::string{w});
        auto cols = add_columns(sub);

        // FULLY decoded from the legit-weapon-page ctor decompile
        // (sub_140036BC0, GUI_64_DX11.exe.c:45700-47380). Default-setters
        // in the ctor tail:
        //   a1[8]  Deadzone           → 0.30   a1[10] Pitch smoothing → 0.30
        //   a1[12] Yaw smoothing      → 0.20   a1[16] Silent aim FOV  → 0.50
        //   a1[40] Pitch compensation → 0.30   a1[42] Yaw compensation→ 0.40
        auto aim_gb = element_factory::make<c_groupbox>("Aimbot");
        aim_gb->set_title_font(&font); aim_gb->set_bounds({0,0,0,360});
        cols->add_child(aim_gb);
        auto aim_en = add_check_p(aim_gb, (base + "aimbot.enabled").c_str(),    "Enabled",    false, font);
        // Options vector built at fn top: ["Head","Body","Crosshair"];
        // none selected by default (no mask-setter in the ctor tail).
        add_dd_p(aim_gb, (base + "aimbot.hitboxes").c_str(), "Hitboxes",
                 {"Head","Body","Crosshair"}, 0, font)->set_multi_select(true);
        // Target priority options are an encrypted runtime list —
        // placeholder entries until reversed.
        add_dd_p(aim_gb, (base + "aimbot.target_priority").c_str(), "Target priority",
                 {"Crosshair","Distance","Health","Cycle","Threat"}, 0, font);
        add_float_p(aim_gb, (base + "aimbot.fov").c_str(),
                    "Field of view", 0.0f, 30.0f, 0.0f, "\xC2\xB0", font);   // step 0.1
        add_float_p(aim_gb, (base + "aimbot.deadzone").c_str(),
                    "Deadzone", 0.0f, 1.0f, 0.30f, "%", font);               // step 0.01
        add_float_p(aim_gb, (base + "aimbot.pitch_smoothing").c_str(),
                    "Pitch smoothing", 0.0f, 1.0f, 0.30f, "%", font);
        add_float_p(aim_gb, (base + "aimbot.yaw_smoothing").c_str(),
                    "Yaw smoothing", 0.0f, 1.0f, 0.20f, "%", font);
        add_check_p(aim_gb, (base + "aimbot.silent_aim").c_str(), "Silent aim", false, font);
        add_float_p(aim_gb, (base + "aimbot.silent_aim_fov").c_str(),
                    "Silent aim field of view", 0.0f, 30.0f, 0.50f, "\xC2\xB0", font);
        aim_gb->set_reveal_master(aim_en, [aim_en]{ return aim_en->get(); });

        auto bt_gb = element_factory::make<c_groupbox>("Backtracking");
        bt_gb->set_title_font(&font); bt_gb->set_bounds({0,0,0,130});
        cols->add_child(bt_gb);
        auto bt_en = add_check_p(bt_gb, (base + "backtracking.enabled").c_str(), "Enabled", false, font);
        add_int_p(bt_gb, (base + "backtracking.max_time").c_str(),
                  "Maximum time", 0, 200, 0, " ms", font);
        add_int_p(bt_gb, (base + "backtracking.max_distance").c_str(),
                  "Maximum distance", 0, 64, 0, " units", font);
        bt_gb->set_reveal_master(bt_en, [bt_en]{ return bt_en->get(); });

        auto trg_gb = element_factory::make<c_groupbox>("Triggerbot");
        trg_gb->set_title_font(&font); trg_gb->set_bounds({0,0,0,180});
        cols->add_child(trg_gb);
        auto trg_en = add_check_p(trg_gb, (base + "trigger.enabled").c_str(),  "Enabled",  false, font);
        add_check_p(trg_gb,               (base + "trigger.magnet").c_str(),   "Magnet",   false, font);
        add_check_p(trg_gb,               (base + "trigger.autowall").c_str(), "Autowall", false, font);
        add_int_p(trg_gb, (base + "trigger.autowall_hitchance").c_str(),
                  "Autowall hitchance", 0, 100, 0, " %", font);
        add_int_p(trg_gb, (base + "trigger.autowall_min_damage").c_str(),
                  "Autowall minimum damage", 0, 135, 0, " damage", font);
        trg_gb->set_reveal_master(trg_en, [trg_en]{ return trg_en->get(); });

        auto rc_gb = element_factory::make<c_groupbox>("Recoil control");
        rc_gb->set_title_font(&font); rc_gb->set_bounds({0,0,0,170});
        cols->add_child(rc_gb);
        auto rc_en = add_check_p(rc_gb, (base + "recoil.enabled").c_str(),    "Enabled",    false, font);
        add_check_p(rc_gb,              (base + "recoil.standalone").c_str(), "Standalone", false, font);
        add_check_p(rc_gb, (base + "recoil.compensate_damage_taken").c_str(),
                    "Compensate for damage taken", false, font);
        add_float_p(rc_gb, (base + "recoil.pitch_compensation").c_str(),
                    "Pitch compensation", 0.0f, 1.0f, 0.30f, "%", font);
        add_float_p(rc_gb, (base + "recoil.yaw_compensation").c_str(),
                    "Yaw compensation", 0.0f, 1.0f, 0.40f, "%", font);
        rc_gb->set_reveal_master(rc_en, [rc_en]{ return rc_en->get(); });

        strip->add_child(sub);
    }

    page->add_child(strip);
}

// ---------------------------------------------------------------------------
// Rage tab — Main + Weapons
// ---------------------------------------------------------------------------

void populate_rage_main(std::shared_ptr<c_page>& page,
                        const Renderer::font& font,
                        const build_ctx& ctx)
{
    // BINARY structure (dump menu_Rage.json > Main): General / Exploits /
    // Keys / Antiaim. Column split per user 2026-06-11: LEFT = General,
    // Exploits, Keys; RIGHT = Antiaim (Move condition dropdown) + ONE
    // switching per-condition box.
    // Controls keep the config_bridge paths where the bridge defines one
    // (rage.aimbot.*, rage.exploits.mode, rage.force.kb.*) so the
    // features stay wired; binary-only controls use rage.general/keys.*.
    auto cols = add_columns(page);

    // ---- General (left) -------------------------------------------------
    auto gen_gb = element_factory::make<c_groupbox>("General");
    gen_gb->set_title_font(&font); gen_gb->set_bounds({0,0,0,200});
    cols->add_child(gen_gb);
    cols->set_child_column(gen_gb, 0);
    add_check_p(gen_gb, "rage.general.master_switch",        "Master switch",        false, font);
    add_check_p(gen_gb, "rage.aimbot.silent_aim",            "Silent aim",           true,  font);
    add_check_p(gen_gb, "rage.aimbot.team_check",            "Team check",           true,  font);
    add_check_p(gen_gb, "rage.aimbot.friendly_fire",         "Friendly fire",        false, font);
    add_check_p(gen_gb, "rage.general.ignore_steam_friends", "Ignore Steam friends", false, font);
    add_check_p(gen_gb, "rage.aimbot.dormant_aimbot",        "Dormant aimbot",       false, font);

    // ---- Exploits (left) — ONE "Exploit speed" dropdown ------------------
    auto ex_gb = element_factory::make<c_groupbox>("Exploits");
    ex_gb->set_title_font(&font); ex_gb->set_bounds({0,0,0,80});
    cols->add_child(ex_gb);
    cols->set_child_column(ex_gb, 0);
    add_dd_p(ex_gb, "rage.exploits.mode", "Exploit speed",
             {"Reliable","Fast","Unreliable"}, 0, font);

    // ---- Keys (left) — TEN keybinders in binary order --------------------
    auto keys_gb = element_factory::make<c_groupbox>("Keys");
    keys_gb->set_title_font(&font); keys_gb->set_bounds({0,0,0,340});
    cols->add_child(keys_gb);
    cols->set_child_column(keys_gb, 0);
    {
        struct { const char* lbl; const char* path; const char* tag;
                 std::uint8_t r,g,b; } kbs[] = {
            {"Hide shots",         "rage.aimbot.kb.hide_shots",         "HS",   0x4A,0xC0,0x6A},
            {"Double tap",         "rage.aimbot.kb.double_tap",         "DT",   0xFA,0xA6,0x18},
            {"Ragebot activation", "rage.aimbot.kb.ragebot_activation", "RAGE", 0xE0,0x4A,0x4A},
            {"Resolver override",  "rage.keys.resolver_override",       "RES",  0xD0,0x7A,0xE0},
            {"Force on shot",      "rage.force.kb.force_on_shot",       "OS",   0xFA,0x70,0x18},
            {"Force body aim",     "rage.force.kb.force_body_aim",      "BAIM", 0xE0,0x4A,0x4A},
            {"Force safe point",   "rage.force.kb.force_safe_point",    "SAFE", 0x6A,0xB0,0xE0},
            {"Damage override",    "rage.keys.damage_override",         "DMG",  0xFA,0xC8,0x18},
            {"Hitbox override",    "rage.keys.hitbox_override",         "HB",   0xD2,0xD2,0xD2},
            {"Autowall override",  "rage.keys.autowall_override",       "AW",   0x4A,0xC0,0x6A},
        };
        for (auto& e : kbs) {
            auto kb = add_kb_p(keys_gb, e.path, e.lbl, font);
            register_kb(ctx, kb, e.tag, e.r, e.g, e.b);
        }
    }

    // ---- Antiaim (right) — holds the Move condition dropdown -------------
    auto aa_gb = element_factory::make<c_groupbox>("Antiaim");
    aa_gb->set_title_font(&font); aa_gb->set_bounds({0,0,0,80});
    cols->add_child(aa_gb);
    cols->set_child_column(aa_gb, 1);
    auto cond = element_factory::make<c_dropdown>("Move condition",
        std::vector<std::string>{
            "Global","Stand","Move","Duck","Air","Slow walk","Fakeduck",
            "Auto direction","Manual",
        });
    cond->set_font(&font); cond->set_bounds({0,0,0,36});
    {
        auto st = settings().int_state("rage.move_condition", 0);
        cond->set(st->get());
        std::weak_ptr<c_dropdown> wd = cond;
        cond->add_on_change([wd, st]{
            if (auto p = wd.lock()) st->set(p->get());
        });
    }
    aa_gb->add_child(cond);

    // ---- ONE switching per-condition box, right column under Antiaim ----
    const std::pair<const char*, antiaim_condition_e> conds[] = {
        {"Global",         antiaim_condition_e::global},
        {"Stand",          antiaim_condition_e::stand},
        {"Move",           antiaim_condition_e::move},
        {"Duck",           antiaim_condition_e::duck},
        {"Air",            antiaim_condition_e::air},
        {"Slow walk",      antiaim_condition_e::slow_walk},
        {"Fakeduck",       antiaim_condition_e::fakeduck},
        {"Auto direction", antiaim_condition_e::auto_direction},
        {"Manual",         antiaim_condition_e::manual},
    };
    for (auto& [label, ce] : conds) {
        auto box = element_factory::make<antiaim_groupbox_t>(
            label, page, cond, ce, &font);
        cols->add_child(box);
        cols->set_child_column(box, 1);
    }
}

void populate_rage_weapons(std::shared_ptr<c_page>& page,
                           const Renderer::font& font,
                           const Renderer::font* weapon_icon_font)
{
    auto strip = std::make_shared<c_icon_tab_strip>(
        std::vector<c_icon_tab_strip::entry_t>{
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
    if (weapon_icon_font) strip->set_icon_font(weapon_icon_font);

    static const char* weapons[] = {
        "Pistol","Heavy","Shotgun","SMG","Rifle","Scout","AWP","Auto",
    };
    int weapon_id = 0;
    for (auto* w : weapons) {
        auto wp = std::make_shared<rage_weapon_page_t>(
            nullptr, w, "", weapon_id++, &font);
        strip->add_child(wp);
    }
    page->add_child(strip);
}

// ---------------------------------------------------------------------------
// Visuals tab — three subtabs (Players / World / View) per binary tree
// ---------------------------------------------------------------------------

// Groupbox visible only while a dropdown sits on a given option — the
// binary's "one box that switches with the selection" pattern (chams
// "Configuring model", same idea as the rage Move-condition boxes).
class dd_gated_groupbox_t : public c_groupbox {
public:
    dd_gated_groupbox_t(std::string label,
                        std::shared_ptr<c_dropdown> dd, int index)
        : c_groupbox{std::move(label)}, dd_{std::move(dd)}, index_{index} {}
    bool visible() const noexcept override {
        if (!dd_) return c_groupbox::visible();
        return dd_->get() == index_;
    }
private:
    std::shared_ptr<c_dropdown> dd_;
    int index_;
};

void populate_visuals_players(std::shared_ptr<c_page>& page,
                              const Renderer::font& font)
{
    auto cols = add_columns(page);

    (void)cols;  // overridden by the icon strip below.

    // User 2026-06-02 (tabs.txt spec): Visuals → Players has a top
    // icon strip for Enemy / Team / Self. Each strip page carries:
    //   * ESP groupbox — one checkbox+colorpicker ROW per feature.
    //     Weapons + Flags additionally carry a multi-select dropdown.
    //   * Chams groupbox — Configuring model dropdown.
    //   * Visible/Hidden style dropdowns + colorpickers.
    auto strip = std::make_shared<c_icon_tab_strip>(
        std::vector<c_icon_tab_strip::entry_t>{
            {"Enemy", tab_icon_e::enemy},
            {"Team",  tab_icon_e::team},
            {"Self",  tab_icon_e::self},
        });
    strip->set_font(&font);

    const struct { const char* tag; const char* path_prefix; } targets[3] = {
        {"Enemy", "visuals.players.enemy"},
        {"Team",  "visuals.players.team"},
        {"Self",  "visuals.players.self"},
    };
    for (auto& t : targets) {
        auto sub = element_factory::make<c_page>(t.tag);
        auto sc  = add_columns(sub);
        const std::string base = std::string{t.path_prefix} + ".";
        auto p = [&](const char* leaf) { return base + leaf; };

        // ---- ESP groupbox — feature rows --------------------------
        // Each row is a c_label with a c_checkbox + c_colorpicker
        // child (Weapons + Flags also get a multi-select dropdown).
        // Matches the binary's sub_140035350 + sub_140035700 factory
        // pair stitched onto a single row in sub_1400318A0.
        auto esp_gb = element_factory::make<c_groupbox>("ESP");
        esp_gb->set_title_font(&font); esp_gb->set_bounds({0,0,0,260});
        sc->add_child(esp_gb);
        // Swatch colors sampled from the original capture (og-vis2.png):
        // Ammo/Flags blue, Glow purple, Health teal-green, rest white.
        // All checkboxes default OFF and the Weapons/Flags dropdowns
        // default EMPTY ("-") exactly like the original.
        const Renderer::color k_white  {0xFF,0xFF,0xFF,0xFF};
        const Renderer::color k_blue   {0x18,0x6C,0xFA,0xFF};
        const Renderer::color k_purple {0xA0,0x58,0x97,0xFF};
        const Renderer::color k_green  {0x18,0xFA,0xA6,0xFF};

        // Everything OFF by default (user: "make esp off by default"). The
        // user turns on the boxes/elements they want per target.
        add_esp_row(esp_gb, p("esp.ammo").c_str(),     "Ammo",         false, k_blue,   font);
        add_esp_row(esp_gb, p("esp.box").c_str(),      "Bounding box", false, k_white,  font);
        add_esp_row(esp_gb, p("esp.glow").c_str(),     "Glow",         false, k_purple, font);
        add_esp_row(esp_gb, p("esp.health").c_str(),   "Health",       false, k_green,  font);
        add_esp_row(esp_gb, p("esp.name").c_str(),     "Name",         false, k_white,  font);
        add_esp_row(esp_gb, p("esp.skeleton").c_str(), "Skeleton",     false, k_white,  font);
        add_esp_row_dd(esp_gb, p("esp.weapons").c_str(), "Weapons",
                       false, k_white,
                       {"Icon","Text","Ammo"}, 0ULL, font);
        add_esp_row_dd(esp_gb, p("esp.flags").c_str(), "Flags",
                       false, k_blue,
                       {"Lag compensation","Money","Armor","Flashed",
                        "Reloading","Scoped","Defusing","Location",
                        "Objective carrier","Steam friend","Fake duck",
                        "Stomach damage","Firing","Latency","Exploiting",
                        "Dormant","Icons","Inventory"}, 0ULL, font);

        // ---- Chams — matches the original capture (og-vis2.png):
        // gb "Chams" holds a "Configuring model" label + dropdown whose
        // options are the chams TARGETS (strings 0x14007DEEF..0x14007E0DA:
        // Visible, Hidden, Attachments, History, Desync, Arms, Weapon).
        // The selected target then gets its OWN groupbox below (titled by
        // the selection) with a "Style" row (colorpicker on the same
        // line) and an unlabeled style dropdown — only one is visible at
        // a time, switching with the dropdown.
        auto chams_gb = element_factory::make<c_groupbox>("Chams");
        chams_gb->set_title_font(&font); chams_gb->set_bounds({0,0,0,110});
        sc->add_child(chams_gb);
        // Non-inline dropdowns draw their own label row tight above the
        // box — give the dd the label instead of a separate c_label
        // (which doubled the spacing).
        auto cm_dd = element_factory::make<c_dropdown>(
            std::string{"Configuring model"},
            std::vector<std::string>{
                "Visible","Hidden","Attachments","History",
                "Desync","Arms","Weapon",
            },
            settings().int_state(p("chams.configuring_model"), 0));
        cm_dd->set_font(&font); cm_dd->set_bounds({0,0,0,36});
        chams_gb->add_child(cm_dd);

        {
            static constexpr const char* k_targets[] = {
                "Visible","Hidden","Attachments","History",
                "Desync","Arms","Weapon",
            };
            int idx = 0;
            for (auto* tgt : k_targets) {
                auto tg = element_factory::make<dd_gated_groupbox_t>(
                    tgt, cm_dd, idx++);
                tg->set_title_font(&font); tg->set_bounds({0,0,0,110});
                // "Style" + colorpicker on ONE row (original: swatch
                // right-aligned on the Style line), then the unlabeled
                // style dropdown ("Custom" default per capture).
                auto row = element_factory::make<c_esp_row>();
                row->set_label("Style");
                row->set_font(&font);
                tg->add_child(row);
                auto cp = element_factory::make<c_colorpicker>(
                    std::string{}, style::current().accent, true,
                    settings().color_state(
                        base + "chams." + tgt + ".color",
                        style::current().accent));
                cp->set_font(&font);
                row->set_colorpicker(cp);
                auto sdd = element_factory::make<c_dropdown>(std::string{""},
                    std::vector<std::string>{
                        "Custom","Flat","Pulse","Wireframe","Glow","Glow (2)",
                    },
                    settings().int_state(base + "chams." + tgt + ".style", 0));
                sdd->set_font(&font);
                // Attach to the Style row — renders directly beneath it
                // (a free-standing dd reserves an empty label row).
                row->set_dropdown(sdd);
                sc->add_child(tg);
                sc->set_child_column(tg, 1);   // stack under Chams (right)
            }
        }

        strip->add_child(sub);
    }
    page->add_child(strip);
}

void populate_visuals_world(std::shared_ptr<c_page>& page,
                            const Renderer::font& font)
{
    // BINARY (dump menu_Visuals.json > World + strings 0x14007E31D..):
    // exactly TWO groupboxes — Entities and World.
    auto cols = add_columns(page);

    auto ent = element_factory::make<c_groupbox>("Entities");
    ent->set_title_font(&font); ent->set_bounds({0,0,0,260});
    cols->add_child(ent);
    add_check_p(ent, "visuals.world.entities.bomb",                "Bomb",                false, font);
    add_check_p(ent, "visuals.world.entities.hostage",             "Hostage",             false, font);
    add_check_p(ent, "visuals.world.entities.grenades",            "Grenades",            false, font);
    add_check_p(ent, "visuals.world.entities.grenade_prediction",  "Grenade prediction",  false, font);
    // Floats 0..1 step 0.01 per dump.
    add_float_p(ent, "visuals.world.entities.ragdoll_gravity", "Ragdoll gravity", 0.0f, 1.0f, 0.0f, "", font);
    add_float_p(ent, "visuals.world.entities.ragdoll_force",   "Ragdoll force",   0.0f, 1.0f, 0.0f, "", font);

    auto wd = element_factory::make<c_groupbox>("World");
    wd->set_title_font(&font); wd->set_bounds({0,0,0,300});
    cols->add_child(wd);
    // "Weather" string (0x14007E5EC) precedes Skybox in the builder; its
    // creator is encrypted (dump missed it) — options placeholder.
    add_dd_p(wd, "visuals.world.weather", "Weather",
             {"Off","Rain","Snow"}, 0, font);
    // Skybox option list is a runtime table (dump captured the dropdown
    // with no options) — placeholder until reversed.
    add_dd_p(wd, "visuals.world.skybox", "Skybox",
             {"Default","Baggage","Daylight","Dusk","Night","Tibet","Vertigo"},
             0, font);
    // Ambient light — checkbox + colorpicker on the SAME row.
    add_esp_row(wd, "visuals.world.ambient_light", "Ambient light",
                true,  Renderer::color{0xFF,0xFF,0xFF,0xFF}, font);
    add_check_p(wd, "visuals.world.fullbright", "Fullbright", false, font);
    add_float_p(wd, "visuals.world.nightmode",  "Nightmode",         0.0f, 1.0f, 0.0f, "", font);
    add_float_p(wd, "visuals.world.wall_trans", "Wall transparency", 0.0f, 1.0f, 0.0f, "", font);
    add_float_p(wd, "visuals.world.prop_trans", "Prop transparency", 0.0f, 1.0f, 0.0f, "", font);
}

void populate_visuals_extra(std::shared_ptr<c_page>& page,
                            const Renderer::font& font)
{
    // Layout per the original (user 2026-06-11): LEFT column = Extra,
    // Crosshair; RIGHT column = Removals, Custom model properties.
    // Children alternate columns by index → order E, R, C, CMP.
    auto cols = add_columns(page);

    auto ex = element_factory::make<c_groupbox>("Extra");
    ex->set_title_font(&font); ex->set_bounds({0,0,0,460});
    cols->add_child(ex);

    auto rm = element_factory::make<c_groupbox>("Removals");
    rm->set_title_font(&font); rm->set_bounds({0,0,0,400});
    cols->add_child(rm);
    add_check_p(ex, "visuals.extra.shared_esp",         "Shared ESP",              false, font);
    add_check_p(ex, "visuals.extra.visible_only_esp",   "Visible only ESP",        false, font);
    add_check_p(ex, "visuals.extra.radar_reveal",       "Radar reveal",            false, font);
    add_check_p(ex, "visuals.extra.flat_arrows",        "Flat arrows",             false, font);
    add_check_p(ex, "visuals.extra.footsteps",          "Footsteps",               false, font);
    add_check_p(ex, "visuals.extra.hitmarker",          "Hitmarker",               false, font);
    add_check_p(ex, "visuals.extra.weapon_recoil",      "Weapon recoil",           false, font);
    add_check_p(ex, "visuals.extra.weapon_spread",      "Weapon spread",           false, font);
    // Trailing space is in the binary string ('Taser range ' 0x14007EB62).
    add_check_p(ex, "visuals.extra.taser_range",        "Taser range ",            false, font);
    add_check_p(ex, "visuals.extra.knife_range",        "Knife range",             false, font);
    // Dormant ESP — checkbox + colorpicker on the SAME row (the dump's
    // unlabeled colorpicker right after it).
    add_esp_row(ex, "visuals.extra.dormant_esp", "Dormant ESP",
                true,  Renderer::color{0xFF,0xFF,0xFF,0xFF}, font);
    add_check_p(ex, "visuals.extra.bullet_imp_client",  "Bullet impacts (client)", true,  font);
    add_check_p(ex, "visuals.extra.bullet_imp_server",  "Bullet impacts (server)", true,  font);
    add_check_p(ex, "visuals.extra.bullet_tracers",     "Bullet tracers",          true,  font);
    add_check_p(ex, "visuals.extra.oob_arrows",         "Out of bounds arrows",    true,  font);
    add_check_p(ex, "visuals.extra.health_based",       "Health based",            false, font);

    auto ch = element_factory::make<c_groupbox>("Crosshair");
    ch->set_title_font(&font); ch->set_bounds({0,0,0,140});
    cols->add_child(ch);
    add_check_p(ch, "visuals.crosshair.force",        "Force crosshair",    false, font);
    add_check_p(ch, "visuals.crosshair.autowall",     "Autowall crosshair", false, font);
    add_check_p(ch, "visuals.crosshair.autowall_dot", "Autowall dot",       false, font);

    add_check_p(rm, "visuals.rm.post_processing",      "Disable post processing",     false, font);
    add_check_p(rm, "visuals.rm.fog",                  "Disable fog",                 false, font);
    add_check_p(rm, "visuals.rm.cascade_shadows",      "Disable cascade shadows",     false, font);
    add_check_p(rm, "visuals.rm.blood_splatter",       "Disable blood splatter",      false, font);
    add_check_p(rm, "visuals.rm.teammate_rendering",   "Disable teammate rendering",  false, font);
    add_check_p(rm, "visuals.rm.no_flash",             "No flash",                    false, font);
    add_check_p(rm, "visuals.rm.no_smoke",             "No smoke",                    false, font);
    add_check_p(rm, "visuals.rm.no_scope",             "No scope",                    false, font);
    add_check_p(rm, "visuals.rm.no_recoil",            "No recoil",                   false, font);
    add_check_p(rm, "visuals.rm.no_visual_kick",       "No visual kick",              false, font);
    add_check_p(rm, "visuals.rm.no_visual_punch",      "No visual punch",             false, font);
    add_check_p(rm, "visuals.rm.no_landing_bob",       "No landing bob",              false, font);
    add_check_p(rm, "visuals.rm.no_fakeduck_bob",      "No fakeduck bob",             false, font);
    add_check_p(rm, "visuals.rm.custom_model_props",   "Custom model properties",     true,  font);

    // Custom model properties — its OWN groupbox on the right column
    // (strings 0x14007F3DF..0x14007F572: Pearlescent / Phong /
    // Reflectivity, floats 0..1 with "%").
    auto cm = element_factory::make<c_groupbox>("Custom model properties");
    cm->set_title_font(&font); cm->set_bounds({0,0,0,160});
    cols->add_child(cm);
    add_float_p(cm, "visuals.cm.pearlescent",  "Pearlescent",  0.0f, 1.0f, 0.0f, "%", font);
    add_float_p(cm, "visuals.cm.phong",        "Phong",        0.0f, 1.0f, 0.0f, "%", font);
    add_float_p(cm, "visuals.cm.reflectivity", "Reflectivity", 0.0f, 1.0f, 0.0f, "%", font);
}

// ---------------------------------------------------------------------------
// Miscellaneous tab — View / Helpers / Movement / Skins subtabs per the
// live dump (menu_Miscellaneous.json) and the binary builder decompile
// (GUI_64_DX11.exe.c:97120-97710). Content, order, ranges and suffixes
// follow the binary exactly.
// ---------------------------------------------------------------------------

void populate_misc_view(std::shared_ptr<c_page>& page,
                        const Renderer::font& font)
{
    auto cols = add_columns(page);

    // View groupbox — 5 checkboxes + Visual interpolation + 4 suffixed
    // sliders (°/°/ units/%) per the decompile.
    auto vw = element_factory::make<c_groupbox>("View");
    vw->set_title_font(&font); vw->set_bounds({0,0,0,360});
    cols->add_child(vw);
    // Thirdperson: checkbox + keybinder on one row (same shape as Auto peek
    // / Edge jump in the Movement page). The bridge previously read
    // "misc.view.thirdperson" but no control ever created that path, so
    // thirdperson could be neither enabled nor bound from this menu.
    {
        auto row = element_factory::make<c_esp_row>();
        row->set_label("Thirdperson");
        row->set_font(&font);
        vw->add_child(row);
        auto cb = element_factory::make<c_checkbox>(
            std::string{}, settings().bool_state("misc.view.thirdperson", false));
        cb->set_font(&font);
        row->set_checkbox(cb);
        auto kb = element_factory::make<c_keybinder>(
            std::string{}, settings().kb_state("misc.view.kb.thirdperson"));
        kb->set_font(&font);
        row->set_keybinder(kb);
    }
    add_check_p(vw, "misc.view.thirdperson_spectate",  "Thirdperson while spectating",      false, font);
    add_check_p(vw, "misc.view.no_thirdperson_grenade","Disable thirdperson with grenades", false, font);
    add_check_p(vw, "misc.view.freecam",               "Freecam",                           false, font);
    add_check_p(vw, "misc.view.flip_knife_hand",       "Flip knife hand",                   false, font);
    add_check_p(vw, "misc.view.visualize_silent",      "Visualize silent angles",           false, font);
    // Demo (2026-06-11): Visual interpolation is a checkbox, not a slider.
    add_check_p(vw, "misc.view.visual_interpolation",  "Visual interpolation", false, font);
    add_int_p  (vw, "misc.view.fov",        "Field of view",        0, 150, 0, "°",   font);
    add_int_p  (vw, "misc.view.scoped_fov", "Scoped field of view", 0, 150, 0, "°",   font);
    add_int_p  (vw, "misc.view.tp_distance","Thirdperson distance", 0, 150, 0, " units",   font);
    add_float_p(vw, "misc.view.zoom_factor","Zoom factor", 0.0f, 1.0f, 0.0f, "%", font);

    // Viewmodel groupbox — five float sliders, all -3..3 step 0.1.
    auto vm = element_factory::make<c_groupbox>("Viewmodel");
    vm->set_title_font(&font); vm->set_bounds({0,0,0,220});
    cols->add_child(vm);
    // Demo (2026-06-11): viewmodel offsets/roll/fov are -10..10, not -3..3.
    add_float_p(vm, "misc.viewmodel.fov",      "Field of view", -10.0f, 10.0f, 0.0f, "", font);
    add_float_p(vm, "misc.viewmodel.x_offset", "X offset",      -10.0f, 10.0f, 0.0f, "", font);
    add_float_p(vm, "misc.viewmodel.y_offset", "Y offset",      -10.0f, 10.0f, 0.0f, "", font);
    add_float_p(vm, "misc.viewmodel.z_offset", "Z offset",      -10.0f, 10.0f, 0.0f, "", font);
    add_float_p(vm, "misc.viewmodel.roll",     "Roll",          -10.0f, 10.0f, 0.0f, "", font);
}

void populate_misc_helpers(std::shared_ptr<c_page>& page,
                           const Renderer::font& font)
{
    auto cols = add_columns(page);

    auto hp = element_factory::make<c_groupbox>("Helpers");
    hp->set_title_font(&font); hp->set_bounds({0,0,0,560});
    cols->add_child(hp);
    add_check_p(hp, "misc.helpers.show_watermark", "Show watermark", true,  font);
    // Watermark field selection — multi dropdown; option strings verbatim
    // from the binary (incl. the literal "@TODO: add remaining flags").
    {
        auto wm = add_dd_p(hp, "misc.helpers.watermark_fields", "",
                {"@TODO: add remaining flags","Build name","Frames per second",
                 "Latency to server","System time","Server map","Server type",
                 "Server tickrate","Username"}, 0, font);
        wm->set_multi_select(true);
        wm->toggle_multi(1);   // Build name
        wm->toggle_multi(2);   // Frames per second
    }
    add_check_p(hp, "misc.helpers.show_keybind_states",    "Show keybind states",        true,  font);
    add_check_p(hp, "misc.helpers.spectators",             "Spectators",                 true,  font);
    add_check_p(hp, "misc.helpers.extended_backtracking",  "Extended backtracking",      false, font);
    add_check_p(hp, "misc.helpers.auto_bomb_defuse",       "Auto bomb defuse",           false, font);
    add_check_p(hp, "misc.helpers.aim_at_bomb_defusing",   "Aim at bomb while defusing", false, font);
    add_check_p(hp, "misc.helpers.auto_match_accept",      "Auto match accept",          false, font);
    add_check_p(hp, "misc.helpers.preserve_killfeed",      "Preserve killfeed",          false, font);
    add_check_p(hp, "misc.helpers.reveal_hidden_cvars",    "Reveal hidden cvars",        false, font);
    add_check_p(hp, "misc.helpers.reveal_mm_ranks",        "Reveal matchmaking ranks",   false, font);
    add_check_p(hp, "misc.helpers.bypass_inventory_lock",  "Bypass inventory lock",      false, font);
    add_check_p(hp, "misc.helpers.bypass_sv_pure",         "Bypass sv_pure",             false, font);
    add_check_p(hp, "misc.helpers.force_sv_cheats",        "Force sv_cheats",            false, font);
    add_check_p(hp, "misc.helpers.auto_fire",              "Auto fire",                  false, font);
    add_int_p  (hp, "misc.helpers.auto_fire_delay",        "Auto fire delay", 0, 200, 0, "", font);
    // Server region — full 46-entry list captured live from the binary
    // (dump menu_Miscellaneous.json).
    add_dd_p(hp, "misc.helpers.server_region", "Server region",
            {"Amsterdam","Atlanta","Buenos Aires","Chennai","Chicago","Dallas",
             "Dubai","Frankfurt","Hong Kong","Johannesburg","Lima","London",
             "Los Angeles","Madrid","Moses Lake","Mumbai","PW Guangdong 1",
             "PW Guangdong 2","PW Guangdong 2 (Mobile)","PW Guangdong 2 (Telecom)",
             "PW Guangdong 2 (Unicom)","PW Hebei","PW Shanghai",
             "PW Shanghai (Mobile)","PW Shanghai (Telecom)","PW Shanghai (Unicom)",
             "PW Tianjin","PW Tianjin (Mobile)","PW Tianjin (Telecom)",
             "PW Tianjin (Unicom)","PW Wuhan","PW Zhejiang","Paris","Santiago",
             "Sao Paulo","Seattle","Seoul","Shanghai (sha-4) Backbone",
             "Singapore","Sterling","Stockholm (Bromma)","Stockholm (Kista)",
             "Sydney","Tokyo (North)","Tokyo (South)","Vienna","Warsaw"},
            0, font);
    // Clantag changer is a DROPDOWN in the binary (Custom/Onetap/Backwards),
    // followed by the Custom clantag textbox.
    // "None" is index 0 so the control has an off state — the bridge used to
    // read enable from the bool map (which this int dropdown never writes),
    // leaving the clantag permanently disabled with no way to turn it on.
    add_dd_p(hp, "misc.helpers.clantag_changer", "Clantag changer",
            {"None","Custom","Onetap","Backwards"}, 0, font);
    {
        auto tb = element_factory::make<c_textbox_new>("Custom clantag ", "");
        tb->set_font(&font); hp->add_child(tb);
    }

    auto bb = element_factory::make<c_groupbox>("Buybot");
    bb->set_title_font(&font); bb->set_bounds({0,0,0,200});
    cols->add_child(bb);
    cols->set_child_column(bb, 1);    // Helpers left; Buybot + Sounds right
    add_check_p(bb, "misc.buybot.enable", "Enable", false, font);
    // Option lists are runtime weapon tables in the binary (not captured
    // by the dump) — placeholder lists kept until reversed.
    add_dd_p(bb, "misc.buybot.primary",   "Primary",
            {"None","AK-47","M4A4","M4A1-S","AWP"}, 0, font);
    add_dd_p(bb, "misc.buybot.secondary", "Secondary",
            {"None","USP-S","P2000","Glock","Deagle"}, 0, font);
    {
        auto ut = add_dd_p(bb, "misc.buybot.utility", "Utility",
                {"None","HE","Flashbang","Smoke","Molotov"}, 0, font);
        ut->set_multi_select(true);
        ut->toggle_multi(0);   // "None" — no empty "-"
    }

    auto snd = element_factory::make<c_groupbox>("Sounds");
    snd->set_title_font(&font); snd->set_bounds({0,0,0,200});
    cols->add_child(snd);
    cols->set_child_column(snd, 1);
    add_check_p(snd, "misc.sounds.on_kill_mic_spam", "On kill mic spam", false, font);
    add_check_p(snd, "misc.sounds.hit",              "Hit sound",        false, font);
    add_check_p(snd, "misc.sounds.kill",             "Kill sound",       false, font);
    // Volumes are FLOAT 0..1 step 0.01 with "%" suffix in the binary.
    // Demo (2026-06-11): volumes are int 0..100 %, not float 0..1.
    add_int_p(snd, "misc.sounds.weapon_volume",   "Weapon volume",     0, 100, 100, " %", font);
    add_int_p(snd, "misc.sounds.footstep_volume", "Footstep volume",   0, 100, 100, " %", font);
    add_int_p(snd, "misc.sounds.other_volume",    "Other sound volume",0, 100, 100, " %", font);
}

void populate_misc_movement(std::shared_ptr<c_page>& page,
                            const Renderer::font& font,
                            const build_ctx& ctx)
{
    auto cols = add_columns(page);

    // Binary order: Leg movement, Auto peek (+keybinder), Edge jump
    // (+keybinder), then the plain checkboxes.
    auto mv = element_factory::make<c_groupbox>("Movement");
    mv->set_title_font(&font); mv->set_bounds({0,0,0,420});
    cols->add_child(mv);
    add_check_p(mv, "misc.movement.leg_movement", "Leg movement", true, font);
    // Auto peek / Edge jump: checkbox + keybinder chip on the SAME row
    // (binary: label-row + checkbox + keybinder attachments).
    auto add_kb_row = [&](const char* lbl, const char* cb_path,
                          const char* kb_path, const char* tag,
                          std::uint8_t r, std::uint8_t g, std::uint8_t b) {
        auto row = element_factory::make<c_esp_row>();
        row->set_label(lbl);
        row->set_font(&font);
        mv->add_child(row);
        auto cb = element_factory::make<c_checkbox>(
            std::string{}, settings().bool_state(cb_path, true));  // dump: ON
        cb->set_font(&font);
        row->set_checkbox(cb);
        auto kb = element_factory::make<c_keybinder>(
            std::string{}, settings().kb_state(kb_path));
        kb->set_font(&font);
        row->set_keybinder(kb);
        register_kb(ctx, kb, tag, r, g, b);
    };
    add_kb_row("Auto peek", "misc.movement.auto_peek",
               "misc.movement.kb.auto_peek", "PEEK", 0xFA, 0xA6, 0x18);
    add_kb_row("Edge jump", "misc.movement.edge_jump",
               "misc.movement.kb.edge_jump", "EJ",   0x6A, 0xB0, 0xE0);
    // Fake duck / Slow walk had NO control anywhere in the menu — they
    // existed only as anti-aim conditions and ESP flags — so the binds
    // c_anti_aim::slow_walk and CMovement's fakeduck path read
    // ("misc.movement.kb.fakeduck" / ".kb.slow_walk") could never be set
    // and both features were unreachable. Same row shape as the two above.
    add_kb_row("Fake duck", "misc.movement.fakeduck",
               "misc.movement.kb.fakeduck",  "FD",   0xE0, 0x4A, 0x4A);
    add_kb_row("Slow walk", "misc.movement.slow_walk",
               "misc.movement.kb.slow_walk", "SW",   0x4A, 0xC0, 0x6A);
    add_check_p(mv, "misc.movement.accurate_walk",     "Accurate walk",     false, font);
    add_check_p(mv, "misc.movement.auto_bunnyhop",     "Auto bunnyhop",     false, font);
    add_check_p(mv, "misc.movement.fast_crouch",       "Fast crouch",       false, font);
    add_check_p(mv, "misc.movement.fast_stop",         "Fast stop",         false, font);
    add_check_p(mv, "misc.movement.crouch_in_air",     "Crouch in air",     false, font);
    add_check_p(mv, "misc.movement.strafe_assistance", "Strafe assistance", false, font);
    add_check_p(mv, "misc.movement.auto_strafe",       "Auto strafe",       false, font);
}

void populate_misc_skins(std::shared_ptr<c_page>& page,
                         const Renderer::font& font)
{
    auto cols = add_columns(page);

    auto md = element_factory::make<c_groupbox>("Models");
    md->set_title_font(&font); md->set_bounds({0,0,0,220});
    cols->add_child(md);
    add_check_p(md, "misc.models.force_rare_animations", "Force rare animations", false, font);
    // Knife/Glove option lists are runtime item tables in the binary (not
    // captured by the dump) — placeholders until reversed. Defaults idx 2.
    add_dd_p(md, "misc.models.knife", "Knife model",
            {"Default","Karambit","Bayonet","Butterfly","M9 Bayonet"}, 0, font);
    add_dd_p(md, "misc.models.glove", "Glove model",
            {"Default","Sport","Specialist","Driver","Hand wraps"}, 0, font);
    // Player model — full 37-entry list captured live from the binary.
    add_dd_p(md, "misc.models.player", "Player model",
            {"'Blueberries' Buckshot | NSWC SEAL",
             "'Two Times' McCoy | TACP Cavalry",
             "Cmdr. Mae 'Dead Cold' Jamison | SWAT",
             "1st Lieutenant Farlow | SWAT",
             "John 'Van Healen' Kask | SWAT",
             "Bio-Haz Specialist | SWAT",
             "Sergeant Bombson | SWAT",
             "Chem-Haz Specialist | SWAT",
             "Rezan the Redshirt | Sabre",
             "Lieutenant 'Tree Hugger' Farlow | SWAT",
             "Ground Rebel  | Elite Crew",
             "Osiris | Elite Crew",
             "Prof. Shahmat | Elite Crew",
             "The Elite Mr. Muhlik | Elite Crew",
             "Jungle Rebel | Elite Crew",
             "Soldier | Phoenix",
             "Enforcer | Phoenix",
             "Slingshot | Phoenix",
             "Street Soldier | Phoenix",
             "Operator | FBI SWAT",
             "Markus Delrow | FBI HRT",
             "Michael Syfers  | FBI Sniper",
             "Special Agent Ava | FBI",
             "3rd Commando Company | KSK",
             "Seal Team 6 Soldier | NSWC SEAL",
             "Buckshot | NSWC SEAL",
             "'Two Times' McCoy | USAF TACP",
             "Lt. Commander Ricksaw | NSWC SEAL",
             "Primeiro Tenente | Brazilian 1st Battalion",
             "Dragomir | Sabre",
             "Maximus | Sabre",
             "Rezan The Ready | Sabre",
             "Blackwolf | Sabre",
             "'The Doctor' Romanov | Sabre",
             "Dragomir | Sabre Footsoldier",
             "B Squadron Officer | SAS",
             "D Squadron Officer | NZSAS"},
            2, font);

    // Skins groupbox — Weapon dropdown, Wear 0..1, Seed 0..500,
    // StatTrak SLIDER 0..9999, current-weapon filter, paintkit list.
    auto sk = element_factory::make<c_groupbox>("Skins");
    sk->set_title_font(&font); sk->set_bounds({0,0,0,360});
    cols->add_child(sk);
    add_dd_p(sk, "misc.skins.weapon", "Weapon",
            {"Knife","Glock-18","USP-S","Deagle","AK-47","M4A4","AWP"}, 0, font);
    add_float_p(sk, "misc.skins.wear", "Wear", 0.0f, 1.0f, 0.0f, "", font);
    add_int_p  (sk, "misc.skins.seed", "Seed", 0, 500,  0, "", font);
    add_int_p  (sk, "misc.skins.stattrak", "StatTrak", 0, 9999, 0, "", font);
    add_check_p(sk, "misc.skins.current_weapon_only", "Show for current weapon only", false, font);
    {
        auto lb = element_factory::make<c_listbox>(
            "Paintkit list",
            std::vector<std::string>{
                "Vanilla","Asiimov","Fade","Dragon Lore","Howl","Crimson Web",
            }, 5, true);
        lb->set_font(&font); sk->add_child(lb);
    }
}

// ---------------------------------------------------------------------------
// Settings tab — Cheat / GUI / Cloud / Debug subtabs
// ---------------------------------------------------------------------------

void populate_settings_cheat(std::shared_ptr<c_page>& page,
                             const Renderer::font& font)
{
    auto cols = add_columns(page);

    // BINARY (GUI_64_DX11.exe.c:97714-97772): ONE "Cheat" groupbox —
    // Unload button, "Restrictions" multi dropdown [Anti-untrusted,
    // Anti-SMAC], "Log output" dropdown [Console, Screen] (def 1),
    // "Log events" multi dropdown [Kill, Fire], "Log color" colorpicker.
    auto cheat_gb = element_factory::make<c_groupbox>("Cheat");
    cheat_gb->set_title_font(&font); cheat_gb->set_bounds({0,0,0,260});
    cols->add_child(cheat_gb);
    {
        auto unload = element_factory::make<c_button>("Unload", []{});
        unload->set_font(&font); unload->set_bounds({0,0,0,24});
        cheat_gb->add_child(unload);
    }
    {
        auto rs = add_dd_p(cheat_gb, "settings.cheat.restrictions", "Restrictions",
                {"Anti-untrusted","Anti-SMAC"}, 0, font);
        rs->set_multi_select(true);
        rs->toggle_multi(0);   // Anti-untrusted on by default
    }
    add_dd_p(cheat_gb, "settings.cheat.log_output", "Log output",
            {"Console","Screen"}, 0, font);
    {
        auto le = add_dd_p(cheat_gb, "settings.cheat.log_events", "Log events",
                {"Kill","Fire"}, 0, font);
        le->set_multi_select(true);
        le->toggle_multi(0);   // Kill on by default
    }
    add_col_p(cheat_gb, "settings.cheat.log_color", "Log color",
              Renderer::color{0xFF,0xFF,0xFF,0xFF}, font);
}

void populate_settings_gui(std::shared_ptr<c_page>& page,
                           const Renderer::font& font,
                           const build_ctx& ctx)
{
    auto cols = add_columns(page);

    auto gui_gb = element_factory::make<c_groupbox>("GUI");
    gui_gb->set_title_font(&font); gui_gb->set_bounds({0,0,0,280});
    cols->add_child(gui_gb);
    {
        auto menu_state = ctx.menu_kb_state
            ? std::static_pointer_cast<c_keybinder::state_t>(ctx.menu_kb_state)
            : std::shared_ptr<c_keybinder::state_t>{};
        auto kb = std::make_shared<c_keybinder>("Open GUI", menu_state);
        kb->set_font(&font); gui_gb->add_child(kb);
    }
    {
        auto scl = add_dropdown(gui_gb, "GUI scaling",
                                {"100%","125%","150%","175%","200%"}, 0, font);
        // Capture by weak_ptr to avoid a shared_ptr cycle (dropdown
        // owns the on_change_ vector; a strong ref back to the
        // dropdown would keep both alive forever).
        std::weak_ptr<c_dropdown> wscl = scl;
        scl->add_on_change([wscl] {
            auto sp = wscl.lock();
            if (!sp) return;
            static const float k_table[5] = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
            const int idx = sp->get();
            if (idx >= 0 && idx < 5) {
                style::set_gui_scale(k_table[idx]);
            }
        });
    }
    {
        auto cp = add_col_p(gui_gb, "settings.gui.accent_color", "GUI accent color",
                            style::current().accent, font);
        // Mirror picker value into the live style accent.
        std::weak_ptr<c_colorpicker> wcp = cp;
        cp->add_on_change([wcp]{
            if (auto p = wcp.lock()) style::current().accent = p->color();
        });
    }
    add_check_p(gui_gb, "settings.gui.animations",               "GUI animations",                true,  font);
    add_check_p(gui_gb, "settings.gui.allow_resize",             "Allow resize",                  true,  font);
    add_check_p(gui_gb, "settings.gui.ignore_strict_kb_warning", "Ignore strict keybind warning", false, font);
    // "Center GUI" is a BUTTON inside a button grid (dump: button_grid
    // then "Center GUI").
    {
        auto grid = element_factory::make<c_button_grid>(/*per_row=*/1);
        grid->set_bounds({0,0,0,0});
        auto cg = element_factory::make<c_button>("Center GUI", []{
            // Center request — host code can hook a global flag here;
            // visual-only for now.
        });
        cg->set_font(&font); cg->set_bounds({0,0,0,24});
        grid->add_child(cg);
        gui_gb->add_child(grid);
    }

    // BINARY (dump menu_Settings.json > GUI): three sliders + a
    // "Smoothing" dropdown [Slow/Normal/Fastest] (the dropdown's options
    // sit between Accelerator and Distance ratio in the string table).
    auto sm_gb = element_factory::make<c_groupbox>("Smooth scrolling");
    sm_gb->set_title_font(&font); sm_gb->set_bounds({0,0,0,200});
    cols->add_child(sm_gb);
    add_float_p(sm_gb, "settings.smooth.accel_threshold", "Accelerate threshold", 0.0f, 20.0f, 0.0f, "", font);  // step 0.5
    add_float_p(sm_gb, "settings.smooth.accelerator",     "Accelerator",          0.0f, 20.0f, 0.0f, "", font);  // step 0.5
    add_float_p(sm_gb, "settings.smooth.distance_ratio",  "Distance ratio",       0.0f, 10.0f, 0.0f, "", font);  // step 0.1
    add_dd_p   (sm_gb, "settings.smooth.smoothing",       "Smoothing",
                {"Slow","Normal","Fastest"}, 1, font);

    // BINARY: a separate "Rounding" groupbox — Rounding checkbox +
    // "Factor" slider (0..10 step 0.25, " px", default 2.75 = the
    // gui_scale*2.75 radius the renderer uses). Both are LIVE-wired:
    // the checkbox flips every per-widget toggle, the slider drives
    // style::rounding().anim_scale (the shared radius multiplier).
    auto rnd_gb = element_factory::make<c_groupbox>("Rounding");
    rnd_gb->set_title_font(&font); rnd_gb->set_bounds({0,0,0,110});
    cols->add_child(rnd_gb);
    {
        auto cb = add_check_p(rnd_gb, "settings.rounding.enabled", "Rounding", true, font);
        std::weak_ptr<c_checkbox> wcb = cb;
        cb->add_on_change([wcb]{
            if (auto p = wcb.lock()) style::rounding().set_all(p->get());
        });
    }
    {
        auto sl = add_float_p(rnd_gb, "settings.rounding.factor",
                              "Factor", 0.0f, 10.0f, 2.75f, " px", font);
        std::weak_ptr<c_slider<float>> wsl = sl;
        sl->add_on_change([wsl]{
            if (auto p = wsl.lock()) style::rounding().anim_scale = p->get();
        });
    }
}

// Settings > Debug — the binary creates this subtab EMPTY (dump
// menu_Settings.json lists subtab "Debug" with no groupboxes). The old
// "Console" subtab was improvised; its log controls live in the Cheat
// groupbox per the binary.
void populate_settings_debug(std::shared_ptr<c_page>& /*page*/,
                             const Renderer::font& /*font*/)
{
}

// No-visual element that filters a listbox by a search textbox each
// frame. Filters from a shared MASTER list (the unfiltered source);
// re-runs only when the query text or the master list changes.
class list_filter_runner : public c_base_element {
public:
    // NOTE: kept VISIBLE (renders nothing) because c_page/c_groupbox skip
    // invisible children in update() — an invisible runner would never
    // filter. Hosted at PAGE level so the groupbox flow ignores it.
    list_filter_runner(std::weak_ptr<c_textbox_new> tb,
                       std::weak_ptr<c_listbox> lb,
                       std::shared_ptr<std::vector<std::string>> master)
        : c_base_element{""}, tb_{std::move(tb)}, lb_{std::move(lb)},
          master_{std::move(master)} {}

    std::string_view type_name() const override { return ", FilterRunner"; }
    bool             needs_hash() const override { return false; }

    void update(float /*dt*/) override {
        auto tb = tb_.lock(); auto lb = lb_.lock();
        if (!tb || !lb || !master_) return;
        std::string q;
        for (char c : tb->value()) q += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (q == last_q_ && master_->size() == last_n_) return;   // nothing changed
        last_q_ = q; last_n_ = master_->size();
        std::vector<std::string> out;
        for (const auto& s : *master_) {
            std::string sl;
            for (char c : s) sl += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (q.empty() || sl.find(q) != std::string::npos) out.push_back(s);
        }
        lb->set_options(std::move(out));
    }
    void render(Renderer::c_renderer_core&) override {}

private:
    std::weak_ptr<c_textbox_new> tb_;
    std::weak_ptr<c_listbox> lb_;
    std::shared_ptr<std::vector<std::string>> master_;
    std::string last_q_ = "\x01";          // force the first filter
    std::size_t last_n_ = static_cast<std::size_t>(-1);
};

// Scan a directory for files of a given extension, returning the bare names
// WITHOUT the extension (so the list shows "default", not "default.cfg" or
// stray "foo.json"). Missing dir → empty list.
static std::vector<std::string> scan_dir_names(const std::filesystem::path& dir,
                                               const std::string& ext = ".cfg")
{
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator{dir, ec}) {
        if (e.is_regular_file(ec) && e.path().extension() == ext)
            out.push_back(e.path().stem().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

void populate_settings_cloud(std::shared_ptr<c_page>& page,
                             const Renderer::font& font)
{
    auto cols = add_columns(page);

    // Layout matched to the original Settings>Cloud (og2-cloud capture):
    //   "Configurations" groupbox: Search box flush ON the list → 2-col
    //   button grid [Create|Delete, Save|Load, Paste|Copy, Refresh-wide].
    //   "Scripts" groupbox: Search → list → grid [Load|Unload,
    //   Reload|Reload all].
    // Lists are LOCAL (user 2026-06-11): configs = ./ot, scripts =
    // ./ot/scripts, rescanned by Create/Delete/Refresh/Reload all.
    const auto cfg_dir = std::filesystem::path{"ot"};
    const auto scr_dir = std::filesystem::path{"ot"} / "scripts";

    // ---- Configurations ----------------------------------------------------
    auto cfg_master = std::make_shared<std::vector<std::string>>(
        scan_dir_names(cfg_dir));

    auto cfg_gb = element_factory::make<c_groupbox>("Configurations");
    cfg_gb->set_title_font(&font); cfg_gb->set_bounds({0,0,0,300});
    cols->add_child(cfg_gb);

    auto cfg_search = element_factory::make<c_textbox_new>("", "Search...");
    cfg_search->set_font(&font);
    cfg_gb->add_child(cfg_search);

    auto cfg_list = element_factory::make<c_listbox>("", *cfg_master, 4, false);
    cfg_list->set_font(&font); cfg_gb->add_child(cfg_list);

    // Filter runner lives at PAGE level (renders nothing) so it gets
    // updated and the groupbox flow ignores it.
    page->add_child(std::make_shared<list_filter_runner>(
        cfg_search, cfg_list, cfg_master));

    // Rescan helper shared by Create / Delete / Refresh.
    std::weak_ptr<c_listbox> w_cfg_list = cfg_list;
    auto cfg_rescan = [cfg_master, w_cfg_list, cfg_dir] {
        *cfg_master = scan_dir_names(cfg_dir);
        if (auto lb = w_cfg_list.lock()) lb->set_options(*cfg_master);
    };

    // Create dialog — name textbox + Create/Cancel. Hosted on the page so
    // it renders on the overlay layer while Settings>Cloud is active.
    auto name_tb = element_factory::make<c_textbox_new>("", "Config name");
    name_tb->set_font(&font);
    name_tb->set_bounds({0, 0, 0, 24});
    auto create_dlg = element_factory::make<c_dialog_window>("Create configuration");
    create_dlg->set_title_font(&font);
    create_dlg->set_button_font(&font);
    create_dlg->set_modal(true);
    create_dlg->set_visible(false);
    create_dlg->set_confirm_label("Create");
    create_dlg->set_dismiss_label("Cancel");
    create_dlg->set_auto_size(false);
    create_dlg->set_size(280.0f, 150.0f);
    create_dlg->add_child(name_tb);
    {
        std::weak_ptr<c_textbox_new> w_name = name_tb;
        create_dlg->set_on_confirm([w_name, cfg_dir, cfg_rescan] {
            if (auto nm = w_name.lock()) {
                if (!nm->value().empty()) {
                    std::error_code ec;
                    std::filesystem::create_directories(cfg_dir, ec);
                    auto p = cfg_dir / (nm->value() + ".cfg");
                    // Write the CURRENT settings registry so a new config is
                    // immediately loadable (was writing just a "// name" stub).
                    GUI::config_system::manager::save_to(p.string(), settings());
                    cfg_rescan();
                }
                nm->set_value("");
            }
        });
        create_dlg->set_on_dismiss([w_name] {
            if (auto nm = w_name.lock()) nm->set_value("");
        });
    }
    page->add_child(create_dlg);

    // 2-column button grid: Create, Delete, Save, Load, Paste, Copy,
    // Refresh (the odd 7th spans full width — c_button_grid behavior).
    {
        std::weak_ptr<c_dialog_window> w_dlg = create_dlg;
        auto open_create = [w_dlg] { if (auto d = w_dlg.lock()) d->open(); };
        auto selected_path = [w_cfg_list, cfg_dir]() -> std::filesystem::path {
            auto lb = w_cfg_list.lock();
            if (!lb) return {};
            const int i = lb->get();
            const auto& opts = lb->options();
            if (i < 0 || i >= static_cast<int>(opts.size())) return {};
            // List entries are bare names now; re-append the .cfg extension.
            return cfg_dir / (opts[static_cast<std::size_t>(i)] + ".cfg");
        };
        auto grid = element_factory::make<c_button_grid>(2);
        auto add_btn = [&](const char* label, std::function<void()> cb) {
            auto b = element_factory::make<c_button>(label, std::move(cb));
            b->set_font(&font); b->set_bounds({0,0,0,24});
            grid->add_child(b);
        };
        add_btn("Create", open_create);
        add_btn("Delete", [selected_path, cfg_rescan] {
            const auto p = selected_path();
            if (!p.empty()) {
                std::error_code ec;
                std::filesystem::remove(p, ec);
                cfg_rescan();
            }
        });
        // Save the current registry to the selected config; Load reads it
        // back (config_bridge.sync_menu_to_config runs each frame and pushes
        // the loaded values into ctx.m_config).
        add_btn("Save", [selected_path] {
            const auto p = selected_path();
            if (!p.empty())
                GUI::config_system::manager::save_to(p.string(), settings());
        });
        add_btn("Load", [selected_path] {
            const auto p = selected_path();
            if (!p.empty())
                GUI::config_system::manager::load_from(p.string(), settings());
        });
        add_btn("Paste",  []{});
        add_btn("Copy",   []{});
        add_btn("Refresh", cfg_rescan);
        cfg_gb->add_child(grid);
    }

    // ---- Scripts -----------------------------------------------------------
    auto scr_master = std::make_shared<std::vector<std::string>>(
        scan_dir_names(scr_dir));

    auto scr_gb = element_factory::make<c_groupbox>("Scripts");
    scr_gb->set_title_font(&font); scr_gb->set_bounds({0,0,0,200});
    cols->add_child(scr_gb);

    auto scr_search = element_factory::make<c_textbox_new>("", "Search...");
    scr_search->set_font(&font);
    scr_gb->add_child(scr_search);

    auto scr_list = element_factory::make<c_listbox>("", *scr_master, 4, false);
    scr_list->set_font(&font); scr_gb->add_child(scr_list);

    page->add_child(std::make_shared<list_filter_runner>(
        scr_search, scr_list, scr_master));

    // 2-column grid: Load, Unload, Reload, Reload all (rescans ./ot/scripts).
    {
        std::weak_ptr<c_listbox> w_scr_list = scr_list;
        auto scr_rescan = [scr_master, w_scr_list, scr_dir] {
            *scr_master = scan_dir_names(scr_dir);
            if (auto lb = w_scr_list.lock()) lb->set_options(*scr_master);
        };
        auto grid = element_factory::make<c_button_grid>(2);
        auto add_btn = [&](const char* label, std::function<void()> cb) {
            auto b = element_factory::make<c_button>(label, std::move(cb));
            b->set_font(&font); b->set_bounds({0,0,0,24});
            grid->add_child(b);
        };
        add_btn("Load",   []{});   // script engine not wired
        add_btn("Unload", []{});
        add_btn("Reload", []{});
        add_btn("Reload all", scr_rescan);
        scr_gb->add_child(grid);
    }
}

} // anon

// ---------------------------------------------------------------------------
// Public entry point.
// ---------------------------------------------------------------------------

void build_cheat_menu(c_tab_manager& tabs, const build_ctx& ctx)
{
    // RAII-style scope guard for the thread-local settings pointer
    // used by add_check_p / add_int_p / … helpers. Pointer is set
    // to ctx.settings (or the process-wide default when absent) for
    // the duration of build, then cleared.
    struct settings_scope {
        settings_scope(c_settings* p) { g_current_settings = p; }
        ~settings_scope()             { g_current_settings = nullptr; }
    } scope(ctx.settings ? ctx.settings : &default_settings());

    const Renderer::font& font = *ctx.body_font;
    const Renderer::font* icon_font = ctx.weapon_icon_font;

    auto legit = tab_with_subtabs("Legit", font, {
        {"Main",    [&](auto& p, auto& f) { populate_legit_main(p, f); }},
        {"Weapons", [&](auto& p, auto& f) {
                       populate_legit_weapons(p, f, icon_font);
                   }},
    });
    tabs.add_child(legit);

    auto rage = tab_with_subtabs("Rage", font, {
        {"Main",    [&](auto& p, auto& f) { populate_rage_main(p, f, ctx); }},
        {"Weapons", [&](auto& p, auto& f) {
                       populate_rage_weapons(p, f, icon_font);
                   }},
    });
    tabs.add_child(rage);

    // Binary subtab order (dump menu_Visuals.json):
    // Players / World / Extra.
    auto visuals = tab_with_subtabs("Visuals", font, {
        {"Players", [&](auto& p, auto& f) { populate_visuals_players(p, f); }},
        {"World",   [&](auto& p, auto& f) { populate_visuals_world(p, f); }},
        {"Extra",   [&](auto& p, auto& f) { populate_visuals_extra(p, f); }},
    });
    tabs.add_child(visuals);

    // Binary subtab order (dump menu_Miscellaneous.json):
    // View / Helpers / Movement / Skins.
    auto misc = tab_with_subtabs("Miscellaneous", font, {
        {"View",     [&](auto& p, auto& f) { populate_misc_view(p, f); }},
        {"Helpers",  [&](auto& p, auto& f) { populate_misc_helpers(p, f); }},
        {"Movement", [&](auto& p, auto& f) { populate_misc_movement(p, f, ctx); }},
        {"Skins",    [&](auto& p, auto& f) { populate_misc_skins(p, f); }},
    });
    tabs.add_child(misc);

    // Binary subtab order (dump menu_Settings.json):
    // Cheat / GUI / Cloud / Debug.
    auto settings = tab_with_subtabs("Settings", font, {
        {"Cheat", [&](auto& p, auto& f) { populate_settings_cheat(p, f); }},
        {"GUI",   [&](auto& p, auto& f) { populate_settings_gui(p, f, ctx); }},
        {"Cloud", [&](auto& p, auto& f) { populate_settings_cloud(p, f); }},
        {"Debug", [&](auto& p, auto& f) { populate_settings_debug(p, f); }},
    });
    tabs.add_child(settings);
}

} // namespace GUI::menu
