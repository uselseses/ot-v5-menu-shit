#pragma once
// =============================================================================
// GUI::focus_state --- one-of-a-kind focus tracker.
//
// Single-focus model: at most one c_base_element holds focus at a
// given time. The changelog records two iterations of this:
//   Jul 15: "Fix element focus bugs"
//   Aug 5-8: "Ask lani about hover / focus behavior (i.e. should hover
//             outline show when another element has focus)"
//           "Fix bugs with element focus system, slightly re-written"
//
// The hover-suppression rule is implemented inside the elements:
// `should_show_hover() == hovered_ && (!focus.is_held() || focus.holder() == self)`.
// =============================================================================

#include <memory>

namespace GUI {

class c_base_element;

class focus_state {
public:
    bool is_held() const { return !holder_.expired(); }

    std::shared_ptr<c_base_element> holder() const { return holder_.lock(); }

    // Returns true if `who` already held focus.
    bool try_grab(const std::shared_ptr<c_base_element>& who) {
        if (auto cur = holder_.lock(); cur == who) return true;
        holder_ = who;
        return false;
    }

    void release(const std::shared_ptr<c_base_element>& who) {
        if (holder_.lock() == who) holder_.reset();
    }

    void clear() { holder_.reset(); }

private:
    std::weak_ptr<c_base_element> holder_;
};

} // namespace GUI
