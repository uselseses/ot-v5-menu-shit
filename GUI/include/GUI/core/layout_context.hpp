#pragma once
// =============================================================================
// GUI::core::layout_context — element positioning and layer system
//
// KEY INSIGHT from binary: Children read parent->next_y (+60) for their Y,
// NOT parent->bounds.y. Parent updates next_y after each child.
//
// Binary uses:
//   g_current_layout_ctx — set by c_menu_window::update() to `this`
//   g_gui_element_list (0x1402465F8) — per-frame element stack
//   Two-queue render system (qword_1402465D8):
//     +0x30..+0x38 = normal queue (begin/end)
//     +0x48..+0x50 = overlay queue (begin/end)
//     Each entry is 64 bytes, dispatched via vt[+16] on entry+56
//
// See HOW_IT_WORKS.md §2, §4, §7 for details.
// =============================================================================

#include <Renderer/geometry.hpp>
#include <cstdint>
#include <vector>

namespace GUI {

// =============================================================================
// Binary element layout offsets (verified via IDA — HOW_IT_WORKS.md §7)
// =============================================================================
namespace binary_offset {

// c_base_element offsets
constexpr int VTABLE        = 0;
constexpr int PARENT_REF    = 8;    // shared_ptr
constexpr int CHILDREN_BEG  = 24;
constexpr int CHILDREN_END  = 32;
constexpr int NEXT_Y        = 60;   // float — KEY: children read this
constexpr int XOR_KEY       = 65;   // 3-byte XOR key for label
constexpr int HIDDEN        = 68;   // byte
constexpr int HOVERED       = 75;   // byte
constexpr int CLICKED       = 76;   // byte
constexpr int HIT_RECT      = 80;   // 16-byte rect (x,y,w,h)
constexpr int BOUNDS_X      = 96;   // i32
constexpr int BOUNDS_Y      = 100;  // i32
constexpr int BOUNDS_W      = 104;  // i32
constexpr int BOUNDS_H      = 108;  // i32
constexpr int ANIM_STATE    = 112;  // 24 bytes — per-element lerp state
constexpr int ELEMENT_FLAGS = 160;  // i32
constexpr int PARENT_PTR    = 168;  // raw ptr
constexpr int PARENT_REF2   = 176;  // shared_ptr
constexpr int SIBLING_MARG  = 184;  // i32
constexpr int LABEL         = 188;  // 64-byte XOR-encrypted string
constexpr int VIS_CTRL      = 312;  // ptr — visibility controller
constexpr int TOGGLE_STATE  = 320;  // byte or temp_rect
constexpr int BOX_X         = 340;  // i32 — checkbox/colorpicker box position
constexpr int BOX_Y         = 344;  // i32
constexpr int INTERACTIVE   = 346;  // byte
constexpr int VIS_CHILD_CNT = 364;  // i32 — visible child count

// c_menu_window specific (+base offsets)
constexpr int MW_MODAL      = 504;  // qword — skip close if set
constexpr int MW_DLG_BEG    = 536;  // ptr — dialog stack begin
constexpr int MW_DLG_END    = 544;  // ptr — dialog stack end
constexpr int MW_CONTENT    = 624;  // 16-byte rect (content area)
constexpr int MW_HAS_FOCUS  = 640;  // byte
constexpr int MW_RESIZING   = 696;  // byte
constexpr int MW_FOOTER     = 724;  // 16-byte rect (footer area)

// c_checkbox specific
constexpr int CB_DISABLED   = 336;  // byte
constexpr int CB_BOX_X      = 340;  // i32
constexpr int CB_BOX_Y      = 344;  // i32

// c_slider specific
constexpr int SL_FILL_PCT   = 152;  // f32
constexpr int SL_FORMAT     = 336;  // format_t
constexpr int SL_HAS_SUFFIX = 400;  // byte
constexpr int SL_SUFFIX     = 401;  // char[64] XOR
constexpr int SL_VALUE      = 472;  // i32
constexpr int SL_HOVER      = 476;  // byte
constexpr int SL_PERCENT    = 477;  // byte — multiply by 100 for display
constexpr int SL_TRACK      = 480;  // 16-byte rect
constexpr int SL_FILL       = 496;  // 16-byte rect
constexpr int SL_FILL_W     = 504;  // i32

// c_colorpicker specific
constexpr int CP_POPUP_OPEN = 68;   // byte
constexpr int CP_HIT_RECT   = 80;   // 16-byte rect (= swatch rect)
constexpr int CP_SWATCH     = 332;  // 16-byte rect
constexpr int CP_HAS_ALPHA  = 480;  // byte
constexpr int CP_POPUP_RECT = 488;  // x,y at +488,+492
constexpr int CP_SV_RECT    = 504;  // 16-byte rect
constexpr int CP_HUE_RECT   = 520;  // 16-byte rect
constexpr int CP_ALPHA_RECT = 536;  // 16-byte rect

// c_groupbox specific
constexpr int GB_COLLAPSED  = 320;  // byte
constexpr int GB_SHOW_TITLE = 344;  // byte
constexpr int GB_SHOW_BORD  = 345;  // byte
constexpr int GB_SHOW_CHEV  = 346;  // byte
constexpr int GB_CONTENT    = 348;  // 16-byte rect

// c_listbox specific
constexpr int LB_HIDDEN     = 400;  // byte
constexpr int LB_ACCENT_STRIP = 496; // 16-byte rect
constexpr int LB_STRIP_W    = 504;   // i32 — when >0, draw accent strip

// c_textbox specific
constexpr int TB_VALUE      = 32;   // 256-byte XOR-encrypted value
constexpr int TB_VALUE_KEY  = 289;  // 3-byte key at +32+257+(i%3)

} // namespace binary_offset

// =============================================================================
// Layer system — render order (two-queue system)
// =============================================================================
enum class render_layer : int {
    background = 0,   // shadows, backgrounds
    normal     = 1,   // main widget tree (queue at +0x30)
    overlay    = 2,   // popups, dropdowns, context menus, dialogs (queue at +0x48)
    tooltip    = 3,   // tooltips (highest)
};

// Queue entry size in binary: 64 bytes
// entry+56 holds `this` pointer, dispatched via vt[+16]
constexpr int QUEUE_ENTRY_SIZE = 64;
constexpr int QUEUE_ENTRY_THIS = 56;

namespace core {

struct element_entry {
    void* element = nullptr;
    bool click_consumed = false;
    bool click_available = false;
};

struct layout_context {
    // Current parent bounds — elements read from parent->bounds
    int parent_x = 0;
    int parent_y = 0;
    int parent_w = 0;
    int parent_h = 0;

