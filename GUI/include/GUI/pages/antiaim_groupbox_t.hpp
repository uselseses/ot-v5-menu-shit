#pragma once
// =============================================================================
// GUI::antiaim_groupbox_t
//
// One c_groupbox per antiaim_condition_e (Global/Stand/Move/Duck/Air/
// SlowWalk/Fakeduck). All sibling groupboxes share the same set of
// controls; only the one whose condition matches the
// "Move condition" dropdown's current value is visible per frame.
//
// RTTI signature (recovered):
//   .?AV<lambda_1>@?1???0antiaim_groupbox_t@GUI@@QEAA@
//       AEBV?$basic_string@..@std@@
//       AEBV?$shared_ptr@Vc_base_element@GUI@@@4@
//       AEBV?$shared_ptr@Vc_dropdown@GUI@@@4@
//       W4antiaim_condition_e@2@@Z
//
// → ctor(string label, shared_ptr<c_base_element> parent,
//        shared_ptr<c_dropdown> condition_dropdown,
//        antiaim_condition_e condition)
// =============================================================================

#include <memory>

#include <GUI/elements/c_groupbox.hpp>
#include <GUI/enums/antiaim_condition_e.hpp>

namespace Renderer { class font; }

namespace GUI {

class c_dropdown;

class antiaim_groupbox_t : public c_groupbox {
public:
    antiaim_groupbox_t(std::string                  label,
                       std::shared_ptr<c_base_element> parent,
                       std::shared_ptr<c_dropdown>     condition_dropdown,
                       antiaim_condition_e          condition,
                       const Renderer::font*        body_font);

    // Visibility is computed lazily from the dropdown, so c_page
    // can skip invisible siblings during layout without a
    // chicken-and-egg with update().
    bool visible() const noexcept override;
    void update(float dt) override;

private:
    std::shared_ptr<c_dropdown> condition_dropdown_;
    antiaim_condition_e         condition_;
};

} // namespace GUI
