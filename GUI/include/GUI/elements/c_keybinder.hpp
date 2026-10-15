#pragma once
// =============================================================================
// GUI::c_keybinder — keybind chip (sibling adornment)
//
// Vtable: 0x1401F6938. Render: 0x140048B10. Layout: 0x140049540
//
// Sibling adornment: attaches to c_label row via element_flag 0x08.
// Renders a small chip on the right showing the bound key name.
// Click -> "listening" state -> next valid keypress captured.
// Right-click opens context menu (Always/Hold/Toggle/Clear).
//
// Binary layout (verified — HOW_IT_WORKS.md §20):
//   +68        = listening flag (byte)
//   +80..96    = hit_rect (chip rect for click detection)
//   +188       = label (XOR-encoded, 64 bytes)
//   +320       = popup_state
//   +340..356  = chip_rect (16 bytes)
//
// Chip positioning (from slot4 @ 0x140049540):
//   chip.w = style.keybind_chip_width (+0x218) — default 48
//   chip.h = style.keybind_chip_height (+0x220) — default 20
//   chip.x = bounds.x + bounds.w - chip.w
//   chip.y = bounds.y
//
// Render order (verified @ 0x140048B10):
//   1. vt[+288] render_filled_rect(chip_rect, 0xFF232328, rounding)
//   2. vt[+296] render_rect_outline(chip_rect) — accent when listening
//   3. resolve VK name via style.vk_name_table[VK] (+0x5E0, 111 entries)
//   4. vt[+472] render_text(chip_center, vk_name,
//                           listening ? accent : 0xFFBEBEBE, ALIGN_CENTER)
//   5. if listening: show "[...]" prompt overlay
// =============================================================================

#include <cstdint>

#include <GUI/elements/c_base_control.hpp>
#include <GUI/enums/keybinder_mode_e.hpp>
#include <GUI/input/virtual_key.hpp>
#include <GUI/containers/c_context_menu_owner.hpp>

#include <Renderer/geometry.hpp>

namespace Renderer { class font; class c_renderer_core; }
namespace GUI::input { struct input_state; }

namespace GUI {

// Packed: low 16 bits = key, next 8 = mode
struct keybind_state {
    input::virtual_key_t  key  = 0;
    keybinder_mode_e      mode = keybinder_mode_e::hold;

    std::uint32_t pack() const noexcept {
        return static_cast<std::uint32_t>(key)
             | (static_cast<std::uint32_t>(mode) << 16);
    }
    static keybind_state unpack(std::uint32_t v) noexcept {
        return { static_cast<input::virtual_key_t>(v & 0xFFFFu),
                 static_cast<keybinder_mode_e>((v >> 16) & 0xFFu) };
    }
};

class c_keybinder : public c_base_control<std::uint32_t, std::uint32_t>,
                    public c_context_menu_owner {
public:
    using state_t = wm::types::thread_safe_t<std::uint32_t, std::uint32_t>;

    c_keybinder(std::string label,
                std::shared_ptr<state_t> state = {},
                bool strict = false)
        : c_base_control{std::move(label), std::move(state)},
          strict_{strict}
    {
        set_element_flag(ef_keybinder);
    }

    std::string_view type_name() const override { return ", Keybinder"; }

    void set_font(const Renderer::font* f) { font_ = f; }
    bool strict() const noexcept { return strict_; }

    keybind_state bind() const { return keybind_state::unpack(get()); }
    void          set_bind(keybind_state b) { set(b.pack()); }
    bool          capturing() const noexcept { return capturing_; }

    // True iff the current bind would fire under the input state
    bool active(const input::input_state& in) const;

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

    // Chip rect (computed during layout, +340..+356)
    rect_t chip_rect() const noexcept { return chip_rect_; }

private:
    void open_context_menu_();

    const Renderer::font* font_ = nullptr;

    // Binary layout mirrors
    bool capturing_     = false;  // +68 listening flag
    // Frame-skip guard so the first frame after click doesn't try to
    // match a key (avoids self-binding the click as a key + gives the
    // user time to release any modifier).
    bool capture_armed_ = false;
    bool strict_        = false;
    bool toggle_latch_  = false;
    bool ctx_open_      = false;  // +320 popup state

    // Chip border accent animations — same model as c_button:
    //   hover_anim_ 0..1  — gray outline -> accent@0x80 on hover.
    //   press_anim_ 0..1  — while the chip is held with LMB, the accent
    //                       alpha ramps further UP toward full.
    // Idle + unhovered + unpressed == flat gray outline (original look).
    bool  pressed_    = false;
    float hover_anim_ = 0.0f;
    float press_anim_ = 0.0f;

    // Chip rect (+340..+356)
    rect_t chip_rect_{};

    // Context menu rect
    Renderer::rect ctx_rect_{};
};

} // namespace GUI