    // KEY: parent->next_y — children read this for their Y position
    // Parent updates this after each child: next_y = child.y + child.h + gap
    float next_y = 0;

    // Input state (set once per frame from input::current())
    Renderer::vec2 mouse_pos{};
    bool mouse_down = false;
    bool mouse_pressed = false;
    bool mouse_released = false;

    // Click handling — matches binary's stack approach
    bool click_pending = false;
    bool click_consumed = false;

    // Element stack for nested click handling
    std::vector<element_entry> element_stack;

    void push_element(void* elem) {
        element_stack.push_back({elem, false, mouse_pressed && !click_consumed});
    }

    void pop_element() {
        if (!element_stack.empty()) element_stack.pop_back();
    }

    bool can_consume_click() const {
        return mouse_pressed && !click_consumed;
    }

    void consume_click() {
        click_consumed = true;
    }

    void begin_frame(const Renderer::vec2& mouse, bool down, bool pressed, bool released) {
        mouse_pos = mouse;
        mouse_down = down;
        mouse_pressed = pressed;
        mouse_released = released;
        click_pending = false;
        click_consumed = false;
        element_stack.clear();
    }

    void set_parent_bounds(int x, int y, int w, int h) {
        parent_x = x;
        parent_y = y;
        parent_w = w;
        parent_h = h;
        next_y = static_cast<float>(y);
    }

    // Advance next_y after laying out a child
    void advance_y(float child_h, float gap) {
        next_y += child_h + gap;
    }
};

inline layout_context& current_layout() {
    static layout_context ctx;
    return ctx;
}

} // namespace core
} // namespace GUI
