#pragma once
// =============================================================================
// GUI::element_factory --- smart-pointer creation helper.
//
// Jul 12-14 changelog:
//   "Migrated entire framework to smart ptrs (+ element factory)"
//   "Eliminate usage of macros for creating weapon pages"
// Aug 1-2:
//   "simplify element factory"
//
// The factory exists so subclasses can hide their constructors and we
// have a single point where shared_ptr ownership is established
// (matters because c_base_element : enable_shared_from_this --- the
// shared_from_this() call only works once the first shared_ptr exists).
//
// Usage:
//   auto cb = GUI::element_factory::make<c_checkbox>("Enabled", state_ptr);
// =============================================================================

#include <memory>
#include <utility>

namespace GUI {

class c_base_element;

class element_factory {
public:
    template <typename T, typename... Args>
    static std::shared_ptr<T> make(Args&&... args) {
        static_assert(std::is_base_of_v<c_base_element, T>,
                      "element_factory::make can only construct c_base_element subclasses");
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

} // namespace GUI
