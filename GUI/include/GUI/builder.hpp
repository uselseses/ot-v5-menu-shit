#pragma once
// =============================================================================
// GUI::builder — fluent helpers for assembling menus without porting the
// full demo's boilerplate. Header-only.
//
// Usage:
//
//   #include <GUI/builder.hpp>
//
//   auto root = GUI::builder::menu("My Menu", &body_font, &tab_font)
//       .tab("Rage", [&](auto& tb) {
//           tb.subtab("Main", [&](auto& pb) {
//               pb.groupbox("Aimbot", [&](auto& g) {
//                   g.checkbox("Enabled")
//                    .slider_i("Hitchance", 0, 100, 72, " %")
//                    .keybinder("Hide shots")
//                    .button("Reset", []{ /* … */ });
//               });
//               pb.groupbox("Accuracy", [&](auto& g) {
//                   g.dropdown("Hitbox",
//                              {"Head","Chest","Stomach","Legs"}, 0);
//                   g.dropdown_multi("Hitboxes",
//                              {"Head","Chest","Pelvis","Legs"},
//                              /*default_mask*/ 0b0101);
//               });
//           });
//       })
//       .build();
//
// The builder methods all return *this so they chain naturally. Each
// underlying widget gets its font, bounds, and add_child wiring done
// for you — values match what the demo's anonymous helpers already do.
//
// Scope: this is a CONVENIENCE layer. It does not replace the raw
// widgets — power users can still `std::make_shared<c_button>(…)` and
// hand-wire whatever they need. The builder just kills the
// 80%-of-cases boilerplate.
// =============================================================================

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <GUI/containers/c_menu_window.hpp>
#include <GUI/containers/c_page.hpp>
#include <GUI/containers/c_page_manager.hpp>
#include <GUI/containers/c_subtab.hpp>
#include <GUI/containers/c_tab.hpp>
#include <GUI/containers/c_tab_manager.hpp>
#include <GUI/core/element_factory.hpp>
#include <GUI/elements/c_button.hpp>
#include <GUI/elements/c_checkbox.hpp>
#include <GUI/elements/c_colorpicker.hpp>
#include <GUI/elements/c_column.hpp>
#include <GUI/elements/c_dropdown.hpp>
#include <GUI/elements/c_groupbox.hpp>
#include <GUI/elements/c_keybinder.hpp>
#include <GUI/elements/c_label.hpp>
#include <GUI/elements/c_listbox.hpp>
#include <GUI/elements/c_separator.hpp>
#include <GUI/elements/c_slider.hpp>
#include <GUI/elements/c_textbox_new.hpp>

#include <Renderer/color.hpp>

namespace Renderer { class font; }

namespace GUI::builder {

// ---------------------------------------------------------------------------
// Forward decls
// ---------------------------------------------------------------------------
class tab_builder;
class page_builder;
class group_builder;

// ===========================================================================
// group_builder — adds widgets to a `c_groupbox`. Returned by
// `page_builder::groupbox(...)`.
// ===========================================================================
class group_builder {
public:
    group_builder(std::shared_ptr<c_groupbox> gb,
                  const Renderer::font* font)
        : gb_(std::move(gb)), font_(font) {}

    // ---- atoms ------------------------------------------------------------
    group_builder& checkbox(std::string label, bool default_value = false)
    {
        auto st = std::make_shared<c_checkbox::state_t>(default_value);
        auto cb = std::make_shared<c_checkbox>(std::move(label), st);
        cb->set_font(font_);
        gb_->add_child(cb);
        last_checkbox_ = cb;
        return *this;
    }

    group_builder& slider_i(std::string label, int min, int max,
                            int default_value, std::string suffix = "")
    {
        auto s = element_factory::make<c_slider<int>>(
            std::move(label), min, max, nullptr, std::move(suffix));
        s->set_font(font_);
        s->set_bounds({0, 0, 0, 32});
        s->set(default_value);
        gb_->add_child(s);
        last_slider_i_ = s;
        return *this;
    }

    group_builder& slider_f(std::string label, float min, float max,
                            float default_value, std::string suffix = "")
    {
        auto s = element_factory::make<c_slider<float>>(
            std::move(label), min, max, nullptr, std::move(suffix));
        s->set_font(font_);
        s->set_bounds({0, 0, 0, 32});
        s->set(default_value);
        gb_->add_child(s);
        last_slider_f_ = s;
        return *this;
    }

    group_builder& dropdown(std::string label,
                            std::vector<std::string> options,
                            int default_idx = 0)
    {
        auto d = element_factory::make<c_dropdown>(
            std::move(label), std::move(options));
        d->set_font(font_);
        d->set_bounds({0, 0, 0, 36});
        d->set(default_idx);
        gb_->add_child(d);
        last_dropdown_ = d;
        return *this;
    }

