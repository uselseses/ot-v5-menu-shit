#pragma once
// =============================================================================
// Renderer::clip_stack --- scissor rectangle stack.
//
// Optimization (Jul 1 changelog: "Optimizations to clip rects (renderer)"):
//   - push() intersects the new rect with the current top so primitives
//     are clipped to the cumulative region.
//   - if the resulting rect is empty, it's still pushed (so pop balances)
//     but caller can check top().empty() to skip emit entirely.
// =============================================================================

#include <vector>

#include <Renderer/geometry.hpp>

namespace Renderer {

class clip_stack {
public:
    clip_stack() = default;

    void reset(rect screen) {
        stack_.clear();
        stack_.push_back(screen);
    }

    void push(rect r) {
        const rect intersected = stack_.empty()
            ? r
            : rect::intersect(top(), r);
        stack_.push_back(intersected);
    }

    void pop() {
        if (!stack_.empty()) stack_.pop_back();
    }

    rect top() const {
        return stack_.empty() ? rect{} : stack_.back();
    }

    bool empty_top() const { return top().empty(); }

private:
    std::vector<rect> stack_;
};

} // namespace Renderer
