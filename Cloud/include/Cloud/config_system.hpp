#pragma once
// =============================================================================
// GUI::config_system::manager — text-based save / load for c_settings.
//
// Format: INI-ish, one section per type, `path=value` per line.
//
//   [bool]
//   legit.general.master_switch=1
//   legit.general.smoke_check=0
//
//   [int]
//   legit.general.reaction_time=100
//
//   [float]
//   visuals.world.cm.pearlescent=0.5
//
//   [string]
//   misc.clantag.text=onetap
//
//   [color]
//   settings.gui.accent_color=0xFFFAA618
//
//   [kb]
//   rage.aimbot.kb.double_tap=0x12345
//
// Unknown sections / lines are skipped silently, so old configs survive
// adding new fields. Same for renamed paths — missing entries fall back
// to the default registered at first access.
// =============================================================================

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include <GUI/menu/c_settings.hpp>

namespace GUI::config_system {

class manager {
public:
    // Write every registered state to `path`. Returns true on success.
    static bool save_to(const std::string& path, const menu::c_settings& s) {
        std::ofstream out(path, std::ios::trunc);
        if (!out) return false;

        out << "[bool]\n";
        for (const auto& [k, st] : s.bools()) {
            if (st) out << k << '=' << (st->get() ? 1 : 0) << '\n';
        }
        out << "\n[int]\n";
        for (const auto& [k, st] : s.ints()) {
            if (st) out << k << '=' << st->get() << '\n';
        }
        out << "\n[float]\n";
        for (const auto& [k, st] : s.floats()) {
            if (st) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(st->get()));
                out << k << '=' << buf << '\n';
            }
        }
        out << "\n[string]\n";
        for (const auto& [k, st] : s.strings()) {
            if (st) out << k << '=' << escape_(st->get()) << '\n';
        }
        out << "\n[color]\n";
        for (const auto& [k, st] : s.colors()) {
            if (st) {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "0x%08X", st->get());
                out << k << '=' << buf << '\n';
            }
        }
        out << "\n[kb]\n";
        for (const auto& [k, st] : s.kbs()) {
            if (st) out << k << '=' << st->get() << '\n';
        }
        return static_cast<bool>(out);
    }

    // Read `path` into `s`. Entries that aren't already registered get
    // lazily created with the parsed value as their default.
    static bool load_from(const std::string& path, menu::c_settings& s) {
        std::ifstream in(path);
        if (!in) return false;

        std::string line;
        std::string section;
        while (std::getline(in, line)) {
            // Trim CR / trailing whitespace.
            while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                line.pop_back();
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            if (line.front() == '[' && line.back() == ']') {
                section = line.substr(1, line.size() - 2);
                continue;
            }
            const auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = line.substr(0, eq);
            const std::string val = line.substr(eq + 1);
            if (key.empty()) continue;

            if (section == "bool") {
                s.bool_state(key, parse_bool_(val))->set(parse_bool_(val));
            } else if (section == "int") {
                int v = 0;
                try { v = std::stoi(val); } catch (...) { continue; }
                s.int_state(key, v)->set(v);
            } else if (section == "float") {
                float v = 0.0f;
                try { v = std::stof(val); } catch (...) { continue; }
                s.float_state(key, v)->set(v);
            } else if (section == "string") {
                std::string v = unescape_(val);
                s.string_state(key, v)->set(v);
            } else if (section == "color") {
                std::uint32_t v = 0;
                if (!parse_uint_(val, v)) continue;
                // color_state expects a Renderer::color default; the
                // stored value is already packed, so write through the
                // raw thread_safe_t set() after lazy-creating.
                auto st = s.color_state(key, Renderer::color::from_packed(v));
                st->set(v);
            } else if (section == "kb") {
                std::uint32_t v = 0;
                if (!parse_uint_(val, v)) continue;
                s.kb_state(key, v)->set(v);
            }
        }
        return true;
    }

private:
    static bool parse_bool_(const std::string& s) {
        if (s == "1" || s == "true" || s == "yes" || s == "on") return true;
        return false;
    }
    static bool parse_uint_(const std::string& s, std::uint32_t& out) {
        try {
            std::size_t idx = 0;
            const auto v = std::stoul(s, &idx, 0);   // base=0 → 0x prefix understood
            out = static_cast<std::uint32_t>(v);
            return true;
        } catch (...) { return false; }
    }
    static std::string escape_(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '\\' || c == '\n' || c == '=') {
                out.push_back('\\');
                if (c == '\n') out.push_back('n');
                else           out.push_back(c);
            } else {
                out.push_back(c);
            }
        }
        return out;
    }
    static std::string unescape_(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                const char n = s[i + 1];
                if (n == 'n')      { out.push_back('\n'); ++i; }
                else               { out.push_back(n);    ++i; }
            } else {
                out.push_back(s[i]);
            }
        }
        return out;
    }
};

} // namespace GUI::config_system