    group_builder& dropdown_multi(std::string label,
                                  std::vector<std::string> options,
                                  std::uint64_t default_mask = 0)
    {
        auto d = element_factory::make<c_dropdown>(
            std::move(label), std::move(options));
        d->set_font(font_);
        d->set_bounds({0, 0, 0, 36});
        d->set_multi_select(true);
        for (int i = 0; i < 64; ++i) {
            if ((default_mask >> i) & 1ULL) d->toggle_multi(i);
        }
        gb_->add_child(d);
        last_dropdown_ = d;
        return *this;
    }

    group_builder& keybinder(std::string label,
                             std::shared_ptr<c_keybinder::state_t> state = {})
    {
        auto kb = std::make_shared<c_keybinder>(
            std::move(label), std::move(state));
        kb->set_font(font_);
        gb_->add_child(kb);
        last_keybinder_ = kb;
        return *this;
    }

    group_builder& button(std::string label,
                          std::function<void()> on_click = {})
    {
        auto b = std::make_shared<c_button>(
            std::move(label), std::move(on_click));
        b->set_font(font_);
        b->set_bounds({0, 0, 0, 24});
        gb_->add_child(b);
        last_button_ = b;
        return *this;
    }

    group_builder& colorpicker(std::string label,
                               Renderer::color default_color)
    {
        auto cp = element_factory::make<c_colorpicker>(
            std::move(label), default_color);
        cp->set_font(font_);
        gb_->add_child(cp);
        last_colorpicker_ = cp;
        return *this;
    }

    group_builder& separator()
    {
        auto sep = element_factory::make<c_separator>();
        gb_->add_child(sep);
        return *this;
    }

    group_builder& label(std::string text)
    {
        auto l = element_factory::make<c_label>(std::move(text));
        l->set_font(font_);
        gb_->add_child(l);
        return *this;
    }

    group_builder& textbox(std::string label,
                           std::string placeholder = "")
    {
        auto tb = element_factory::make<c_textbox_new>(
            std::move(label), std::move(placeholder));
        tb->set_font(font_);
        gb_->add_child(tb);
        return *this;
    }

    group_builder& listbox(std::string label,
                           std::vector<std::string> options,
                           int show_n_items = 5,
                           bool required = true)
    {
        auto lb = element_factory::make<c_listbox>(
            std::move(label), std::move(options),
            show_n_items, required);
        lb->set_font(font_);
        gb_->add_child(lb);
        return *this;
    }

    // ---- escape hatches ---------------------------------------------------
    // Add any pre-built widget directly to the groupbox.
    template <class T>
    group_builder& add(std::shared_ptr<T> w) {
        gb_->add_child(std::move(w));
        return *this;
    }

    // Run a free-form callback against the underlying groupbox for the
    // rare case the user wants to do something the helpers don't cover.
    group_builder& with(std::function<void(c_groupbox&)> f) {
        if (f) f(*gb_);
        return *this;
    }

    // Attach a callback to the LAST added widget (when applicable). The
    // returned ptrs let the caller wire on_change handlers cleanly:
    //   pb.groupbox("X", [&](auto& g){
    //       g.checkbox("Toggle").on_change([&]{ ... });
    //   });
    group_builder& on_change(std::function<void()> f) {
        if (last_checkbox_)    last_checkbox_  ->add_on_change(f);
        else if (last_slider_i_) last_slider_i_->add_on_change(f);
        else if (last_slider_f_) last_slider_f_->add_on_change(f);
        else if (last_dropdown_) last_dropdown_->add_on_change(f);
        else if (last_keybinder_)last_keybinder_->add_on_change(f);
        else if (last_colorpicker_) last_colorpicker_->add_on_change(f);
        return *this;
    }

    std::shared_ptr<c_groupbox> groupbox() const { return gb_; }

    // Accessors for the most-recently-added widget of each type, for
    // callers who need a handle past the chain.
    std::shared_ptr<c_checkbox>      last_checkbox()    const { return last_checkbox_; }
    std::shared_ptr<c_slider<int>>   last_slider_i()    const { return last_slider_i_; }
    std::shared_ptr<c_slider<float>> last_slider_f()    const { return last_slider_f_; }
    std::shared_ptr<c_dropdown>      last_dropdown()    const { return last_dropdown_; }
    std::shared_ptr<c_keybinder>     last_keybinder()   const { return last_keybinder_; }
    std::shared_ptr<c_button>        last_button()      const { return last_button_; }
    std::shared_ptr<c_colorpicker>   last_colorpicker() const { return last_colorpicker_; }

private:
    std::shared_ptr<c_groupbox> gb_;
    const Renderer::font*       font_;
    // Track the most recently added widget so `on_change` / `as<>()`
    // helpers can attach callbacks without an external handle.
    std::shared_ptr<c_checkbox>      last_checkbox_;
    std::shared_ptr<c_slider<int>>   last_slider_i_;
    std::shared_ptr<c_slider<float>> last_slider_f_;
    std::shared_ptr<c_dropdown>      last_dropdown_;
    std::shared_ptr<c_keybinder>     last_keybinder_;
    std::shared_ptr<c_button>        last_button_;
    std::shared_ptr<c_colorpicker>   last_colorpicker_;
};

// ===========================================================================
// page_builder — wraps the page in a c_column and adds groupboxes via a
// `groupbox(title, body)` call. `body` is invoked with a `group_builder`.
// ===========================================================================
class page_builder {
public:
    page_builder(std::shared_ptr<c_page> page,
                 const Renderer::font* font,
                 int columns = 2)
        : page_(std::move(page)), font_(font)
    {
        cols_ = element_factory::make<c_column>(columns);
        page_->add_child(cols_);
    }

