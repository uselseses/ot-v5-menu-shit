#pragma once
// antiaim_condition_e --- recovered from the Antiaim "Move condition"
// dropdown in build_main_gui_layout (sub_14007BD60) and the
// antiaim_groupbox_t RTTI signature:
//   .?AU?$thread_safe_t@W4antiaim_condition_e@GUI@@…
// String EAs (in layout order):
//   "Global" 0x14007d611, "Stand" 0x14007d625, "Move" 0x14007d639,
//   "Duck"   0x14007d64d, "Air"   0x14007d661, "Slow walk" 0x14007d675,
//   "Fakeduck" 0x14007d689, "Auto direction" 0x14007d69d,
//   "Manual" 0x14007d6b1 — the binary creates NINE condition groupboxes
//   (GUI_64_DX11.exe.c:96500-96580), incl. Auto direction and Manual.
#include <cstdint>

namespace GUI {

enum class antiaim_condition_e : std::uint32_t {
    global         = 0,
    stand          = 1,
    move           = 2,
    duck           = 3,
    air            = 4,
    slow_walk      = 5,
    fakeduck       = 6,
    auto_direction = 7,
    manual         = 8,
};

} // namespace GUI
