#pragma once
// =============================================================================
// Renderer::c_renderer_core
//
// Recording primitives. Sits on top of `draw_list` and `clip_stack` and
// is the public surface the GUI library calls into ("render_*"
// functions in the original).
//
// Phase 2a primitives:
//   - render_filled_rect
//   - render_rect            (outline)
//   - render_filled_triangle
//   - render_triangle        (outline)
//   - render_line
//
// Phase 2b adds: bezier curve, gradient circle, circle, polygon,
// unfilled triangle list. Phase 2c adds text.
// =============================================================================

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <Renderer/draw_list.hpp>
#include <Renderer/clip_stack.hpp>
#include <Renderer/color.hpp>
#include <Renderer/geometry.hpp>

namespace Renderer {

// Z-layer enum -- matches the binary's two-work-queue layout in
// dec_PerFrame.c (normal pass + overlay pass). Popups (open dropdown
// option list, open colorpicker plane, context menus, dialog windows)
// push the `overlay` layer so they render above siblings declared later
// in the tree.
enum class layer_e : std::uint8_t { normal = 0, overlay = 1 };

class c_renderer_core {
public:
    c_renderer_core(draw_list& normal_dl, draw_list& overlay_dl)
        : normal_dl_{normal_dl}, overlay_dl_{overlay_dl}, active_{&normal_dl} {}

    // Backwards-compatible single-list ctor: overlay aliases to the
    // same draw list (overlay still flushed, just into the same queue).
    explicit c_renderer_core(draw_list& dl)
        : normal_dl_{dl}, overlay_dl_{dl}, active_{&dl} {}

    void reset(rect screen) {
        clips_.reset(screen);
        screen_ = screen;
        active_ = &normal_dl_;
        alpha_stack_.clear();
    }

    // --- alpha stack ---
    void push_alpha(float a) {
        const float current = alpha_stack_.empty() ? 1.0f : alpha_stack_.back();
        alpha_stack_.push_back(std::clamp(a * current, 0.0f, 1.0f));
    }
    void pop_alpha() {
        if (!alpha_stack_.empty()) {
            alpha_stack_.pop_back();
        }
    }
    float current_alpha() const noexcept {
        return alpha_stack_.empty() ? 1.0f : alpha_stack_.back();
    }
    color apply_alpha(color c) const noexcept {
        if (alpha_stack_.empty()) return c;
        const float a = alpha_stack_.back();
        if (a >= 1.0f) return c;
        if (a <= 0.0f) return c.with_alpha(0);
        return c.with_alpha(static_cast<std::uint8_t>(c.a * a + 0.5f));
    }

    // --- geometry recording & transformation ---
    struct draw_cursor {
        std::size_t normal_verts  = 0;
        std::size_t normal_cmds   = 0;
        std::size_t overlay_verts = 0;
        std::size_t overlay_cmds  = 0;
    };

    draw_cursor capture_cursor() const noexcept {
        return draw_cursor{
            normal_dl_.vertices().size(),
            normal_dl_.commands().size(),
            overlay_dl_.vertices().size(),
            overlay_dl_.commands().size()
        };
    }

    void transform_recorded(const draw_cursor& start, vec2 center, float scale, vec2 offset) {
        auto apply_to_dl = [&](draw_list& dl, std::size_t start_v, std::size_t start_c) {
            auto& verts = dl.vertices();
            for (std::size_t i = start_v; i < verts.size(); ++i) {
                verts[i].pos[0] = center.x + (verts[i].pos[0] - center.x) * scale + offset.x;
                verts[i].pos[1] = center.y + (verts[i].pos[1] - center.y) * scale + offset.y;
            }
            auto& cmds = dl.commands();
            for (std::size_t i = start_c; i < cmds.size(); ++i) {
                if (!cmds[i].scissor.empty()) {
                    cmds[i].scissor.x = center.x + (cmds[i].scissor.x - center.x) * scale + offset.x;
                    cmds[i].scissor.y = center.y + (cmds[i].scissor.y - center.y) * scale + offset.y;
                    cmds[i].scissor.w = cmds[i].scissor.w * scale;
                    cmds[i].scissor.h = cmds[i].scissor.h * scale;
                }
            }
        };

        apply_to_dl(normal_dl_, start.normal_verts, start.normal_cmds);
        apply_to_dl(overlay_dl_, start.overlay_verts, start.overlay_cmds);
    }

    // --- layer stack ---
    void push_layer(layer_e l) {
        layer_stack_.push_back(active_);
        clip_layer_stack_.push_back(clips_);
        if (l == layer_e::overlay) {
            clips_.reset(screen_);
        }
        active_ = (l == layer_e::overlay) ? &overlay_dl_ : &normal_dl_;
    }
    void pop_layer() {
        if (!layer_stack_.empty()) {
            active_ = layer_stack_.back();
            layer_stack_.pop_back();
        }
        if (!clip_layer_stack_.empty()) {
            clips_ = clip_layer_stack_.back();
            clip_layer_stack_.pop_back();
        }
    }

