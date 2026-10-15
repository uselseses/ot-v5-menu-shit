#pragma once
// =============================================================================
// GUI::legit_weapon_page_t
//
// Per-weapon legit page --- constructor wires the page_manager up with
// a new c_page named after the weapon, then populates it with 18
// controls (matches the binary's RTTI: lambdas 1..18 inside the ctor).
//
// Controls (recovered from analysis/menu_layout_order.txt around the
// Legit -> Weapons subtree):
//   Enabled, Master switch, Smoke check, Ignore Steam friends,
//   Reaction time, Hitboxes, Target priority, Field of view, Deadzone,
//   Pitch smoothing, Yaw smoothing, Silent aim, Silent aim FoV,
//   Backtracking, Maximum time, Maximum distance, Triggerbot, Autowall.
// =============================================================================

#include <memory>
#include <string>

#include <GUI/containers/c_page.hpp>

namespace Renderer { class font; }

namespace GUI {

class c_page_manager;
class c_groupbox;

class legit_weapon_page_t : public c_page {
public:
    legit_weapon_page_t(std::shared_ptr<c_page_manager> page_manager,
                        std::string                     weapon_name,
                        std::string                     subtitle,
                        int                             weapon_id,
                        const Renderer::font*           body_font);

    int weapon_id() const noexcept { return weapon_id_; }

private:
    int weapon_id_;
};

} // namespace GUI
