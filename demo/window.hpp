#pragma once
// demo::window --- sandbox window creation + attached console.
// 1:1 with sub_1400074D0 in the original (see dec_WndProc_or_CreateWnd.c).

#include <Windows.h>

#include <cstdint>

namespace demo {

inline constexpr wchar_t k_window_class[] = L"sandbox_window";
inline constexpr wchar_t k_window_title[] = L"GUI v0.4.0 (64-bit, DirectX 11)";

// Pending client-area resize from WM_SIZE. The wndproc only records it;
// the render loop applies it once per frame (ResizeBuffers + RTV
// recreate + renderer/on_resize + GUI globals). Zero/minimized sizes
// are never recorded.
inline bool          g_resize_pending = false;
inline std::uint32_t g_resize_w       = 0;
inline std::uint32_t g_resize_h       = 0;

// Creates the sandbox window, attaches a console with VT processing
// enabled, sizes both to fit the primary monitor. Returns the HWND on
// success or nullptr on failure.
HWND open_window(HINSTANCE hInstance);

// WndProc for the sandbox class. Phase 1 only handles WM_DESTROY (post
// WM_QUIT); subsequent phases extend this to forward input to the GUI
// framework.
LRESULT CALLBACK sandbox_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

} // namespace demo
