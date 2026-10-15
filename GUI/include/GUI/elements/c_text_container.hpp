#pragma once
// =============================================================================
// GUI::c_text_container --- wrapped, scrollable text block.
//
// Jul 4-B changelog: "New element: 'CScrollableTextContainer'"
// Jul 8 changelog:   "Added gradient fade to top/bottom of scrollable
//                     elements" (the top/bottom fade is implemented in
//                     c_scrollable_element)
//
// Used for the Display Changelogs button + lorem-ipsum demos. Lines
// pre-wrap on each `set_text()` call (or after a bounds change) by
// word-wrapping to the container's inner width. Vertical scroll via
// the mouse wheel.
//
// Optional header (`set_header("Title")`) renders a single-line
// emphasized line above the wrapped body.
// =============================================================================

#include <string>
#include <vector>

#include <GUI/elements/c_base_element.hpp>

namespace Renderer { class font; class c_renderer_core; }

namespace GUI {

class c_text_container : public c_base_element {
public:
    explicit c_text_container(std::string label = {})
        : c_base_element{std::move(label)} {}

    std::string_view type_name() const override { return ", Text container"; }
    bool             needs_hash() const override { return false; }

    void set_font  (const Renderer::font* f) { font_ = f; rewrap_ = true; }
    void set_text  (std::string t) { text_ = std::move(t); rewrap_ = true; }
    void set_header(std::string h) { header_ = std::move(h); }

    // Frameless: skip the body fill + outline so the text sits directly on
    // the host (e.g. the changelog dialog body) — the original renders the
    // changelog text with no inner box (user 2026-06-11).
    void set_frameless(bool f) { frameless_ = f; }

    // Recessed: fill with the darker inset colour 0xFF19191D (tab-strip /
    // track bg) instead of the body bg. The original changelog text box is
    // recessed inside the dialog body (user 2026-06-11, changelogs.png).
    void set_recessed(bool r) { recessed_ = r; }

    const std::string& text() const noexcept { return text_; }

    void update(float dt) override;
    void render(Renderer::c_renderer_core& r) override;

private:
    void rewrap();

    std::string                  text_;
    std::string                  header_;
    std::vector<std::string>     lines_;     // post-wrap

    const Renderer::font*        font_      = nullptr;
    bool                         rewrap_    = true;
    bool                         frameless_ = false;
    bool                         recessed_  = false;
    float                        last_w_    = 0;

    float                        scroll_y_  = 0;
};

} // namespace GUI
