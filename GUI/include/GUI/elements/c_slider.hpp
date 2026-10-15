#pragma once
// =============================================================================
// GUI::c_slider<T> — numeric range slider
//
// Render: 0x14002AEE0, 0x14004BB80 (extended). Update: 0x14002AFD0
//
// RTTI shows four instantiations:
//   .?AV?$c_slider@HX@GUI@@   — <int>
//   .?AV?$c_slider@MX@GUI@@   — <float>
//   .?AV?$c_slider@NX@GUI@@   — <double>
//   .?AV?$c_slider@_JX@GUI@@  — <int64>
//
// Binary layout (verified — HOW_IT_WORKS.md §16):
//   +152       = fill_percent (f32)
//   +188       = label (XOR-encoded, 64 bytes)
//   +336       = format_t (value formatter)
//   +400       = has_suffix_label flag (byte)
//   +401       = suffix_label (XOR-encoded, 64 bytes)
//   +472       = value (i32)
//   +476       = hover flag (byte)
//   +477       = is_percent flag (byte — multiply by 100 for display)
//   +480..496  = track_rect
//   +496..512  = fill_rect
//   +504       = fill_width (i32 — when >0, draw fill rect)
//
// Render order (verified @ 0x14004BB80):
//   1. Label text left-aligned, color 0xFFBEBEBE
//   2. Value text right-aligned, color 0xFFAAAAAA
//   3. Track rect filled with 0xFF19191D (NOT 0xFF1F1F1D)
//   4. Fill rect with accent_color
//   5. Border outline
//   6. Tooltip on hover (tt_rect, 0xFF333338, tooltip_rounding)
// =============================================================================

#include <string>
#include <type_traits>

#include <GUI/elements/c_base_control.hpp>
#include <GUI/input/input_state.hpp>

namespace Renderer { class font; class c_renderer_core; }

namespace GUI {

template <typename T>
class c_slider : public c_base_control<T, T> {
public:
    using base_t  = c_base_control<T, T>;
    using state_t = typename base_t::state_t;

    c_slider(std::string label, T min_v, T max_v,
             std::shared_ptr<state_t> state = {},
             std::string suffix = {})
        : base_t{std::move(label), std::move(state)},
          min_{min_v}, max_{max_v}, suffix_{std::move(suffix)} {}

    std::string_view type_name() const override {
        if constexpr (std::is_same_v<T, float>)   return ", Slider<f32>";
        if constexpr (std::is_same_v<T, double>)  return ", Slider<f64>";
        if constexpr (std::is_same_v<T, int>)     return ", Slider<i32>";
        if constexpr (std::is_same_v<T, long long>) return ", Slider<i64>";
        return ", Slider";
    }

    void set_font(const Renderer::font* f) { font_ = f; }

    // Opt-in: render value text in accent when the slider sits at its
    // min (the "min damage = 0 → dynamic" behaviour user-spec'd for
    // the Min damage slider only).
    void set_accent_at_min(bool v) noexcept { accent_at_min_ = v; }
    bool accent_at_min() const noexcept { return accent_at_min_; }

    T min() const noexcept { return min_; }
    T max() const noexcept { return max_; }

    // Fill percent (+152)
    float fill_percent() const noexcept { return fill_percent_; }

    // Track rect (+480..+496)
    typename base_t::rect_t track_rect() const noexcept { return track_rect_; }

    // Fill rect (+496..+512)
    typename base_t::rect_t fill_rect() const noexcept { return fill_rect_; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

private:
    T   min_, max_;
    std::string           suffix_;
    const Renderer::font* font_      = nullptr;
    bool                  dragging_  = false;
    float                 hover_anim_ = 0.0f;
    bool                  accent_at_min_ = false;

    float anim_t_                = 0.0f;
    bool  anim_init_             = false;
    bool  is_actively_dragging_  = false;
    float drag_start_x_          = 0.0f;

    // Binary layout
    float fill_percent_ = 0.0f;           // +152
    bool  is_percent_   = false;          // +477
    typename base_t::rect_t track_rect_{}; // +480..+496
    typename base_t::rect_t fill_rect_{};  // +496..+512
};

extern template class c_slider<int>;
extern template class c_slider<float>;
extern template class c_slider<double>;
extern template class c_slider<long long>;

} // namespace GUI
