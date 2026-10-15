#pragma once
// =============================================================================
// GUI::c_listbox_item --- one row inside a c_listbox.
//
// Exists primarily so the RTTI matches the binary's
// `.?AVc_listbox_item@GUI@@` and the `, Listbox item` type_name string
// at 0x1401f6ee8. The listbox draws rows directly today; promoting
// them to child elements is a follow-up that lets each row carry its
// own keybinder / colorpicker etc. for richer multi-cell rows.
// =============================================================================

#include <GUI/elements/c_base_element.hpp>

namespace GUI {

class c_listbox_item : public c_base_element {
public:
    explicit c_listbox_item(std::string label, int index)
        : c_base_element{std::move(label)}, index_{index} {}

    std::string_view type_name() const override { return ", Listbox item"; }
    bool             needs_hash() const override { return false; }

    int index() const noexcept { return index_; }

private:
    int index_;
};

} // namespace GUI