    // --- clip stack passthrough ---
    void push_clip(rect r) { clips_.push(r); }
    void pop_clip()        { clips_.pop(); }
    rect current_clip() const { return clips_.top(); }

    // --- Phase 2a primitives ---
    void render_filled_rect    (rect r, color c);
    void render_rect           (rect r, color c, float thickness = 1.0f);
    void render_filled_triangle(vec2 p0, vec2 p1, vec2 p2, color c);
    void render_triangle       (vec2 p0, vec2 p1, vec2 p2, color c, float thickness = 1.0f);
    void render_line           (vec2 a, vec2 b, color c, float thickness = 1.0f);

    // --- Phase 2b primitives ---
    // Four-color gradient rect: corner order is TL, TR, BR, BL.
    void render_gradient_rect    (rect r, color tl, color tr, color br, color bl);
    void render_texture          (rect r, texture_handle tex, color tint = color{255, 255, 255, 255});
    // Rotated textured quad — same UV mapping as render_texture, but
    // each corner is rotated about the rect center by `rotation_deg`.
    // Matches binary `vt[+304] render_textured_rect(rect, tex, color,
    // scale, rotation)` used by groupbox + dropdown chevrons
    // (`c_groupbox__render.c:356-362`, `c_dropdown__render.c:430-436`).
    void render_rotated_texture  (rect r, texture_handle tex, color tint,
                                  float rotation_deg);

    // Rounded variants. `radius == 0` falls back to the sharp version.
    void render_filled_rounded_rect(rect r, color c, float radius, int seg_per_corner = 6);
    void render_rounded_rect       (rect r, color c, float radius,
                                    float thickness = 1.0f, int seg_per_corner = 6);

    // Per-corner radii, order {TL, TR, BR, BL}; 0 = sharp corner.
    // All-zero falls back to the sharp version.
    void render_filled_rounded_rect_ex(rect r, color c, const float radii[4],
                                       int seg_per_corner = 6);
    void render_rounded_rect_ex       (rect r, color c, const float radii[4],
                                       float thickness = 1.0f, int seg_per_corner = 6);

    // Circles. `segments == 0` picks a count based on radius.
    void render_filled_circle    (vec2 center, float radius, color c, int segments = 0);
    void render_circle           (vec2 center, float radius, color c,
                                  float thickness = 1.0f, int segments = 0);
    // Radial 2-color gradient: `inner` at the center, `outer` at the rim.
    void render_gradient_circle  (vec2 center, float radius,
                                  color inner, color outer, int segments = 0);

    // Convex polygon (triangle-fan). For concave shapes, decompose
    // beforehand.
    void render_filled_polygon   (const vec2* pts, int count, color c);
    void render_polygon          (const vec2* pts, int count, color c,
                                  float thickness = 1.0f);

    // Quadratic / cubic Bezier as a polyline.
    void render_bezier_quadratic (vec2 p0, vec2 p1, vec2 p2,
                                  color c, float thickness = 1.0f, int segments = 32);
    void render_bezier_cubic     (vec2 p0, vec2 p1, vec2 p2, vec2 p3,
                                  color c, float thickness = 1.0f, int segments = 32);

    // Aug 5-8 changelog: "Add render_triangle() and
    // render_unfilled_triangle()" --- alias provided for source parity.
    void render_unfilled_triangle(vec2 p0, vec2 p1, vec2 p2,
                                  color c, float thickness = 1.0f) {
        render_triangle(p0, p1, p2, c, thickness);
    }

    // --- Phase 2c: text ---
    // Renders UTF-8 `text` at `pos` (text baseline is pos.y + font.ascent()).
    void render_text(const class font& f, vec2 pos, color c, const char* text);
    void render_text(const class font& f, vec2 pos, color c, const std::string& s) {
        render_text(f, pos, c, s.c_str());
    }
    // Renders `text` twice: once at +(1,1) in the shadow color, once at
    // pos in `c`. Reproduces the binary's `text_drop_shadow_demo`.
    void render_text_drop_shadow(const class font& f, vec2 pos,
                                 color c, color shadow, const char* text);

private:
    draw_list&               normal_dl_;
    draw_list&               overlay_dl_;
    draw_list*               active_;          // current emit target
    std::vector<draw_list*>  layer_stack_;     // saved by push_layer
    std::vector<clip_stack>  clip_layer_stack_; // saved by push_layer
    clip_stack               clips_;
    rect                     screen_{};
    std::vector<float>       alpha_stack_;
};

} // namespace Renderer
