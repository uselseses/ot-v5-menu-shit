// =============================================================================
// composites.cpp --- antiaim_groupbox_t / configurable_player_model_t /
// legit_weapon_page_t / rage_weapon_page_t / dialog_window_builder_t.
//
// Each ctor adds its children via element_factory::make so the new
// nodes inherit shared_from_this correctly.
// =============================================================================

#include <GUI/pages/antiaim_groupbox_t.hpp>
#include <GUI/pages/configurable_player_model_t.hpp>
#include <GUI/pages/legit_weapon_page_t.hpp>
#include <GUI/pages/rage_weapon_page_t.hpp>
#include <GUI/pages/dialog_window_builder_t.hpp>
#include <GUI/pages/abstract_visuals_page_t.hpp>
#include <GUI/pages/enemy_visuals_page_t.hpp>
#include <GUI/pages/team_visuals_page_t.hpp>
#include <GUI/pages/self_visuals_page_t.hpp>

#include <Renderer/color.hpp>
#include <GUI/menu/c_settings.hpp>

#include <functional>
#include <memory>
#include <string>

#include <GUI/core/element_factory.hpp>
#include <GUI/containers/c_dialog_window.hpp>
#include <GUI/containers/c_page_manager.hpp>

#include <GUI/elements/c_button.hpp>
#include <GUI/elements/c_checkbox.hpp>
#include <GUI/elements/c_column.hpp>
#include <GUI/elements/c_colorpicker.hpp>
#include <GUI/elements/c_dropdown.hpp>
#include <GUI/elements/c_keybinder.hpp>
#include <GUI/elements/c_label.hpp>
#include <GUI/elements/c_separator.hpp>
#include <GUI/elements/c_slider.hpp>
#include <GUI/elements/c_text_container.hpp>

