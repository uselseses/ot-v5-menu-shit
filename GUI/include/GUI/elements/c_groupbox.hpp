#pragma once
// =============================================================================
// GUI::c_groupbox — collapsible container with title bar + animated arrow
//
// RTTI: .?AVc_groupbox@GUI@@. Vtable: 0x1401F7208
// Address: 0x140060700
//
// Binary layout (verified — HOW_IT_WORKS.md §12):
//   +24..32 = children begin/end
//   +188    = title (XOR-encoded)
//   +320    = collapsed flag (byte)
//   +344    = show_title flag (byte)
//   +345    = show_border flag (byte)
//   +346    = show_chevron flag (byte)
//   +348..364 = content_rect (16 bytes)
// =============================================================================

#include <functional>
#include <memory>

#include <GUI/elements/c_base_element.hpp>

namespace Renderer { class font; }

namespace GUI {

class c_groupbox : public c_base_element {
public:
    explicit c_groupbox(std::string label) : c_base_element{std::move(label)} {
        // Binary: c_groupbox__ctor @ 0x14005FFD0 sets element_flags |= 0x40000.
        set_element_flag(ef_groupbox);
        // The chevron-button click handler in c_groupbox::update is
        // gated on `is_interactive_`. The base default is false, so
        // before this line the minimize button never registered
        // clicks — exactly the user-reported bug.
        set_interactive(true);
    }

    std::string_view type_name() const override { return "c_groupbox"; }
    bool             needs_hash() const override { return false; }

    // State (+320)
    bool collapsed() const noexcept { return collapsed_; }
    void set_collapsed(bool c) {
        collapsed_  = c;
        // Programmatic state changes snap, they don't tween.
        anim_lin_   = c ? 0.0f : 1.0f;
        arrow_anim_ = anim_lin_;
    }

    // Visibility flags (+344, +345, +346)
    bool show_title() const { return show_title_; }
    void set_show_title(bool v) { show_title_ = v; }

    bool show_border() const { return show_border_; }
    void set_show_border(bool v) { show_border_ = v; }

    bool show_chevron() const { return show_chevron_; }
    void set_show_chevron(bool v) { show_chevron_ = v; }

    void set_title_font(const Renderer::font* f) { title_font_ = f; }

    // In-groupbox conditional reveal. When a predicate is set, every child
    // EXCEPT the designated master element is shown only while the predicate
    // returns true; otherwise the box collapses to just the master row (it
    // reuses the existing visible-child height sum, so no overlap). Used for
    // the cheat menu's per-feature "Enabled" toggles — when off you see only
    // "Enabled", when on the rest of the controls appear (user 2026-06-11).
    void set_reveal_master(std::shared_ptr<c_base_element> master,
                           std::function<bool()> pred) {
        reveal_master_ = std::move(master);
        reveal_pred_   = std::move(pred);
    }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

    // Content rect (+348..+364) — area inside the border
    rect_t content_rect() const { return content_rect_; }

private:
    // Binary layout mirrors
    // NOTE: binary stores INVERSE — `+320 = is_open` (default 1).
    // Reconstruction uses `collapsed_` for ergonomics; both encode the
    // same visual state.
    bool collapsed_    = false;  // +320  (inverse of binary's is_open byte)
    bool show_title_   = true;   // +344  (binary default 1)
    bool show_border_  = true;   // +345  (binary default 1)
    bool show_chevron_ = true;   // +346  (binary default 1)
    rect_t content_rect_{};      // +348..+364

    // Animation — see [[groupbox-chevron-anim]].
    //   arrow_anim_         : body-height factor, 0..1, drives the
    //                         "groupbox collapses up to the title"
    //                         animation. 1 = fully expanded, 0 = closed.
    //   chevron_rotation_   : current display rotation of the chevron
    //                         (any range — visually mod 360). 0° = arrow
    //                         points DOWN (collapsed), 180° = UP (open).
    //   target_rotation_    : tween target. Incremented by 180° on every
    //                         click so the chevron keeps spinning
    //                         clockwise across toggles (matches binary).
    float arrow_anim_       = 1.0f;
    // Linear tween progress 0..1 (constant speed over ~200 ms, matching
    // the binary's Xtime_get_ticks-based fraction). arrow_anim_ is
    // ease_in_out_cubic(anim_lin_) so it terminates exactly at 0/1 —
    // the old exponential lerp never reached the target, which kept the
    // collapsed box re-measuring its children and dip-bouncing.
    float anim_lin_         = 1.0f;
    // Body height captured when the tween starts, so children reflowing
    // mid-tween can't feed back into the animated height.
    float anim_total_h_       = 0.0f;
    float anim_content_h_     = 0.0f;
    float chevron_hover_anim_ = 0.0f;
    float chevron_rotation_   = 180.0f;  // default state = open
    float target_rotation_  = 180.0f;
    const Renderer::font* title_font_ = nullptr;

    // Conditional-reveal master + predicate (see set_reveal_master).
    std::weak_ptr<c_base_element> reveal_master_;
    std::function<bool()>         reveal_pred_;
};

} // namespace GUI
