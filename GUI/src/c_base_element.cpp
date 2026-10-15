// =============================================================================
// c_base_element.cpp --- base tree walker.
//
// Binary architecture (from IDA analysis):
// - Children read parent->next_y (+60) for their Y position
// - Parent updates next_y after each child: next_y = child.y + child.h + gap
// - update() computes layout, NOT input handling
// =============================================================================

#include <GUI/elements/c_base_element.hpp>
#include <GUI/input/input_state.hpp>

#include <Renderer/c_renderer_core.hpp>

namespace GUI {

std::shared_ptr<c_base_element>
c_base_element::add_child(std::shared_ptr<c_base_element> child)
{
    child->set_parent(weak_from_this());
    child->recompute_hash(hash_);
    children_.push_back(child);
    return child;
}

bool c_base_element::hit_test(float px, float py) const
{
    const auto& r = (hit_rect_.w > 0 && hit_rect_.h > 0) ? hit_rect_ : bounds_;
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

void c_base_element::render(Renderer::c_renderer_core& r)
{
    if (!visible_) return;
    for (auto& child : children_) {
        if (child) child->render(r);
    }
}

} // namespace GUI