namespace GUI {

// ============================================================================
// abstract_visuals_page_t — shared helper
// ============================================================================
void abstract_visuals_page_t::add_color_field(const Renderer::font* font,
                                              std::string label,
                                              std::uint32_t default_argb)
{
    auto cp = element_factory::make<c_colorpicker>(
        std::move(label),
        Renderer::color::from_packed(default_argb));
    cp->set_font(font);
    add_child(cp);
}

void abstract_visuals_page_t::add_chams_compound(const Renderer::font* font,
                                                 std::string label,
                                                 std::uint32_t primary_argb,
                                                 std::uint32_t secondary_argb)
{
    // Verified per `chams_compound_widget__ctor @ 0x1400337B0` call
    // sequence (positions 4689..12473 in IDA decompile):
    //   1. vector_string__copy_range_ctor → ensure chams styles vector
    //      (thread-safe init of `xmmword_1402467C8`)
    //   2. propagate_state_to_child → store role data
    //   3. alloc_c_label_with_string("Style")
    //   4. alloc_c_dropdown_with_string(empty_label, state_ref)
    //   5. shared_ptr__move_assign(dropdown → compound widget slot)
    //   6. emplace_c_colorpicker × 2 (primary, secondary)
    //   7. propagate_state_to_child × 2 (register widgets)

    // (1) Outer section title — the role's quadrant name
    auto lbl_main = element_factory::make<c_label>(label);
    lbl_main->set_font(font);
    add_child(lbl_main);

    // (2) "Style" sub-label
    auto lbl_style = element_factory::make<c_label>("Style");
    lbl_style->set_font(font);
    add_child(lbl_style);

    // (3) Chams styles dropdown — 6 options from the binary's lazy-init
    // static vector `xmmword_1402467C8` (built by 6 string_assign calls in
    // chams_compound_widget__ctor: Custom / Flat / Pulse / Wireframe /
    // Glow / Glow (2)). Empty dropdown label per binary (label is shown
    // separately by the "Style" sibling).
    auto dd = element_factory::make<c_dropdown>(
        std::string{""},
        std::vector<std::string>{
            "Custom", "Flat", "Pulse", "Wireframe", "Glow", "Glow (2)",
        });
    dd->set_font(font);
    dd->set_bounds({0, 0, 0, 36});
    add_child(dd);

    // (6) Two colorpickers — primary + secondary. Binary calls
    // `emplace_c_colorpicker` twice in succession.
    auto cp1 = element_factory::make<c_colorpicker>(
        label + " (primary)", Renderer::color::from_packed(primary_argb));
    cp1->set_font(font);
    add_child(cp1);

    auto cp2 = element_factory::make<c_colorpicker>(
        label + " (secondary)", Renderer::color::from_packed(secondary_argb));
    cp2->set_font(font);
    add_child(cp2);
}

void abstract_visuals_page_t::populate_shared_sections(const Renderer::font* font)
{
    // Eight ESP sections per the binary sub_1400318A0 call sequence.
    // Each section is a header c_label + one c_colorpicker + one c_checkbox.
    // Section order verified via decompile positions 18201..36800:
    //   "Ammo"         (sub_140035350)
    //   "Bounding box" (sub_140035700 — hardcoded)
    //   "Glow"         (sub_140035350)
    //   "Health"       (sub_140035890 — hardcoded)
    //   "Name"         (sub_140035350)
    //   "Skeleton"     (sub_140035A20 — hardcoded)
    //   "Weapons"      (sub_140035BB0 — hardcoded)
    //   "Flags"        (sub_140035350)
    static constexpr const char* k_sections[] = {
        "Ammo", "Bounding box", "Glow", "Health",
        "Name", "Skeleton",     "Weapons", "Flags",
    };
    for (const char* s : k_sections) {
        auto lbl = element_factory::make<c_label>(s);
        lbl->set_font(font);
        add_child(lbl);

        auto cp = element_factory::make<c_colorpicker>(
            std::string{s}, Renderer::color::from_packed(0xFFFAA618));
        cp->set_font(font);
        add_child(cp);

        auto cb = element_factory::make<c_checkbox>(std::string{s});
        cb->set_font(font);
        add_child(cb);
    }

    // 18 ESP icon flags — each becomes a checkbox so the user can toggle
    // it. Order matches the 18 string_assign calls in sub_1400318A0
    // (positions 11763-15015 of the decompile).
    static constexpr const char* k_icons[] = {
        "Lag compensation", "Money",          "Armor",
        "Flashed",          "Reloading",      "Scoped",
        "Defusing",         "Location",       "Objective carrier",
        "Steam friend",     "Fake duck",      "Stomach damage",
        "Firing",           "Latency",        "Exploiting",
        "Dormant",          "Icons",          "Inventory",
    };
    for (const char* n : k_icons) {
        auto cb = element_factory::make<c_checkbox>(n);
        cb->set_font(font);
        add_child(cb);
    }
}

// ============================================================================
// enemy_visuals_page_t  — binary: sub_140085580
// Constructor adds 4 colorpickers: Visible / Attachments / Hidden / History
// with colors -350696 / -15140186 / -379624 / (truncated) per IDA decompile.
// ============================================================================
enemy_visuals_page_t::enemy_visuals_page_t(const Renderer::font* font)
    : abstract_visuals_page_t{"Enemy", "A", 5}
{
    set_title_font(font);
    set_bounds({0, 0, 0, 600});
    populate_shared_sections(font);                    // 8 sections + 18 icons
    // 4 chams compound widgets per binary sub_1400343D0 (Visible,
    // Attachments, Hidden, History). Each is a compound of label +
    // Style sub-label + style dropdown(6) + 2 colorpickers.
    add_chams_compound(font, "Visible",     0xFFFAA618, 0xFF26431B);
    add_chams_compound(font, "Attachments", 0xFF1B1A26, 0xFFFA3618);
    add_chams_compound(font, "Hidden",      0xFFFA3618, 0xFF18FA1B);
    add_chams_compound(font, "History",     0xFFD8D8DE, 0xFFFAA618);
}

// ============================================================================
// team_visuals_page_t  — binary: sub_1400856B0
// ============================================================================
team_visuals_page_t::team_visuals_page_t(const Renderer::font* font)
    : abstract_visuals_page_t{"Team", "B", 5}
{
    set_title_font(font);
    set_bounds({0, 0, 0, 600});
    populate_shared_sections(font);
    add_color_field(font, "Visible",     0xFF4AC06A);
    add_color_field(font, "Attachments", 0xFF26431B);
    add_color_field(font, "Hidden",      0xFF18FA1B);
    add_color_field(font, "History",     0xFFD8DED8);
}

// ============================================================================
// self_visuals_page_t  — binary: sub_1400857E0
// ============================================================================
self_visuals_page_t::self_visuals_page_t(const Renderer::font* font)
    : abstract_visuals_page_t{"Self", "C", 5}
{
    set_title_font(font);
    set_bounds({0, 0, 0, 600});
    populate_shared_sections(font);
    add_color_field(font, "Visible",     0xFFFAA618);
    add_color_field(font, "Attachments", 0xFF261A1B);
    add_color_field(font, "Hidden",      0xFFFA1818);
    add_color_field(font, "History",     0xFFDED8D8);
}

namespace {

// Helper: append a label + checkbox row. Returns the checkbox so the
// caller can wire on_change callbacks.
std::shared_ptr<c_checkbox>
add_checkbox(c_groupbox& g, const std::string& label, const Renderer::font* font)
{
    auto cb = element_factory::make<c_checkbox>(label);
    cb->set_font(font);
    g.add_child(cb);
    return cb;
}

std::shared_ptr<c_slider<float>>
add_slider_f(c_groupbox& g, const std::string& label, float lo, float hi, float init,
             const std::string& suffix, const Renderer::font* font)
{
    auto s = element_factory::make<c_slider<float>>(label, lo, hi, nullptr, suffix);
    s->set_font(font);
    s->set_bounds({0, 0, 0, 32});
    s->set(init);
    g.add_child(s);
    return s;
}

std::shared_ptr<c_slider<int>>
add_slider_i(c_groupbox& g, const std::string& label, int lo, int hi, int init,
             const std::string& suffix, const Renderer::font* font)
{
    auto s = element_factory::make<c_slider<int>>(label, lo, hi, nullptr, suffix);
    s->set_font(font);
    s->set_bounds({0, 0, 0, 32});
    s->set(init);
    g.add_child(s);
    return s;
}

std::shared_ptr<c_dropdown>
add_dropdown(c_groupbox& g, const std::string& label,
             std::vector<std::string> options, int default_idx,
             const Renderer::font* font)
{
    auto d = element_factory::make<c_dropdown>(label, std::move(options));
    d->set_font(font);
    d->set_bounds({0, 0, 0, 36});
    d->set(default_idx);
    g.add_child(d);
    return d;
}

// ---- path-bound variants -------------------------------------------------
// The per-weapon rage/legit pages originally built controls with no settings
// path, so nothing persisted and the cheat side could never read them (rage
// "Enabled"/"Minimum damage"/"Hitbox" always fell back to defaults, so the
// ragebot never fired). These bind the element to c_settings so the state is
// shared + persisted, exactly like add_check_p/add_int_p in the menu builder.

std::shared_ptr<c_checkbox>
add_checkbox_p(c_groupbox& g, const std::string& path, const std::string& label,
               bool default_v, const Renderer::font* font)
{
    auto cb = element_factory::make<c_checkbox>(
        label, GUI::menu::default_settings().bool_state(path, default_v));
    cb->set_font(font);
    g.add_child(cb);
    return cb;
}

std::shared_ptr<c_slider<int>>
add_slider_i_p(c_groupbox& g, const std::string& path, const std::string& label,
               int lo, int hi, int init, const std::string& suffix,
               const Renderer::font* font)
{
    auto s = element_factory::make<c_slider<int>>(
        label, lo, hi, GUI::menu::default_settings().int_state(path, init), suffix);
    s->set_font(font);
    s->set_bounds({0, 0, 0, 32});
    g.add_child(s);
    return s;
}

std::shared_ptr<c_slider<float>>
add_slider_f_p(c_groupbox& g, const std::string& path, const std::string& label,
               float lo, float hi, float init, const std::string& suffix,
               const Renderer::font* font)
{
    auto s = element_factory::make<c_slider<float>>(
        label, lo, hi, GUI::menu::default_settings().float_state(path, init), suffix);
    s->set_font(font);
    s->set_bounds({0, 0, 0, 32});
    g.add_child(s);
    return s;
}

std::shared_ptr<c_dropdown>
add_dropdown_p(c_groupbox& g, const std::string& path, const std::string& label,
               std::vector<std::string> options, int default_idx,
               const Renderer::font* font)
{
    auto d = element_factory::make<c_dropdown>(
        label, std::move(options), GUI::menu::default_settings().int_state(path, default_idx));
    d->set_font(font);
    d->set_bounds({0, 0, 0, 36});
    g.add_child(d);
    return d;
}

} // anon

// ============================================================================
// antiaim_groupbox_t
// ============================================================================
antiaim_groupbox_t::antiaim_groupbox_t(std::string label,
                                      std::shared_ptr<c_base_element> /*parent*/,
                                      std::shared_ptr<c_dropdown> condition_dropdown,
                                      antiaim_condition_e condition,
                                      const Renderer::font* body_font)
    : c_groupbox{std::move(label)},
      condition_dropdown_{std::move(condition_dropdown)},
      condition_{condition}
{
    // Caller wires us into `parent` via parent->add_child(...) *after*
    // make_shared completes --- shared_from_this() is UB inside this ctor.
    set_title_font(body_font);
    set_bounds({0, 0, 0, 300});

    // EXACT control block per the per-condition builder decompile
    // (GUI_64_DX11.exe.c:35700-36260) cross-checked with the dump
    // (menu_Rage.json > Main > Antiaim) and .rdata cluster 0x1401F50xx:
    //   Pitch dd, Yaw base dd, Yaw offset (L)/(R), Fake angles slider,
    //   LBY mode dd, Jitter mode dd, Jitter offset, Invert fake on jitter,
    //   Stretch yaw dd, Stretch pitch/yaw/roll sliders.
    // ("Auto direction"/"Manual" are CONDITIONS, not controls; "Fake up"/
    //  "Fake down (Untrusted)" are Pitch OPTIONS.)
    add_dropdown(*this, "Pitch",
                 {"Off", "Down", "Emotion", "Up", "Zero",
                  "Fake up", "Fake down (Untrusted)"}, 0, body_font);
    add_dropdown(*this, "Yaw base",        {"At targets", "View angles"}, 0, body_font);
    add_slider_i(*this, "Yaw offset (L)", -180, 180,   0, "\xC2\xB0", body_font); // ° in UTF-8
    add_slider_i(*this, "Yaw offset (R)", -180, 180,   0, "\xC2\xB0", body_font);
    add_slider_i(*this, "Fake angles",     -60,  60,   0, "\xC2\xB0", body_font);
    add_dropdown(*this, "LBY mode",       {"Normal", "Opposite", "Sway"},  0, body_font);
    add_dropdown(*this, "Jitter mode",    {"Off", "Center", "Offset", "Random"}, 0, body_font);
    add_slider_i(*this, "Jitter offset", -180, 180, 0, "\xC2\xB0", body_font);
    add_checkbox(*this, "Invert fake on jitter", body_font);
    add_dropdown(*this, "Stretch yaw",    {"Off", "Back", "Up", "Custom"}, 0, body_font);
    add_slider_i(*this, "Stretch pitch",  -89,  89, 0, "\xC2\xB0", body_font);
    add_slider_i(*this, "Stretch yaw",   -180, 180, 0, "\xC2\xB0", body_font);
    add_slider_i(*this, "Stretch roll",   -45,  45, 0, "\xC2\xB0", body_font);
}

bool antiaim_groupbox_t::visible() const noexcept
{
    if (!condition_dropdown_) return c_groupbox::visible();
    return static_cast<antiaim_condition_e>(condition_dropdown_->get()) == condition_;
}

void antiaim_groupbox_t::update(float dt)
{
    c_groupbox::update(dt);
}

// ============================================================================
// configurable_player_model_t
// ============================================================================
configurable_player_model_t::configurable_player_model_t(
    std::string label, std::shared_ptr<c_base_element> /*parent*/,
    std::shared_ptr<c_dropdown> style_dropdown, chams_style_e style,
    const Renderer::font* body_font, std::uint64_t feature_flags)
    : c_groupbox{std::move(label)},
      style_dropdown_{std::move(style_dropdown)},
      style_{style},
      feature_flags_{feature_flags}
{
    // Caller wires us into `parent` after make_shared completes.
    set_title_font(body_font);
    set_bounds({0, 0, 0, 220});

    add_checkbox(*this, "Visible",     body_font);
    add_checkbox(*this, "Hidden",      body_font);
    add_checkbox(*this, "Attachments", body_font);
    add_checkbox(*this, "Wireframe",   body_font);
    add_slider_f(*this, "Reflectivity", 0.0f, 1.0f, 0.5f, "", body_font);
    add_slider_f(*this, "Phong",        0.0f, 1.0f, 0.7f, "", body_font);
    add_checkbox(*this, "Pearlescent", body_font);
    add_checkbox(*this, "Pulse",       body_font);

    auto col = element_factory::make<c_colorpicker>(
        "Color", Renderer::color{236, 72, 153, 255}, true);
    col->set_font(body_font);
    col->set_bounds({0, 0, 0, 28});
    add_child(col);
}

bool configurable_player_model_t::visible() const noexcept
{
    if (!style_dropdown_) return c_groupbox::visible();
    return static_cast<chams_style_e>(style_dropdown_->get()) == style_;
}

void configurable_player_model_t::update(float dt)
{
    c_groupbox::update(dt);
}

// ============================================================================
// legit_weapon_page_t
// ============================================================================
legit_weapon_page_t::legit_weapon_page_t(std::shared_ptr<c_page_manager> /*page_manager*/,
                                        std::string                     weapon_name,
                                        std::string                     subtitle,
                                        int                             weapon_id,
                                        const Renderer::font*           body_font)
    : c_page{weapon_name + (subtitle.empty() ? std::string{} : (" - " + subtitle))},
      weapon_id_{weapon_id}
{
    // Caller wires us into the page_manager via add_child + set_active_page
    // *after* make_shared completes.

    // Demo layout (demo/main.cpp 2026-06-11): Aimbot + Trigger only.
    // The previous Silent aim / Backtracking / Triggerbot+Autowall
    // groupboxes were cheat-only and have been removed to match.

    // -- aimbot group (demo: 6 cb + 2 sliders) --
    auto aim = element_factory::make<c_groupbox>("Aimbot");
    aim->set_title_font(body_font);
    aim->set_bounds({0, 0, 0, 220});
    add_checkbox(*aim, "Enabled",         body_font);
    add_checkbox(*aim, "Aim assistance",  body_font);
    add_checkbox(*aim, "Silent aim",      body_font);
    add_checkbox(*aim, "Auto scope",      body_font);
    add_checkbox(*aim, "Auto fire",       body_font);
    add_checkbox(*aim, "Auto stop",       body_font);
    add_slider_i(*aim, "Hitchance", 0, 100, 72, " %",       body_font);
    add_slider_f(*aim, "FOV",       0.0f, 30.0f, 5.0f, "\xC2\xB0", body_font);
    add_child(aim);

    // -- trigger group --
    auto tb = element_factory::make<c_groupbox>("Trigger");
    tb->set_title_font(body_font);
    tb->set_bounds({0, 0, 0, 170});
    add_checkbox(*tb, "Trigger bot",      body_font);
    add_checkbox(*tb, "Hitchance check",  body_font);
    add_checkbox(*tb, "Smoke check",      body_font);
    add_slider_i(*tb, "Trigger delay", 0, 500,  50, " ms", body_font);
    add_slider_i(*tb, "Burst time",    0, 1000,150, " ms", body_font);
    add_child(tb);
}

// ============================================================================
// rage_weapon_page_t
// ============================================================================
// Helper class — single-shot update hook that ticks visibility of a
// list of widgets based on a bool state. Has no visuals; just runs
// its callback during update. Used by rage_weapon_page_t below to
// gate Hitbox/Accuracy controls behind the per-weapon Enabled toggle
// (user 2026-06-02 "if enabled isnt on … other parts dont render").
class gate_runner_t : public c_base_element {
public:
    using callback_fn = std::function<void()>;
    explicit gate_runner_t(callback_fn cb)
        : c_base_element{""}, cb_{std::move(cb)} { set_visible(false); }
    std::string_view type_name() const override { return ", GateRunner"; }
    bool             needs_hash() const override { return false; }
    void update(float /*dt*/) override { if (cb_) cb_(); }
    void render(Renderer::c_renderer_core&) override {}
private:
    callback_fn cb_;
};

rage_weapon_page_t::rage_weapon_page_t(std::shared_ptr<c_page_manager> /*page_manager*/,
                                      std::string                     weapon_name,
                                      std::string                     subtitle,
                                      int                             weapon_id,
                                      const Renderer::font*           body_font)
    : c_page{weapon_name + (subtitle.empty() ? std::string{} : (" - " + subtitle))},
      weapon_id_{weapon_id}
{
    // FULLY decoded from the ctor decompile @ 0x140039230 (RTTI-confirmed
    // rage_weapon_page_t). Two option vectors are built up front:
    //   hitboxes  = sub_1400369C0 → [Head, Upper chest, Chest, Lower chest,
    //               Stomach, Pelvis, Arms, Legs, Feet]
    //   conditions= [Duck, Early, Center, Lethal, Visible]
    //
    //   "Aimbot" groupbox (decompile order, defaults from the ctor tail):
    //     Enabled                  checkbox, off
    //     Field of view            slider 0..180 step 1, DEFAULT 180
    //     Minimum damage           slider 0..135 " damage", DEFAULT 45
    //     Hitbox                   multi dd (hitboxes), none selected
    //     Hitbox override          multi dd (hitboxes), none selected
    //     Multipoint               multi dd (hitboxes), none selected
    //     Minimum damage override  slider 0..135 " damage", default 0
    //     Head multipoint          slider 0..1 "%" step 0.01, DEFAULT 0.45
    //     Body multipoint          slider 0..1 "%" step 0.01, DEFAULT 0.60
    //   "Accuracy" groupbox:
    //     Hitchance                slider 0..1 "%" step 0.01, default 0
    //     Auto scope / Auto stop   checkboxes, off
    //     Auto stop conditions     multi dd (conditions), none selected
    auto cols = element_factory::make<c_column>(2);
    add_child(cols);

    const std::vector<std::string> k_hitboxes{
        "Head", "Upper chest", "Chest", "Lower chest", "Stomach",
        "Pelvis", "Arms", "Legs", "Feet",
    };

    auto hitbox_gb = element_factory::make<c_groupbox>("Aimbot");
    hitbox_gb->set_title_font(body_font);
    hitbox_gb->set_bounds({0, 0, 0, 420});
    cols->add_child(hitbox_gb);

    // Per-weapon settings path: rage.weapon.<weapon_id>.<leaf>
    const std::string wp = "rage.weapon." + std::to_string(weapon_id_) + ".";

    auto enabled_cb = add_checkbox_p(*hitbox_gb, wp + "enabled", "Enabled",
                                     false, body_font);
    add_slider_i_p(*hitbox_gb, wp + "fov", "Field of view", 0, 180, 180,
                   "\xC2\xB0", body_font);
    // Min damage keeps the dynamic accent-at-min value text (user 2026-06-01).
    add_slider_i_p(*hitbox_gb, wp + "min_damage", "Minimum damage", 0, 135, 45,
                   " damage", body_font)
        ->set_accent_at_min(true);
    {
        auto hb = add_dropdown_p(*hitbox_gb, wp + "hitbox", "Hitbox",
                                 k_hitboxes, 0, body_font);
        hb->set_multi_select(true);
        auto ho = add_dropdown_p(*hitbox_gb, wp + "hitbox_override",
                                 "Hitbox override", k_hitboxes, 0, body_font);
        ho->set_multi_select(true);
        auto mp = add_dropdown_p(*hitbox_gb, wp + "multipoint", "Multipoint",
                                 k_hitboxes, 0, body_font);
        mp->set_multi_select(true);
    }
    add_slider_i_p(*hitbox_gb, wp + "min_damage_override",
                   "Minimum damage override", 0, 135, 0, " damage", body_font);
    add_slider_f_p(*hitbox_gb, wp + "head_multipoint", "Head multipoint",
                   0.0f, 1.0f, 0.45f, "%", body_font);
    add_slider_f_p(*hitbox_gb, wp + "body_multipoint", "Body multipoint",
                   0.0f, 1.0f, 0.60f, "%", body_font);

    auto accuracy = element_factory::make<c_groupbox>("Accuracy");
    accuracy->set_title_font(body_font);
    accuracy->set_bounds({0, 0, 0, 220});
    cols->add_child(accuracy);

    add_slider_i_p(*accuracy, wp + "hitchance", "Hitchance", 0, 100, 0, "%",
                   body_font);
    add_checkbox_p(*accuracy, wp + "auto_scope", "Auto scope", false, body_font);
    add_checkbox_p(*accuracy, wp + "auto_stop",  "Auto stop",  false, body_font);
    {
        auto dd = add_dropdown_p(*accuracy, wp + "auto_stop_conditions",
                     "Auto stop conditions",
                     {"Duck", "Early", "Center", "Lethal", "Visible"},
                     0, body_font);
        dd->set_multi_select(true);
    }

    // Reveal the Aimbot controls (everything after "Enabled") in-place
    // while Enabled is on. The Accuracy box ALWAYS renders (user
    // 2026-06-11: "accuracy should render even if enabled is off").
    std::weak_ptr<c_checkbox> w_en = enabled_cb;
    auto pred = [w_en] { auto c = w_en.lock(); return c && c->get(); };
    hitbox_gb->set_reveal_master(enabled_cb, pred);
}

// ============================================================================
// dialog_window_builder_t
// ============================================================================
dialog_window_builder_t::dialog_window_builder_t(std::string title)
    : title_{std::move(title)} {}

dialog_window_builder_t& dialog_window_builder_t::header(std::string h)
{ header_ = std::move(h); return *this; }

dialog_window_builder_t& dialog_window_builder_t::body_text(std::string t)
{ body_ = std::move(t); return *this; }

dialog_window_builder_t& dialog_window_builder_t::button(std::string label, callback cb)
{ buttons_.emplace_back(std::move(label), std::move(cb)); return *this; }

dialog_window_builder_t& dialog_window_builder_t::wrap_width(float w)
{ wrap_w_ = w; return *this; }

dialog_window_builder_t& dialog_window_builder_t::on_dismiss(callback fn)
{ on_dismiss_ = std::move(fn); return *this; }

dialog_window_builder_t& dialog_window_builder_t::on_confirm(callback fn)
{ on_confirm_ = std::move(fn); return *this; }

dialog_window_builder_t& dialog_window_builder_t::dismiss_label(std::string l)
{ dismiss_label_ = std::move(l); return *this; }

dialog_window_builder_t& dialog_window_builder_t::confirm_label(std::string l)
{ confirm_label_ = std::move(l); return *this; }

std::shared_ptr<c_dialog_window>
dialog_window_builder_t::build(const Renderer::font* body_font) const
{
    auto dlg = element_factory::make<c_dialog_window>(title_);
    dlg->set_title_font(body_font);
    // Window size: heuristic = wrap_w_ + padding.
    dlg->set_bounds({0, 0, wrap_w_ + 24.0f, 220.0f});

    if (!header_.empty()) {
        auto h = element_factory::make<c_label>(header_);
        h->set_font(body_font);
        h->set_bounds({0, 0, 0, 22});
        dlg->add_child(h);
        auto sep = element_factory::make<c_separator>();
        sep->set_bounds({0, 0, 0, 8});
        dlg->add_child(sep);
    }

    if (!body_.empty()) {
        auto tc = element_factory::make<c_text_container>();
        tc->set_font(body_font);
        tc->set_text(body_);
        tc->set_bounds({0, 0, 0, 100});
        dlg->add_child(tc);
    }

    // Button row --- caller-supplied buttons + optional Confirm/Dismiss.
    for (auto& [label, cb] : buttons_) {
        auto b = element_factory::make<c_button>(label, cb);
        b->set_font(body_font);
        b->set_bounds({0, 0, 0, 26});
        dlg->add_child(b);
    }
    if (on_confirm_) {
        auto b = element_factory::make<c_button>(confirm_label_, on_confirm_);
        b->set_font(body_font);
        b->set_bounds({0, 0, 0, 26});
        dlg->add_child(b);
    }
    if (on_dismiss_) {
        auto b = element_factory::make<c_button>(dismiss_label_, on_dismiss_);
        b->set_font(body_font);
        b->set_bounds({0, 0, 0, 26});
        dlg->add_child(b);
    }

    return dlg;
}

} // namespace GUI
