#pragma once
// Compact virtual-key codes used by c_keybinder. These match Win32
// VK_* numerically so we can pass values to MapVirtualKeyW directly
// (the original imports both MapVirtualKeyA and MapVirtualKeyW).
//
// The framework treats mouse buttons as additional VKs --- the changelog
// (Jun 29) notes "Fixed M4/M5 buttons", so XBUTTON1/XBUTTON2 are bound
// labels too.
#include <cstdint>

namespace GUI::input {

using virtual_key_t = std::uint16_t;

constexpr virtual_key_t k_none      = 0x00;
constexpr virtual_key_t k_lbutton   = 0x01;
constexpr virtual_key_t k_rbutton   = 0x02;
constexpr virtual_key_t k_cancel    = 0x03;
constexpr virtual_key_t k_mbutton   = 0x04;
constexpr virtual_key_t k_xbutton1  = 0x05; // "Mouse 4"
constexpr virtual_key_t k_xbutton2  = 0x06; // "Mouse 5"
// ...remainder match Win32 VK_* numbers.

} // namespace GUI::input
