#pragma once
// keybinder_mode_e --- verified against binary's create_context_menu_c_keybinder
// (@ 0x1400485A0). The right-click context menu builds items in this exact
// order, and each item's "selected" flag is `**(DWORD*)(this+408) == idx`.
//
// String pool addresses:
//   "Always" @ 0x1401F668C
//   "Toggle" @ 0x1401F6694
//   "Hold"   @ 0x1401F669C
//   "None"   built inline (no .rdata entry — see ctx menu lambda 4)
#include <cstdint>

namespace GUI {

enum class keybinder_mode_e : std::uint8_t {
    always = 0,
    toggle = 1,
    hold   = 2,
    none   = 3,   // binary's 4th option = disable bind
};

} // namespace GUI
