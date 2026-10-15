// =============================================================================
// demo/window.cpp --- reproduces sub_1400074D0 (see analysis/dec_WndProc_or_CreateWnd.c).
//
// Notable details kept 1:1 with the original:
//   - per-monitor v2 DPI awareness via SetThreadDpiAwarenessContext(-2)
//   - class name L"sandbox_window", style CS_DBLCLKS (== 64 in the decompile)
//   - window rect scaled by `min(width, ...) / 1920` --- the original
//     literally uses width/1920 alone, so we keep the same math
//   - both window and console titles set to L"GUI v0.4.0 (64-bit, DirectX 11)"
//   - console placed top-left, sized to (2*sw/5, 3*sh/4)
//   - ShowWindow(SW_SHOWDEFAULT=10), UpdateWindow
// =============================================================================

#include "window.hpp"

#include <GUI/input/input_state.hpp>

#include <cstdio>      // freopen_s, FILE, stdin/stdout/stderr
#include <windowsx.h>  // GET_X_LPARAM / GET_Y_LPARAM

namespace demo {

namespace {

inline void update_mouse_pos(LPARAM lp) {
    GUI::input::current().mouse_pos = {
        static_cast<float>(GET_X_LPARAM(lp)),
        static_cast<float>(GET_Y_LPARAM(lp)),
    };
}

inline void on_mouse_button(LPARAM lp, int btn, bool down) {
    // Mouse button messages also carry the cursor position. Reading
    // it here closes the gap where WM_MOUSEMOVE wasn't synthesized
    // between SetCursorPos + mouse_event (which breaks hit-testing
    // for synthetic click sequences and rapid clicks).
    update_mouse_pos(lp);
    auto& s = GUI::input::current();
    if (down && !s.mouse_down[btn]) {
        s.mouse_pressed    [btn] = true;
        s.mouse_pressed_raw[btn] = true;
    }
    if (!down && s.mouse_down[btn])     s.mouse_released[btn] = true;
    s.mouse_down[btn] = down;
}

} // anon

LRESULT CALLBACK sandbox_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto& in = GUI::input::current();

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        // Record only — the render loop owns all D3D work. Coalesces
        // naturally: the last WM_SIZE before the frame wins. Covers
        // SIZE_MAXIMIZED too (only SIZE_MINIMIZED is skipped).
        if (wParam != SIZE_MINIMIZED &&
            LOWORD(lParam) > 0 && HIWORD(lParam) > 0) {
            g_resize_w       = LOWORD(lParam);
            g_resize_h       = HIWORD(lParam);
            g_resize_pending = true;
        }
        return 0;

    case WM_ERASEBKGND:
        // We paint every pixel via the D3D swapchain. Let GDI erase the
        // exposed area during a maximize/resize and it flashes the desktop
        // through (read as "blue under it" mid-resize, user 2026-06-11).
        // Returning non-zero tells Windows the background is handled.
        return 1;

    case WM_MOUSEMOVE:    update_mouse_pos(lParam); return 0;

    case WM_LBUTTONDOWN: SetCapture(hWnd); on_mouse_button(lParam, GUI::input::mb_left,   true);  return 0;
    case WM_LBUTTONUP:                     on_mouse_button(lParam, GUI::input::mb_left,   false); ReleaseCapture(); return 0;
    case WM_RBUTTONDOWN:                   on_mouse_button(lParam, GUI::input::mb_right,  true);  return 0;
    case WM_RBUTTONUP:                     on_mouse_button(lParam, GUI::input::mb_right,  false); return 0;
    case WM_MBUTTONDOWN:                   on_mouse_button(lParam, GUI::input::mb_middle, true);  return 0;
    case WM_MBUTTONUP:                     on_mouse_button(lParam, GUI::input::mb_middle, false); return 0;

    case WM_MOUSEWHEEL:
        in.wheel_delta += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wParam < 256) {
            if (!in.key_down[wParam]) in.key_pressed[wParam] = true;
            in.key_down[wParam] = true;
        }
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wParam < 256) {
            in.key_down[wParam] = false;
            in.key_released[wParam] = true;
        }
        return 0;

    case WM_CHAR:
        if (wParam >= 0x20 && wParam != 0x7F)  // printable
            in.chars_typed.push_back(static_cast<wchar_t>(wParam));
        return 0;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

static void attach_console()
{
    if (!AllocConsole())
        return;

    // Redirect CRT stdio handles to the new console (same call sequence
    // as the original: CONIN$, CONOUT$, CONOUT$ for stdin/stderr/stdout).
    FILE* dummy = nullptr;
    _wfreopen_s(&dummy, L"CONIN$",  L"r", stdin);
    _wfreopen_s(&dummy, L"CONOUT$", L"w", stderr);
    _wfreopen_s(&dummy, L"CONOUT$", L"w", stdout);

    // Enable ANSI/VT escape processing (ENABLE_VIRTUAL_TERMINAL_PROCESSING).
    HANDLE std_out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode    = 0;
    GetConsoleMode(std_out, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;   // == 4
    SetConsoleMode(std_out, mode);

    SetConsoleTitleW(k_window_title);
}

HWND open_window(HINSTANCE hInstance)
{
    // Per-monitor v2 DPI awareness --- matches the original calls.
    // SetThreadDpiHostingBehavior(DPI_HOSTING_BEHAVIOR_MIXED == 1)
    SetThreadDpiHostingBehavior(DPI_HOSTING_BEHAVIOR_MIXED);
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);
    const float scale  = static_cast<float>(screen_w) / 1920.0f;

    WNDCLASSEXW wc      = {};
    wc.cbSize           = sizeof(wc);
    wc.style            = CS_DBLCLKS;          // == 64
    wc.lpfnWndProc      = sandbox_wndproc;
    wc.hInstance        = hInstance ? hInstance : GetModuleHandleW(nullptr);
    wc.lpszClassName    = k_window_class;
    // Black background brush so any pre-first-frame / resize-exposed area
    // paints black instead of flashing the desktop blue (user 2026-06-11).
    wc.hbrBackground    = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassExW(&wc);

    const int x = static_cast<int>(scale *  50.0f);
    const int y = static_cast<int>(scale *  50.0f);
    const int w = screen_w - static_cast<int>(scale * 100.0f);
    const int h = screen_h - static_cast<int>(scale * 100.0f);

    HWND hWnd = CreateWindowExW(
        0,
        k_window_class,
        nullptr,                               // title set below
        WS_OVERLAPPEDWINDOW,                   // == 0xCF0000
        x, y, w, h,
        nullptr, nullptr,
        wc.hInstance,
        nullptr);

    if (!hWnd)
        return nullptr;

    SetWindowTextW(hWnd, k_window_title);

    attach_console();

    HWND console_hwnd = GetConsoleWindow();
    if (console_hwnd) {
        SetWindowPos(
            console_hwnd, nullptr,
            static_cast<int>(scale * 25.0f), static_cast<int>(scale * 25.0f),
            2 * (screen_w / 5), 3 * (screen_h / 4),
            0);
    }

    ShowWindow(hWnd, SW_SHOWDEFAULT);          // == 10
    UpdateWindow(hWnd);
    return hWnd;
}

} // namespace demo
