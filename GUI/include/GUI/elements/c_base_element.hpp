#pragma once
// =============================================================================
// GUI::c_base_element — root of every UI object
//
// BINARY LAYOUT (verified via IDA analysis — see HOW_IT_WORKS.md §7):
//   +0    = vtable ptr
//   +8-16 = parent ref (shared_ptr, thread-safe)
//   +24   = children.begin()
//   +32   = children.end()
//   +60   = next_y (float) — KEY: children read this for their Y position
//   +65-67 = XOR key for encrypted_string_t at +188
//   +68   = class-specific control mode byte.
//           For stacked controls this usually means inline/sibling layout:
//           skip normal parent-cursor layout and/or suppress the standalone label.
//           Other derived classes reuse the same offset for focus/capture/open state.
//   +75   = hovered (byte)
//   +76   = clicked (byte)
//   +80   = hit_rect (16 bytes: x,y,w,h)
//   +96   = bounds.x (i32)
//   +100  = bounds.y (i32)
//   +104  = bounds.w (i32)
//   +108  = bounds.h (i32)
//   +112-136 = anim_state (per-element color/lerp state)
//   +160  = element_flags (i32)
//   +168  = parent_element (raw ptr)
//   +176  = parent_ref (shared_ptr)
//   +184  = sibling_margin (i32)
//   +188  = label (XOR-encoded encrypted_string_t<64>)
//   +312  = visibility_controller ptr
//   +320  = toggle_state / temp_rect
//   +340  = box_x (i32)
//   +344  = box_y (i32)
//   +346  = is_interactive (byte)
//   +364  = visible_child_count (i32)
//
// KEY INSIGHT: Children read parent->next_y (+60) for their Y position,
// NOT parent->bounds.y. Parent updates next_y after each child.
// =============================================================================

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <GUI/core/hash.hpp>

namespace Renderer { class c_renderer_core; }

namespace GUI {

class c_base_element : public std::enable_shared_from_this<c_base_element> {
public:
    virtual ~c_base_element() = default;

    // --- identity ---
    virtual std::string_view type_name() const = 0;
    virtual bool             needs_hash() const { return true; }

    const std::string& label() const noexcept { return label_; }
    hash_t hash() const noexcept { return hash_; }

    // --- tree ---
    void set_parent(std::weak_ptr<c_base_element> p) { parent_ = std::move(p); }
    std::weak_ptr<c_base_element> parent() const { return parent_; }
    const std::vector<std::shared_ptr<c_base_element>>& children() const { return children_; }
    std::shared_ptr<c_base_element> add_child(std::shared_ptr<c_base_element> child);

    // --- bounds (binary: +96..+108) ---
    struct rect_t { float x = 0, y = 0, w = 0, h = 0; };
    rect_t        bounds() const noexcept { return bounds_; }
    void          set_bounds(rect_t r)    { bounds_ = r; }

    // Hit rect (binary: +80..+96) — clickable area
    rect_t        hit_rect() const noexcept { return hit_rect_; }
    void          set_hit_rect(rect_t r)    { hit_rect_ = r; }

    // KEY: next_y (binary: +60) — children read this for their Y position
    float         next_y() const noexcept { return next_y_; }
    void          set_next_y(float y)      { next_y_ = y; }

    // Extra height (binary: +184) — added based on visibility
    float         extra_height() const noexcept { return extra_height_; }
    void          set_extra_height(float h)     { extra_height_ = h; }

    // --- visibility ---
    virtual bool  visible() const noexcept { return visible_; }
    virtual void  set_visible(bool v)      { visible_ = v; }
    virtual float visibility_fraction() const noexcept { return visible_ ? 1.0f : 0.0f; }

    // --- input state (binary: +75, +76) ---
    bool          hovered() const noexcept { return hovered_; }
    void          set_hovered(bool h)      { hovered_ = h; }
    bool          clicked() const noexcept { return clicked_; }
    void          set_clicked(bool c)      { clicked_ = c; }

    // Binary +68 is not a base "visible/open" flag. For button/checkbox/
    // dropdown/slider-style layout it means inline/sibling mode: the control
    // is owned by a row and should not perform the normal stacked layout.
    bool          inline_layout() const noexcept { return inline_layout_; }
    void          set_inline_layout(bool r)      { inline_layout_ = r; }

    // Back-compat with earlier reconstruction names.
    bool          is_in_row() const noexcept { return inline_layout(); }
    void          set_in_row(bool r)         { set_inline_layout(r); }

    // Is element interactive? (binary: +346)
    bool          is_interactive() const noexcept { return is_interactive_; }
    void          set_interactive(bool i)         { is_interactive_ = i; }

    // --- element_flags (binary: +160) ---
    // Full enum verified via IDA 2026-05-31 — see HOW_IT_WORKS.md §34 gap #2
    enum element_flag : std::uint32_t {
        // Sibling adornment type bits (mutually exclusive)
        ef_keybinder      = 0x08,   // sibling keybind chip
        ef_colorpicker    = 0x10,   // sibling color swatch
        ef_dropdown       = 0x20,   // sibling dropdown arrow
        ef_checkbox       = 0x40,   // sibling checkbox square

        // Scroll/layout modifiers
        ef_scroll_pad_top    = 0x80,    // adds (scrollbar_h + 1) to top padding
        ef_has_label         = 0x100,   // element has label row
        ef_scrollable        = 0x400,   // container is scrollable
        ef_scroll_reduce_h   = 0x1000,  // reduce height by (scrollbar_h - 1)
        ef_popup_child       = 0x4000,  // child of popup element
        ef_no_clip           = 0x8000,  // skip clip rect push
        ef_textbox_mode_a    = 0x10000, // textbox input mode bit 0
        ef_textbox_mode_b    = 0x20000, // textbox input mode bit 1
        ef_groupbox          = 0x40000, // c_groupbox marker — binary: c_groupbox__ctor @ 0x14005FFD0 sets this on +160
        ef_use_label_rect    = 0x80000, // use label bounds for layout
        ef_blocked           = 0x800000,// parent is blocking input
        ef_no_clip_children  = 0x2000000, // don't clip child elements
    };
    std::uint32_t element_flags() const noexcept { return element_flags_; }
    void          set_element_flag(std::uint32_t bit) { element_flags_ |= bit; }
    void          clear_element_flag(std::uint32_t bit) { element_flags_ &= ~bit; }
    bool          has_flag(std::uint32_t bit) const { return (element_flags_ & bit) != 0; }

    // --- virtual lifecycle ---
    virtual void  update(float dt) { (void)dt; }
    virtual void  render(Renderer::c_renderer_core& r);

protected:
    explicit c_base_element(std::string label) : label_{std::move(label)} {
        recompute_hash(k_hash_offset);
    }

    void recompute_hash(hash_t parent_hash) {
        if (needs_hash())
            hash_ = combine_hash(parent_hash, label_);
    }

    bool hit_test(float px, float py) const;

    std::string                                  label_;
    hash_t                                       hash_   {0};
    rect_t                                       bounds_ {};
    rect_t                                       hit_rect_ {};
    float                                        next_y_ {0};      // +60
    float                                        extra_height_ {0}; // +184
    std::weak_ptr<c_base_element>                parent_;
    std::vector<std::shared_ptr<c_base_element>> children_;

    bool          visible_       {true};
    bool          hovered_       {false};
    bool          clicked_       {false};
    bool          inline_layout_ {false};
    bool          is_interactive_{false};
    std::uint32_t element_flags_ {0};
};

} // namespace GUI
