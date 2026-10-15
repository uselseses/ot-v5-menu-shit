#pragma once
// =============================================================================
// GUI::configurable_player_model_t
//
// One c_groupbox per chams_style_e (Flat/Textured/Glow/Glow2/Wireframe/
// Pulse/Pearlescent/Phong). Sibling groupboxes share controls; the one
// whose style matches the "Style" dropdown's current value is visible.
//
// RTTI signature (recovered):
//   .?AV<lambda_1>@?1???0configurable_player_model_t@GUI@@QEAA@
//       AEBV?$basic_string@..@std@@
//       I W4chams_style_e@2@
//       AEBV?$shared_ptr@Vc_base_element@GUI@@@4@
//       AEBV?$shared_ptr@Vc_dropdown@GUI@@@4@
//       _K @ Z
//
// → ctor(string label, uint32 ?, chams_style_e style,
//        shared_ptr<c_base_element> parent,
//        shared_ptr<c_dropdown> style_dropdown,
//        uint64 flags)
//
// We use a slightly simplified signature here; the trailing uint32/
// uint64 are reserved for the cheat-side feature flags and ignored
// at the layout level.
// =============================================================================

#include <cstdint>
#include <memory>

#include <GUI/elements/c_groupbox.hpp>
#include <GUI/enums/chams_style_e.hpp>

namespace Renderer { class font; }

namespace GUI {

class c_dropdown;

class configurable_player_model_t : public c_groupbox {
public:
    configurable_player_model_t(std::string                  label,
                                std::shared_ptr<c_base_element> parent,
                                std::shared_ptr<c_dropdown>     style_dropdown,
                                chams_style_e                style,
                                const Renderer::font*        body_font,
                                std::uint64_t                feature_flags = 0);

    bool visible() const noexcept override;
    void update(float dt) override;

private:
    std::shared_ptr<c_dropdown> style_dropdown_;
    chams_style_e               style_;
    std::uint64_t               feature_flags_;
};

} // namespace GUI
