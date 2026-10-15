#pragma once
// =============================================================================
// GUI::c_base_control<Read, Write>
//
// Templated parent for value-bearing controls. The two type parameters
// match what RTTI shows in the original binary:
//
//   .?AV?$c_base_control@HH@GUI@@   --- <int32, int32>
//   .?AV?$c_base_control@II@GUI@@   --- <uint32, uint32>
//   .?AV?$c_base_control@MM@GUI@@   --- <float, float>
//   .?AV?$c_base_control@NN@GUI@@   --- <double, double>
//   .?AV?$c_base_control@_J_J@GUI@@ --- <int64, int64>
//   .?AV?$c_base_control@_K_K@GUI@@ --- <uint64, uint64>
//   .?AV?$c_base_control@_N_N@GUI@@ --- <bool, bool>
//
// The `Read` type is what consumers see (return type of get()); `Write`
// is what writers send (parameter of set()). They differ in containers
// like multi-dropdown where Read is a bitmask and Write is "toggle this
// index".
//
// Storage is shared with the cheat thread via
// `wm::types::thread_safe_t<Read, Write>` --- the SRWLOCK lives there.
// =============================================================================

#include <memory>
#include <utility>

#include <GUI/core/c_control_interface.hpp>

#include <wm/types/thread_safe_t.hpp>

namespace GUI {

template <typename Read, typename Write = Read>
class c_base_control : public c_control_interface {
public:
    using state_t = wm::types::thread_safe_t<Read, Write>;
    using read_t  = Read;
    using write_t = Write;

    Read get() const {
        return state_ ? state_->get() : Read{};
    }

    void set(Write v) {
        if (!state_) return;
        state_->set(std::move(v));
        fire_on_change();
    }

    const std::shared_ptr<state_t>& state() const noexcept { return state_; }

protected:
    c_base_control(std::string label, std::shared_ptr<state_t> state)
        : c_control_interface{std::move(label)}, state_{std::move(state)} {
        if (!state_) state_ = std::make_shared<state_t>();
    }

    std::shared_ptr<state_t> state_;
};

} // namespace GUI
