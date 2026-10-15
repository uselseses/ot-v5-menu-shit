#pragma once
// =============================================================================
// GUI::menu::build_cheat_menu — single entry-point that builds the
// entire cheat-side menu (Legit / Rage / Visuals / Miscellaneous /
// Settings) into a c_tab_manager.
//
// Structure is the authoritative binary tree decoded via IDA MCP from
// cheat_menu_build @ sub_14007BD60 (label addresses 0x14007BE30..
// 0x14008247A). See `memory/authoritative_binary_menu.md`.
//
// Hosts (demo / production cheat / D3D9 sandbox / …) construct a
// `build_ctx` with their fonts and optional hooks, then call this
// function. The framework demo tabs (renderer / GUI / dev-renderer)
// stay in demo code; only the cheat tabs live here.
// =============================================================================

#include <functional>
#include <memory>
#include <string>

namespace Renderer { class font; struct color; }

namespace GUI {

class c_tab_manager;
class c_keybinder;
namespace input { template <typename T> struct color_holder; }

namespace menu {

class c_settings;

struct build_ctx {
    // Required fonts.
    const Renderer::font* body_font   = nullptr;
    const Renderer::font* title_font  = nullptr;

    // Optional weapon-icon font (rage + legit weapons icon strip).
    // When nullptr, the strip falls back to text labels.
    const Renderer::font* weapon_icon_font = nullptr;

    // Optional indicator-registry callback. The cheat menu creates
    // keybinder widgets for rage / movement / misc that the host
    // surface as a left-edge HUD overlay. Host passes a closure that
    // registers each keybind with its short tag + tint color.
    std::function<void(const std::shared_ptr<c_keybinder>& kb,
                       const char* short_tag,
                       std::uint8_t r, std::uint8_t g,
                       std::uint8_t b, std::uint8_t a)>
        register_indicator_kb;

    // Optional "Open dialog" handler — the demo wires this to a
    // c_dialog_window. Production cheat would wire to its own dialog
    // system. When nullptr, the button is inert.
    std::function<void()> open_dialog;

    // Optional Open-GUI keybinder shared state — if present the
    // Settings > GUI > Open GUI keybinder binds to this.
    std::shared_ptr<void> menu_kb_state;   // c_keybinder::state_t*

    // Optional settings registry — when non-null, every checkbox /
    // slider / dropdown / colorpicker / keybinder binds its state to
    // a path-keyed entry here (so save/load + reset-to-default work).
    // When null, widgets fall back to a process-wide default
    // c_settings (see GUI::menu::default_settings()).
    c_settings* settings = nullptr;
};

// Populate the supplied tab_manager with the 5 cheat-side tabs.
// Does NOT clear existing tabs — call BEFORE adding any other tabs
// if you want them on the left. The framework demo tabs can be added
// after this call.
void build_cheat_menu(c_tab_manager& tabs, const build_ctx& ctx);

} // namespace menu
} // namespace GUI
