#pragma once
// =============================================================================
// GUI::rage_weapon_page_t
//
// Per-weapon rage page (8 controls — matches the lambda count in the
// binary's ctor RTTI).
//
// Controls (recovered):
//   Hitbox, Multipoint, Minimum damage, Hitchance, Autowall,
//   Auto scope, Auto stop, Auto stop conditions.
// =============================================================================

#include <memory>
#include <string>

#include <GUI/containers/c_page.hpp>

namespace Renderer { class font; }

namespace GUI {

class c_page_manager;

class rage_weapon_page_t : public c_page {
public:
    rage_weapon_page_t(std::shared_ptr<c_page_manager> page_manager,
                       std::string                     weapon_name,
                       std::string                     subtitle,
                       int                             weapon_id,
                       const Renderer::font*           body_font);

    int weapon_id() const noexcept { return weapon_id_; }

private:
    int weapon_id_;
};

} // namespace GUI