    template <class Body>
    page_builder& groupbox(std::string title, Body&& body,
                           float height_hint = 0.0f)
    {
        auto gb = element_factory::make<c_groupbox>(std::move(title));
        gb->set_title_font(font_);
        if (height_hint > 0.0f) gb->set_bounds({0, 0, 0, height_hint});
        cols_->add_child(gb);
        group_builder gbuild{gb, font_};
        body(gbuild);
        return *this;
    }

    // Add a pre-built element directly to the column (e.g. an icon strip
    // that sits above the groupboxes).
    template <class T>
    page_builder& add(std::shared_ptr<T> w) {
        page_->add_child(std::move(w));
        return *this;
    }

    page_builder& columns(int n) {
        cols_ = element_factory::make<c_column>(n);
        page_->add_child(cols_);
        return *this;
    }

    std::shared_ptr<c_page>   page() const { return page_; }
    std::shared_ptr<c_column> column() const { return cols_; }

private:
    std::shared_ptr<c_page>   page_;
    std::shared_ptr<c_column> cols_;
    const Renderer::font*     font_;
};

// ===========================================================================
// tab_builder — a top-level tab. Each `subtab` registers a sub-page
// populated by the supplied callback.
// ===========================================================================
class tab_builder {
public:
    tab_builder(std::shared_ptr<c_tab> tab,
                const Renderer::font* font,
                int default_columns)
        : tab_(std::move(tab)), font_(font), columns_(default_columns) {}

    template <class Body>
    tab_builder& subtab(std::string name, Body&& body)
    {
        auto st = element_factory::make<c_subtab>(name);
        auto pm = element_factory::make<c_page_manager>();
        auto page = element_factory::make<c_page>("page");

        page_builder pb{page, font_, columns_};
        body(pb);

        pm->add_child(page);
        pm->set_active_page(page);
        st->add_child(pm);
        tab_->add_child(st);
        if (!active_set_) {
            tab_->set_active_subtab(st);
            active_set_ = true;
        }
        return *this;
    }

    std::shared_ptr<c_tab> tab() const { return tab_; }

private:
    std::shared_ptr<c_tab> tab_;
    const Renderer::font*  font_;
    int                    columns_;
    bool                   active_set_ = false;
};

// ===========================================================================
// menu_builder — the top-level entry. `build()` returns a ready
// `c_menu_window` with the tabs wired in.
// ===========================================================================
class menu_builder {
public:
    menu_builder(std::string title,
                 const Renderer::font* body_font,
                 const Renderer::font* tab_font = nullptr,
                 float w = 800.0f, float h = 600.0f)
        : title_(std::move(title)),
          body_font_(body_font),
          tab_font_(tab_font ? tab_font : body_font),
          w_(w), h_(h)
    {
        win_  = element_factory::make<c_menu_window>(title_, body_font_);
        win_->set_bounds({0, 0, w_, h_});
        tabs_ = element_factory::make<c_tab_manager>(c_tab_manager::vertical);
        tabs_->set_tab_font(tab_font_);
        win_->add_child(tabs_);
    }

    template <class Body>
    menu_builder& tab(std::string name, Body&& body, int columns = 2)
    {
        auto t = element_factory::make<c_tab>(name);
        t->set_subtab_font(body_font_);

        tab_builder tb{t, body_font_, columns};
        body(tb);

        tabs_->add_child(t);
        if (!first_set_) { tabs_->set_active_tab(t); first_set_ = true; }
        return *this;
    }

    // Add a pre-built tab directly (e.g. the demo's framework tabs).
    menu_builder& add_tab(std::shared_ptr<c_tab> t) {
        tabs_->add_child(t);
        if (!first_set_) { tabs_->set_active_tab(t); first_set_ = true; }
        return *this;
    }

    std::shared_ptr<c_menu_window> build() const { return win_; }

    std::shared_ptr<c_tab_manager> tab_manager() const { return tabs_; }

private:
    std::string             title_;
    const Renderer::font*   body_font_;
    const Renderer::font*   tab_font_;
    float                   w_, h_;
    std::shared_ptr<c_menu_window> win_;
    std::shared_ptr<c_tab_manager> tabs_;
    bool                    first_set_ = false;
};

// Convenience free function so callers can write
//   GUI::builder::menu("X", &body, &tab).tab(…).build();
inline menu_builder menu(std::string title,
                         const Renderer::font* body_font,
                         const Renderer::font* tab_font = nullptr,
                         float w = 800.0f, float h = 600.0f)
{
    return menu_builder{std::move(title), body_font, tab_font, w, h};
}

} // namespace GUI::builder
