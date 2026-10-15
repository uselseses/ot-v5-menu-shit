#pragma once
// =============================================================================
// GUI::menu::c_settings — string-keyed registry of persistent widget states.
//
// One state per (path, type). Lazy-creates entries with a supplied default
// on first access; subsequent calls return the same shared_ptr so widgets
// re-bind to the same backing value across menu rebuilds.
//
// Each widget in `cheat_menu_builder.cpp` calls the matching accessor
// (bool_state / int_state / float_state / colour_state / kb_state) with a
// stable dotted path (e.g. "legit.general.smoke_check") and a default
// taken from the IDB. The Cloud lib's config manager serialises every
// entry to JSON for save / load.
// =============================================================================

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include <Renderer/color.hpp>
#include <wm/types/thread_safe_t.hpp>

namespace GUI::menu {

using bool_state_t   = wm::types::thread_safe_t<bool, bool>;
using int_state_t    = wm::types::thread_safe_t<int, int>;
using float_state_t  = wm::types::thread_safe_t<float, float>;
using string_state_t = wm::types::thread_safe_t<std::string, std::string>;
// Colors are stored PACKED (0xAARRGGBB) — matches what c_colorpicker
// holds internally so the registry can bind directly to its state_t.
using color_state_t  = wm::types::thread_safe_t<std::uint32_t, std::uint32_t>;
// Keybinder state is the packed std::uint32_t produced by
// GUI::keybind_state::pack() (low 16 bits = VK, next 8 = mode).
using kb_state_t     = wm::types::thread_safe_t<std::uint32_t, std::uint32_t>;

class c_settings {
public:
    c_settings() = default;
    c_settings(const c_settings&)            = delete;
    c_settings& operator=(const c_settings&) = delete;

    // Each accessor: if `path` is already registered for this type, return
    // the existing state; otherwise create it seeded with `default_v` and
    // remember the default so a future "reset to defaults" can restore it.
    std::shared_ptr<bool_state_t> bool_state(const std::string& path, bool default_v = false) {
        auto& slot = bools_[path];
        if (!slot) {
            slot = std::make_shared<bool_state_t>(default_v);
            bool_defaults_[path] = default_v;
        }
        return slot;
    }
    std::shared_ptr<int_state_t> int_state(const std::string& path, int default_v = 0) {
        auto& slot = ints_[path];
        if (!slot) {
            slot = std::make_shared<int_state_t>(default_v);
            int_defaults_[path] = default_v;
        }
        return slot;
    }
    std::shared_ptr<float_state_t> float_state(const std::string& path, float default_v = 0.0f) {
        auto& slot = floats_[path];
        if (!slot) {
            slot = std::make_shared<float_state_t>(default_v);
            float_defaults_[path] = default_v;
        }
        return slot;
    }
    std::shared_ptr<string_state_t> string_state(const std::string& path,
                                                 std::string default_v = {}) {
        auto& slot = strings_[path];
        if (!slot) {
            slot = std::make_shared<string_state_t>(default_v);
            string_defaults_[path] = default_v;
        }
        return slot;
    }
    std::shared_ptr<color_state_t> color_state(const std::string& path,
                                               Renderer::color default_v = {}) {
        auto& slot = colors_[path];
        if (!slot) {
            slot = std::make_shared<color_state_t>(default_v.packed());
            color_defaults_[path] = default_v.packed();
        }
        return slot;
    }
    std::shared_ptr<kb_state_t> kb_state(const std::string& path, std::uint32_t default_v = 0) {
        auto& slot = kbs_[path];
        if (!slot) {
            slot = std::make_shared<kb_state_t>(default_v);
            kb_defaults_[path] = default_v;
        }
        return slot;
    }

    // Reset every state to its captured default.
    void reset_to_defaults() {
        for (auto& [k, s] : bools_)   if (s) s->set(bool_defaults_[k]);
        for (auto& [k, s] : ints_)    if (s) s->set(int_defaults_[k]);
        for (auto& [k, s] : floats_)  if (s) s->set(float_defaults_[k]);
        for (auto& [k, s] : strings_) if (s) s->set(string_defaults_[k]);
        for (auto& [k, s] : colors_)  if (s) s->set(color_defaults_[k]);
        for (auto& [k, s] : kbs_)     if (s) s->set(kb_defaults_[k]);
    }

    // Read-only accessors for the serialiser (Cloud lib walks these).
    const auto& bools()   const { return bools_; }
    const auto& ints()    const { return ints_; }
    const auto& floats()  const { return floats_; }
    const auto& strings() const { return strings_; }
    const auto& colors()  const { return colors_; }
    const auto& kbs()     const { return kbs_; }

private:
    std::unordered_map<std::string, std::shared_ptr<bool_state_t>>   bools_;
    std::unordered_map<std::string, std::shared_ptr<int_state_t>>    ints_;
    std::unordered_map<std::string, std::shared_ptr<float_state_t>>  floats_;
    std::unordered_map<std::string, std::shared_ptr<string_state_t>> strings_;
    std::unordered_map<std::string, std::shared_ptr<color_state_t>>  colors_;
    std::unordered_map<std::string, std::shared_ptr<kb_state_t>>     kbs_;

    std::unordered_map<std::string, bool>            bool_defaults_;
    std::unordered_map<std::string, int>             int_defaults_;
    std::unordered_map<std::string, float>           float_defaults_;
    std::unordered_map<std::string, std::string>     string_defaults_;
    std::unordered_map<std::string, std::uint32_t>   color_defaults_;
    std::unordered_map<std::string, std::uint32_t>   kb_defaults_;
};

// One settings instance per host. The demo creates a static one; production
// integrations can plug their own through the build_ctx.
inline c_settings& default_settings() {
    static c_settings s;
    return s;
}

} // namespace GUI::menu
