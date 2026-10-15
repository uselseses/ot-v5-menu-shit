#pragma once
// =============================================================================
// GUI::input::input_state --- per-frame snapshot of mouse/keyboard.
//
// The demo's WndProc translates Win32 messages into this struct; widgets
// read it during update() to detect hover/click. begin_frame() clears
// edge bits (pressed/released this frame, wheel delta, typed chars) so
// they only fire once.
//
// `key_down[256]` is indexed by Win32 VK (matches the keybinder names
// in GUI/input/virtual_key_names.hpp).
// =============================================================================

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include <Renderer/geometry.hpp>

namespace GUI::input {

enum mouse_button : int { mb_left = 0, mb_right = 1, mb_middle = 2, mb_count = 3 };

struct input_state {
    Renderer::vec2  mouse_pos {};
    std::array<bool, mb_count> mouse_down     {};
    std::array<bool, mb_count> mouse_pressed  {};   // edge: up -> down this frame
    // User 2026-06-01: mirror of `mouse_pressed` that is NOT cleared
    // by consume_mouse_press. Popup-opener triggers read this so they
    // can react to a click that another widget already consumed (e.g.
    // a modal owner that consumed an outside-click). Cleared only at
    // begin_frame().
    std::array<bool, mb_count> mouse_pressed_raw{};
    std::array<bool, mb_count> mouse_released {};   // edge: down -> up this frame
    float                      wheel_delta = 0;     // in WHEEL_DELTAs (120 per notch)

    std::array<bool, 256>      key_down     {};
    std::array<bool, 256>      key_pressed  {};
    std::array<bool, 256>      key_released {};

    // Char input (WM_CHAR), consumed by textboxes.
    std::wstring               chars_typed;

    // Modal input owner. When a widget opens a popup (colorpicker /
    // dropdown / context menu) it stores its `this` pointer here so
    // every other widget's update() can early-out instead of reacting
    // to hover/click that visually belongs to the popup. Cleared each
    // frame by begin_frame() and re-claimed during the same frame's
    // widget walk if the popup is still open.
    const void* modal_owner = nullptr;
    // Optional: a callback the owner registers so a NEW claimant can
    // force-close the previous popup in a single frame. Used to make
    // opening one widget (colorpicker B / dropdown / keybinder) close
    // any already-open popup (colorpicker A) automatically.
    std::function<void()> modal_close_fn;

    // Returns true when an OTHER widget owns the modal — widgets
    // should skip their hover/click handlers in this case. The
    // modal-owning widget itself sees `false` (still interactive).
    bool input_blocked_by_other(const void* self) const noexcept {
        return modal_owner != nullptr && modal_owner != self;
    }

    // Plain claim — no displace callback. Backwards-compatible.
    void claim_modal(const void* self) noexcept {
        if (modal_owner == self) return;
        if (modal_close_fn) { auto fn = std::move(modal_close_fn); fn(); }
        modal_owner = self;
        modal_close_fn = nullptr;
    }

    // Claim + register a close callback so future claimants can close
    // this widget's popup before stealing the modal.
    void claim_modal(const void* self, std::function<void()> close) {
        if (modal_owner == self) {
            modal_close_fn = std::move(close);
            return;
        }
        if (modal_close_fn) { auto fn = std::move(modal_close_fn); fn(); }
        modal_owner = self;
        modal_close_fn = std::move(close);
    }

    // Explicit release — used by widgets when their popup closes so
    // other widgets become interactive again on the SAME frame.
    void release_modal(const void* self) noexcept {
        if (modal_owner == self) {
            modal_owner = nullptr;
            modal_close_fn = nullptr;
        }
    }

    // Called once per frame, before the GUI tree update.
    // User 2026-06-01: do NOT clear modal_owner here. Clearing it
    // every frame meant widgets updated EARLIER in the tree walk than
    // the popup-owning widget saw `modal_owner == nullptr` and freely
    // ran hover/click handlers under the open popup. Persisting the
    // owner across frames (until release_modal) closes that hole.
    void begin_frame() {
        mouse_pressed     .fill(false);
        mouse_pressed_raw .fill(false);
        mouse_released    .fill(false);
        key_pressed       .fill(false);
        key_released      .fill(false);
        wheel_delta = 0;
        chars_typed.clear();
    }

    // Consume a mouse click so other elements don't also react to it
    void consume_mouse_press(int btn = mb_left) {
        mouse_pressed[btn] = false;
    }
};

// Process-wide singleton --- one input device tracked per process.
inline input_state& current() {
    static input_state s;
    return s;
}

// Convenience predicates ----------------------------------------------------
inline bool hovered(Renderer::rect r) {
    return r.contains(current().mouse_pos);
}

inline bool clicked(Renderer::rect r, int btn = mb_left) {
    return current().mouse_pressed[btn] && hovered(r);
}

inline bool released(Renderer::rect r, int btn = mb_left) {
    return current().mouse_released[btn] && hovered(r);
}

} // namespace GUI::input
