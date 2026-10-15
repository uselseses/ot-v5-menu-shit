#pragma once
// =============================================================================
// virtual_key_names --- 1:1 with the table the original binary uses for
// c_keybinder display.
//
// Source: strings recovered from .rdata around 0x1401d56e0..0x1401d5d48.
// The author's abbreviation style is preserved verbatim ("L Menu",
// "Md Play Pause", "Br. Bk", "Num. 0", "L Thumb Btn", "Nav. View", ...).
//
// Indexed by Win32 VK code (0..255). Unmapped slots fall through to the
// k_unassigned constant. Display labels for letter/digit keys (0..9, A..Z)
// are not in the table --- the renderer uses the character itself.
// =============================================================================

#include <array>
#include <string_view>
#include <cstdint>

#include <GUI/input/virtual_key.hpp>

namespace GUI::input {

inline constexpr std::string_view k_unassigned{"Unassigned"};

inline constexpr auto virtual_key_names = []{
    std::array<std::string_view, 256> a{};
    for (auto& s : a) s = k_unassigned;

    // Control / mouse / IME -----------------------------------------------
    a[0x03] = "Cancel";        // VK_CANCEL
    a[0x05] = "Mouse 4";       // VK_XBUTTON1
    a[0x06] = "Mouse 5";       // VK_XBUTTON2
    a[0x08] = "Backspace";     // VK_BACK
    a[0x09] = "Tab";           // VK_TAB
    a[0x0C] = "Clear";         // VK_CLEAR
    a[0x0D] = "Return";        // VK_RETURN
    a[0x10] = "Shift";         // VK_SHIFT
    a[0x11] = "Control";       // VK_CONTROL
    a[0x12] = "Menu";          // VK_MENU (Alt)
    a[0x13] = "Pause";         // VK_PAUSE
    a[0x14] = "Caps Lock";     // VK_CAPITAL
    a[0x15] = "IME on";        // VK_KANA / VK_HANGUL
    a[0x17] = "Junja";         // VK_JUNJA
    a[0x18] = "Final";         // VK_FINAL
    a[0x19] = "Hanja";         // VK_HANJA / VK_KANJI
    a[0x1A] = "IME Off";       // VK_IME_OFF
    a[0x1B] = "Escape";        // VK_ESCAPE
    a[0x1D] = "Nonconv";       // VK_NONCONVERT
    a[0x1E] = "Accept";        // VK_ACCEPT
    a[0x1F] = "Modechange";    // VK_MODECHANGE

    // Navigation ----------------------------------------------------------
    a[0x20] = "Space";
    a[0x21] = "Prior";         // Page Up
    a[0x22] = "Next";          // Page Down
    a[0x23] = "End";
    a[0x24] = "Home";
    a[0x25] = "Left";
    a[0x26] = "Up";
    a[0x27] = "Right";
    a[0x28] = "Down";
    a[0x29] = "Select";
    a[0x2A] = "Print";
    a[0x2B] = "Execute";
    a[0x2C] = "Snapshot";
    a[0x2D] = "Insert";
    a[0x2E] = "Delete";
    a[0x2F] = "Help";

    // Windows keys --------------------------------------------------------
    a[0x5B] = "L Win.";
    a[0x5C] = "R Win.";
    a[0x5D] = "Apps";
    a[0x5F] = "Sleep";

    // Numpad --------------------------------------------------------------
    a[0x60] = "Num. 0";  a[0x61] = "Num. 1";  a[0x62] = "Num. 2";
    a[0x63] = "Num. 3";  a[0x64] = "Num. 4";  a[0x65] = "Num. 5";
    a[0x66] = "Num. 6";  a[0x67] = "Num. 7";  a[0x68] = "Num. 8";
    a[0x69] = "Num. 9";
    a[0x6A] = "Multiply";
    a[0x6B] = "Add";
    a[0x6C] = "Separator";
    a[0x6D] = "Subtract";
    a[0x6E] = "Decimal";
    a[0x6F] = "Divide";

    // F1..F24 -------------------------------------------------------------
    a[0x70] = "F1";  a[0x71] = "F2";  a[0x72] = "F3";  a[0x73] = "F4";
    a[0x74] = "F5";  a[0x75] = "F6";  a[0x76] = "F7";  a[0x77] = "F8";
    a[0x78] = "F9";  a[0x79] = "F10"; a[0x7A] = "F11"; a[0x7B] = "F12";
    a[0x7C] = "F13"; a[0x7D] = "F14"; a[0x7E] = "F15"; a[0x7F] = "F16";
    a[0x80] = "F17"; a[0x81] = "F18"; a[0x82] = "F19"; a[0x83] = "F20";
    a[0x84] = "F21"; a[0x85] = "F22"; a[0x86] = "F23"; a[0x87] = "F24";

    // Xbox Controller navigation ------------------------------------------
    a[0x88] = "Nav. View";
    a[0x89] = "Nav. Menu";
    a[0x8A] = "Nav. U";
    a[0x8B] = "Nav. D";
    a[0x8C] = "Nav. L";
    a[0x8D] = "Nav. R";
    a[0x8E] = "Nav. Acc.";
    a[0x8F] = "Nav. Can.";

    // Locks / OEM ---------------------------------------------------------
    a[0x90] = "Num. Lk";
    a[0x91] = "Scroll";
    a[0x92] = "Equal";          // VK_OEM_NEC_EQUAL
    a[0x93] = "Masshou";        // VK_OEM_FJ_MASSHOU
    a[0x94] = "Touroku";        // VK_OEM_FJ_TOUROKU

    // Modifier left/right -------------------------------------------------
    a[0xA0] = "L Shift";
    a[0xA1] = "R Shift";
    a[0xA2] = "L Ctrl";
    a[0xA3] = "R Ctrl";
    a[0xA4] = "L Menu";         // Left Alt
    a[0xA5] = "R Menu";         // Right Alt

    // Browser keys --------------------------------------------------------
    a[0xA6] = "Br. Bk";
    a[0xA7] = "Br. Fwd";
    a[0xA8] = "Br. Rfsh";
    a[0xA9] = "Br. Stop";
    a[0xAA] = "Br. Srch";
    a[0xAB] = "Br. Fav";
    a[0xAC] = "Br. Hm";

    // Volume --------------------------------------------------------------
    a[0xAD] = "Vol. Mt";
    a[0xAE] = "Vol. Dn";
    a[0xAF] = "Vol. Up";

    // Media ---------------------------------------------------------------
    a[0xB0] = "Md Nx Track";
    a[0xB1] = "Md Pr Track";
    a[0xB2] = "Md Stop";
    a[0xB3] = "Md Play Pause";
    a[0xB4] = "Ln Mail";
    a[0xB5] = "Ln Md Sel";
    a[0xB6] = "Ln A1";
    a[0xB7] = "Ln A2";

    // Xbox controller shoulders / triggers / dpad / thumbs ----------------
    a[0xC4] = "R Shoulder";
    a[0xC5] = "L Shoulder";
    a[0xC6] = "L Trig.";
    a[0xC7] = "R Trig.";
    a[0xC8] = "D Up";
    a[0xC9] = "D Down";
    a[0xCA] = "D Left";
    a[0xCB] = "D Right";
    a[0xCE] = "L Thumb Btn";
    a[0xCF] = "R Thumb Btn";
    a[0xD0] = "L Thumb U";
    a[0xD1] = "L Thumb D";
    a[0xD2] = "L Thumb R";
    a[0xD3] = "L Thumb L";
    a[0xD4] = "R Thumb U";
    a[0xD5] = "R Thumb D";
    a[0xD6] = "R Thumb R";
    a[0xD7] = "R Thumb L";

    // OEM tail ------------------------------------------------------------
    a[0xE1] = "OEM AX";
    a[0xE2] = "OEM102";
    a[0xE3] = "ICO00";
    a[0xE5] = "Proc. K";
    a[0xE7] = "Packet";
    a[0xE9] = "Reset";
    a[0xEC] = "WS Ctrl";
    a[0xEE] = "CUSEL";
    a[0xF1] = "Finish";
    a[0xF5] = "Backtab";
    a[0xF6] = "Attn";
    a[0xF7] = "CRSEL";
    a[0xF8] = "EXSEL";
    a[0xF9] = "EREOF";
    a[0xFB] = "Zoom";
    a[0xFC] = "NONAME";
    a[0xFE] = "OEM Clear";

    a[0xFF] = "Invalid";
    return a;
}();

} // namespace GUI::input
