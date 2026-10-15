#pragma once
// =============================================================================
// GUI::c_dialog_window — modal popup window
//
// Vtable: 0x1401F71A8. Enrolled on overlay queue. Render @ 0x14005E120.
//
// Binary truth (verified via MCP 2026-05-31):
//   * The render function has ZERO ARGB literals and ZERO vt[+N] calls
//     of its own — it is a *composite* of child widgets, not a chrome
//     drawer.
//   * `create_internal_elements` builds two `c_button` children with
//     labels "Confirm" (string @ 0x1401F7028) and "Dismiss" (string @
//     0x1401F7030) via the factory `sub_14005F3C0` — the same
//     allocator pattern as the Cloud groupbox button builders
//     (cloud_save_button_builder etc., see [[config-system]]).
//   * Dialogs are registered in a global FNV-1a hashed map keyed by
//     name — `dialog_window_map_find @ 0x1400545F0`. The hash basis is
//     `0xCBF29CE484222325` (the canonical FNV-1a 64-bit offset basis).
//   * Right-clicking a c_keybinder chip opens its associated dialog
//     via this map (see [[keybinds-system]]).
//
// Reconstruction adds the Confirm / Dismiss buttons inline (rather
// than via the factory) so the dialog is usable without the full
// shared_ptr<Ref_count_obj2> dance.
//
// Render order in the reconstruction:
//   1. push overlay layer
//   2. fill title bar (0xFF151519)
//   3. fill body  (0xFF232328)
//   4. outline   (0xFF3E3E42)
//   5. title text (0xFFDEDEDE)
//   6. close X (top-right of title bar)
//   7. content children (in clipped body)
//   8. Confirm button (bottom right of body)
//   9. Dismiss button (bottom right - confirm width - gap)
//  10. pop overlay layer
// =============================================================================

#include <GUI/containers/c_base_window.hpp>
#include <GUI/elements/c_button.hpp>
#include <functional>
#include <memory>

namespace Renderer { class font; }

namespace GUI {

class c_dialog_window : public c_base_window {
public:
    explicit c_dialog_window(std::string title)
        : c_base_window{std::move(title)} {}

    std::string_view type_name() const override { return ", Dialog window"; }

    // Modal flag — when true, blocks interaction with parent window
    bool modal() const noexcept { return modal_; }
    void set_modal(bool m) { modal_ = m; }
    void set_title_font(const Renderer::font* f) { title_font_ = f; }
    void set_button_font(const Renderer::font* f) { button_font_ = f; }

    // Button labels — defaults match binary strings "Confirm" / "Dismiss"
    void set_confirm_label(std::string s) { confirm_label_ = std::move(s); }
    void set_dismiss_label(std::string s) { dismiss_label_ = std::move(s); }

    // Single-button mode — when true, hide Confirm and centre Dismiss
    // alone (matches the binary's "Changelogs:" dialog which has only
    // a "Dismiss" pill at the bottom).
    void set_show_confirm(bool v) { show_confirm_ = v; }
    bool show_confirm() const noexcept { return show_confirm_; }

    // Rects computed during layout
    rect_t title_bar_rect()    const noexcept { return title_bar_rect_; }
    rect_t body_rect()         const noexcept { return body_rect_; }
    rect_t close_button_rect() const noexcept { return close_button_rect_; }
    rect_t confirm_button_rect() const noexcept { return confirm_button_rect_; }
    rect_t dismiss_button_rect() const noexcept { return dismiss_button_rect_; }

    // Callbacks
    void set_on_close(std::function<void()> cb)   { on_close_   = std::move(cb); }
    void set_on_confirm(std::function<void()> cb) { on_confirm_ = std::move(cb); }
    void set_on_dismiss(std::function<void()> cb) { on_dismiss_ = std::move(cb); }

    // Convenience open/close — toggles visible_ with smooth animation.
    void open()  {
        visible_ = true;
        closing_ = false;
    }
    void close() {
        closing_ = true;
    }
    bool is_open() const noexcept { return visible_ && !closing_; }

    // Auto-size from child content + button labels. ON by default; turn
    // off if the host needs a fixed dialog (e.g. a chrome-only popup).
    void set_auto_size(bool v) { auto_size_ = v; }
    bool auto_size() const noexcept { return auto_size_; }

    // Fixed size used when auto_size is off. Must survive the parent
    // menu window stomping bounds_ with its full content rect every
    // frame (which is what made dialogs render menu-sized).
    void set_size(float w, float h) { sized_w_ = w; sized_h_ = h; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

    // Override c_base_window's layout — the dialog has NO separate
    // title bar; the title is rendered inline inside the body. So
    // children start below `title_band_h` (font pixel height + 18 px
    // for breathing room) instead of below `sz.title_bar_h` (40 px).
    void layout_children() override;

private:
    bool modal_ = false;  // +320
    const Renderer::font* title_font_  = nullptr;
    const Renderer::font* button_font_ = nullptr;

    rect_t title_bar_rect_{};       // +340..+356
    rect_t body_rect_{};            // +356..+372
    rect_t close_button_rect_{};    // +372..+388
    rect_t confirm_button_rect_{};
    rect_t dismiss_button_rect_{};

    std::string confirm_label_ = "Confirm";  // binary: 0x1401F7028
    std::string dismiss_label_ = "Dismiss";  // binary: 0x1401F7030

    // Dismiss / Confirm rendered as real c_button instances so they
    // share the same render path as menu buttons (user 2026-06-02:
    // "make them use the same one, the right button is the one menu
    // uses"). Lazy-built in update() once the dialog goes visible.
    std::shared_ptr<c_button> dismiss_btn_;
    std::shared_ptr<c_button> confirm_btn_;
    bool buttons_built_ = false;

    std::function<void()> on_close_;
    std::function<void()> on_confirm_;
    std::function<void()> on_dismiss_;

    // Smooth opening / closing transition
    float anim_progress_ = 0.0f;
    bool  closing_       = false;

    // Auto-size from content. When true (default), the dialog
    // recomputes bounds_.w / bounds_.h each frame from its children
    // and the title text width.
    bool auto_size_ = true;
    // Cached previous-frame visibility so we can claim_modal on the
    // VISIBLE transition (not every frame).
    bool was_visible_ = false;
    // True after auto_size has run for the current open. Reset on
    // visible→false so re-opens re-measure.
    bool auto_sized_  = false;
    // Persisted dialog size — written by auto-size or set_size() and
    // re-applied every frame (the parent window overwrites bounds_).
    float sized_w_ = 0.0f;
    float sized_h_ = 0.0f;
    // True = render both Dismiss + Confirm; false = Dismiss-only.
    bool show_confirm_ = true;
};

} // namespace GUI
