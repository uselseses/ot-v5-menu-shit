#pragma once
// =============================================================================
// GUI::c_control_interface --- the "AbstractControl interface" added in
// the Jul 28 changelog.
//
// Every value-bearing widget inherits from this. The interface gives:
//   - typed `serialize(...)` / `deserialize(...)` hooks (used by the
//     cloud config system, JSON-backed)
//   - the on-change callback list (Jul 12-14: "Ability to add on-change
//     hooks to a control element (should be used for scripting API)")
//
// Non-control elements (label, separator, body, column, ...) inherit
// directly from c_base_element and skip the interface entirely. Hash
// allocation likewise --- needs_hash() returns false in those.
// =============================================================================

#include <functional>
#include <memory>
#include <vector>

#include <GUI/elements/c_base_element.hpp>

// Forward-declare nlohmann::json without dragging the header in --- the
// concrete include comes in with Cloud.
namespace nlohmann { class json; }

namespace GUI {

class c_control_interface : public c_base_element {
public:
    bool needs_hash() const override { return true; }

    // --- on-change hooks ---
    using on_change_fn = std::function<void()>;

    void add_on_change(on_change_fn fn) {
        on_change_.push_back(std::move(fn));
    }

    // --- serialization (filled in Phase 7 with nlohmann::json) ---
    virtual void serialize  (nlohmann::json& out) const { (void)out; }
    virtual void deserialize(const nlohmann::json& in)  { (void)in;  }

protected:
    using c_base_element::c_base_element;

    // Subclasses call this from their set() to fan out to listeners.
    void fire_on_change() const {
        for (auto& fn : on_change_) if (fn) fn();
    }

private:
    std::vector<on_change_fn> on_change_;
};

} // namespace GUI
