#pragma once
// =============================================================================
// GUI::dialog_window_builder_t
//
// Fluent builder for c_dialog_window. The binary's RTTI shows 8
// lambdas captured inside it --- one per chainable slot below.
//
// Usage:
//   auto dlg = GUI::dialog_window_builder_t{"Confirm"}
//                .body_text("Delete config \"main\"?")
//                .button("Yes", []{ … })
//                .button("Cancel", []{ … })
//                .build(font);
// =============================================================================

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Renderer { class font; }

namespace GUI {

class c_dialog_window;

class dialog_window_builder_t {
public:
    using callback = std::function<void()>;

    explicit dialog_window_builder_t(std::string title);

    dialog_window_builder_t& header        (std::string h);   // overrides title at top of body
    dialog_window_builder_t& body_text     (std::string t);   // multi-line; \n separates
    dialog_window_builder_t& button        (std::string label, callback on_click);
    dialog_window_builder_t& wrap_width    (float w);         // body wrap width (px)
    dialog_window_builder_t& on_dismiss    (callback fn);
    dialog_window_builder_t& on_confirm    (callback fn);
    dialog_window_builder_t& dismiss_label (std::string l);   // default "Dismiss"
    dialog_window_builder_t& confirm_label (std::string l);   // default "Confirm"

    std::shared_ptr<c_dialog_window> build(const Renderer::font* body_font) const;

private:
    std::string                                    title_;
    std::string                                    header_;
    std::string                                    body_;
    std::vector<std::pair<std::string, callback>>  buttons_;
    float                                          wrap_w_         = 360.0f;
    callback                                       on_dismiss_;
    callback                                       on_confirm_;
    std::string                                    dismiss_label_ = "Dismiss";
    std::string                                    confirm_label_ = "Confirm";
};

} // namespace GUI
