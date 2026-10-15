#pragma once
// =============================================================================
// GUI::input::cursor --- system cursor management.
//
// Reversed from sub_14008D6C0 which reads cursor type from
// *(qword_1402465F8 + 24) and calls LoadCursorW/SetCursor.
//
// Cursor IDs map to Windows standard cursors:
//   0 = IDC_ARROW     (32512)
//   1 = IDC_HAND      (32649)
//   2 = IDC_IBEAM     (32513)
//   3 = IDC_SIZEWE    (32646)
//   4 = IDC_SIZENS    (32648)
//   5 = IDC_SIZENESW  (32644)
//   6 = IDC_SIZENWSE  (32645)
//   7 = IDC_SIZEALL   (32643) - move cursor for logo drag
//   8 = IDC_NO        (32642)
// =============================================================================

#include <cstdint>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace GUI::input {

enum class cursor_type : std::uint8_t {
    arrow      = 0,   // default
    hand       = 1,   // clickable (buttons, tabs, logo hover)
    ibeam      = 2,   // text edit (textbox)
    size_we    = 3,   // horizontal resize
    size_ns    = 4,   // vertical resize
    size_nesw  = 5,   // diagonal resize NE-SW
    size_nwse  = 6,   // diagonal resize NW-SE
    size_all   = 7,   // move (logo drag, window drag)
    no         = 8,   // not allowed
};

// Per-frame cursor state. Reset to arrow at frame start; widgets set
// their desired cursor during update(). At frame end, apply_cursor()
// calls SetCursor with the final value.
struct cursor_state {
    cursor_type current = cursor_type::arrow;

    void reset() { current = cursor_type::arrow; }

    void set(cursor_type c) {
        // Higher priority cursors win (resize > hand > arrow)
        if (static_cast<int>(c) > static_cast<int>(current))
            current = c;
    }

#ifdef _WIN32
    void apply() const {
        static HCURSOR cache[9] = {};
        static bool initialized = false;

        if (!initialized) {
            cache[0] = LoadCursorA(nullptr, IDC_ARROW);
            cache[1] = LoadCursorA(nullptr, IDC_HAND);
            cache[2] = LoadCursorA(nullptr, IDC_IBEAM);
            cache[3] = LoadCursorA(nullptr, IDC_SIZEWE);
            cache[4] = LoadCursorA(nullptr, IDC_SIZENS);
            cache[5] = LoadCursorA(nullptr, IDC_SIZENESW);
            cache[6] = LoadCursorA(nullptr, IDC_SIZENWSE);
            cache[7] = LoadCursorA(nullptr, IDC_SIZEALL);
            cache[8] = LoadCursorA(nullptr, IDC_NO);
            initialized = true;
        }

        int idx = static_cast<int>(current);
        if (idx >= 0 && idx < 9 && cache[idx])
            SetCursor(cache[idx]);
    }
#else
    void apply() const {} // no-op on non-Windows
#endif
};

// Global cursor state for this frame
inline cursor_state& cursor() {
    static cursor_state s;
    return s;
}

} // namespace GUI::input
