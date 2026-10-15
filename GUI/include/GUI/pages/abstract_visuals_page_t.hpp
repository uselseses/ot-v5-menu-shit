#pragma once
// =============================================================================
// GUI::abstract_visuals_page_t
//
// Binary ref: sub_1400343D0 @ 0x1400343D0 (constructor used by all three
// role-specific subclasses). Vtable @ 0x1401F5740 in IDB.
//
// Binary ctor signature (from IDA decompile):
//   sub_1400343D0(this+2, parent, label_str, id_letter_str, mode_int, data_ref)
//
// Decompile shows each role-page constructor:
//   - calls the base ctor sub_1400318A0(this, parent, label, id, mode, data)
//   - overrides the vtable to the concrete role class
//   - appends 4 c_colorpicker children: "Visible", "Attachments",
//     "Hidden", "History" each with role-specific default colors.
//
// The reconstruction models this as a c_groupbox subclass — the role name
// becomes the groupbox title, and the four colorpickers sit inside the body.
// =============================================================================

#include <string>
#include <GUI/elements/c_groupbox.hpp>

namespace Renderer { class font; }

namespace GUI {

class abstract_visuals_page_t : public c_groupbox {
public:
    // Binary: sub_1400318A0(this, parent, label, id_letter, mode, data_ref)
    abstract_visuals_page_t(std::string label, std::string id_letter,
                            int mode)
        : c_groupbox{std::move(label)},
          id_letter_{std::move(id_letter)},
          mode_{mode}
    {}

    std::string_view type_name() const override { return ", AbstractVisualsPage"; }

    // The single-letter id ("A" / "B" / "C") used by config persistence.
    const std::string& id_letter() const noexcept { return id_letter_; }
    int mode() const noexcept { return mode_; }

protected:
    // Helper: append a colorpicker with default ARGB pack.
    void add_color_field(const Renderer::font* font,
                         std::string label,
                         std::uint32_t default_argb);

    // Per binary sub_1400318A0: the abstract base populates four section
    // labels (Bounding box / Health / Skeleton / Weapons) plus an ESP-icons
    // multi-select of 18 entries. Concrete subclasses (enemy/team/self)
    // then layer their own four colorpickers on top.
    void populate_shared_sections(const Renderer::font* font);

    // Per binary `chams_compound_widget__ctor @ 0x1400337B0`: build a
    // compound widget = label + "Style" sub-label + dropdown of 6 chams
    // styles + 2 colorpickers (primary, secondary). Used 4× by each
    // concrete role class for Visible / Attachments / Hidden / History.
    void add_chams_compound(const Renderer::font* font,
                            std::string label,
                            std::uint32_t primary_argb,
                            std::uint32_t secondary_argb);

    std::string id_letter_;
    int         mode_ = 0;
};

} // namespace GUI
