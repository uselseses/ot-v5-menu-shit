#pragma once
// chams_style_e --- inferred from the "Style" dropdown adjacent to
// "Configuring model" and the RTTI of configurable_player_model_t.
// String EAs: "Wireframe" 0x1401f55b0, "Pulse" 0x1401f55a4,
// "Glow (2)" 0x1401f55c0, "Pearlescent" 0x1401fafd0, "Phong" 0x1401fb008.
//
// Phase-0 ordering is provisional --- to be locked down by reading the
// dropdown's option-string-array initializer in build_main_gui_layout.
#include <cstdint>

namespace GUI {

enum class chams_style_e : std::uint32_t {
    flat        = 0,
    textured    = 1,
    glow        = 2,
    glow_2      = 3, // "Glow (2)"
    wireframe   = 4,
    pulse       = 5,
    pearlescent = 6,
    phong       = 7,
};

} // namespace GUI
