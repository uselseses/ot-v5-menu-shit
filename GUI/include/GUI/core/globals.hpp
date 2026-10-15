#pragma once
// =============================================================================
// GUI::globals — one-shot singletons the framework reads at runtime
//
// Mirrors the global qword_* slots from the binary. See HOW_IT_WORKS.md §2.
//
//   qword_14023A680  -- g_engine (gui_engine_interface*)
//   qword_140245EB8  -- demo_gui_engine_interface* (set by WinMain)
//   qword_140246368  -- gui-scale + lock pair
//   qword_1402465D8  -- render queue base (two-queue system)
//   qword_1402465F8  -- g_gui_element_list (per-frame element list)
// =============================================================================

#include <cstdint>
#include <memory>

namespace GUI {

class gui_engine_interface;
class focus_state;
class c_base_element;

namespace globals {
    // Engine interface (vtable at g_engine @ 0x14023A680)
    inline std::shared_ptr<gui_engine_interface> g_engine;

    // Focus state management
    inline std::shared_ptr<focus_state> g_focus;

    // Backbuffer dimensions (updated each frame via get_viewport_size)
    inline std::uint32_t g_screen_w = 0;
    inline std::uint32_t g_screen_h = 0;

    // GUI scale — quantized to 0.25 steps in [1.0, 2.0]
    // Formula from RendererInit: round(min(h/1080, w/1920) * 4) * 0.25
    inline float g_gui_scale = 1.0f;

    // Current layout context — set at start of c_menu_window::update()
    // c_tab_manager and c_page_manager read this to get bounds
    inline c_base_element* g_current_layout_ctx = nullptr;

    // Frame timing (from Xtime_get_ticks() / 10000)
    inline std::uint64_t g_frame_time_ms = 0;
    inline std::uint64_t g_last_frame_time_ms = 0;

    // Per-frame delta (seconds)
    inline float g_delta_time = 0.0f;

    // Animation speed — formula: 25.0 * (anim_speed + 3)
    // Default anim_speed = 1, so default base = 100
    inline int g_anim_speed = 1;
    inline float g_anim_base = 100.0f;

    // Debug flags
    inline bool g_debug_outlines = false;
    inline bool g_logo_anim_enabled = false;  // starts OFF in binary
}

} // namespace GUI
