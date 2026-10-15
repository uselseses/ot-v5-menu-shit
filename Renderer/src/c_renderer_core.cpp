// =============================================================================
// c_renderer_core.cpp --- primitive recording.
//
// Each emit call:
//   1) reserves vertex / index slots through the active draw_cmd
//   2) writes the geometry (untextured == uv {0,0}, white SRV bound)
//
// The default texture (1x1 white) is bound implicitly by passing the
// null texture_handle --- the DX11 backend swaps it for the real white
// SRV at flush time.
// =============================================================================

#include <Renderer/c_renderer_core.hpp>
#include <Renderer/font.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

namespace Renderer {

namespace {

constexpr texture_handle k_no_texture{};

inline void write_vertex(vertex* dst, float x, float y, color c) {
    dst->pos[0] = x; dst->pos[1] = y;
    dst->uv [0] = 0.0f; dst->uv [1] = 0.0f;
    dst->col    = c.packed();
}

// Sensible default segment count for a circle of the given radius.
// Keeps tessellation tight without blowing up the index buffer.
inline int auto_segments(float radius) {
    if (radius < 4)  return 8;
    if (radius < 12) return 16;
    if (radius < 32) return 24;
    if (radius < 64) return 32;
    return 48;
}

inline color lerp_color(color a, color b, float t) {
    const float u = 1.0f - t;
    return color{
        static_cast<std::uint8_t>(a.r * u + b.r * t + 0.5f),
        static_cast<std::uint8_t>(a.g * u + b.g * t + 0.5f),
        static_cast<std::uint8_t>(a.b * u + b.b * t + 0.5f),
        static_cast<std::uint8_t>(a.a * u + b.a * t + 0.5f),
    };
}

} // anon

void c_renderer_core::render_filled_rect(rect r, color c)
{
    if (clips_.empty_top() || r.empty()) return;
    c = apply_alpha(c);

    const auto base = active_->prim_reserve(/*verts=*/4, /*idx=*/6, k_no_texture, clips_.top());
    vertex*  v = active_->vertices().data() + base;
    index_t* i = active_->indices().data()  + (active_->indices().size() - 6);

    write_vertex(&v[0], r.x,       r.y,       c);
    write_vertex(&v[1], r.x + r.w, r.y,       c);
    write_vertex(&v[2], r.x + r.w, r.y + r.h, c);
    write_vertex(&v[3], r.x,       r.y + r.h, c);

    i[0] = static_cast<index_t>(base + 0);
    i[1] = static_cast<index_t>(base + 1);
    i[2] = static_cast<index_t>(base + 2);
    i[3] = static_cast<index_t>(base + 0);
    i[4] = static_cast<index_t>(base + 2);
    i[5] = static_cast<index_t>(base + 3);
}

void c_renderer_core::render_rect(rect r, color c, float thickness)
{
    if (clips_.empty_top() || r.empty()) return;
    const float t = thickness;
    render_filled_rect(rect{r.x,           r.y,             r.w,         t           }, c); // top
    render_filled_rect(rect{r.x,           r.y + r.h - t,   r.w,         t           }, c); // bottom
    render_filled_rect(rect{r.x,           r.y + t,         t,           r.h - 2 * t }, c); // left
    render_filled_rect(rect{r.x + r.w - t, r.y + t,         t,           r.h - 2 * t }, c); // right
}

void c_renderer_core::render_filled_triangle(vec2 p0, vec2 p1, vec2 p2, color c)
{
    if (clips_.empty_top()) return;
    c = apply_alpha(c);

    const auto base = active_->prim_reserve(3, 3, k_no_texture, clips_.top());
    vertex*  v = active_->vertices().data() + base;
    index_t* i = active_->indices().data()  + (active_->indices().size() - 3);

    write_vertex(&v[0], p0.x, p0.y, c);
    write_vertex(&v[1], p1.x, p1.y, c);
    write_vertex(&v[2], p2.x, p2.y, c);

    i[0] = static_cast<index_t>(base + 0);
    i[1] = static_cast<index_t>(base + 1);
    i[2] = static_cast<index_t>(base + 2);
}

void c_renderer_core::render_triangle(vec2 p0, vec2 p1, vec2 p2, color c, float thickness)
{
    render_line(p0, p1, c, thickness);
    render_line(p1, p2, c, thickness);
    render_line(p2, p0, c, thickness);
}

void c_renderer_core::render_line(vec2 a, vec2 b, color c, float thickness)
{
    if (clips_.empty_top()) return;
    c = apply_alpha(c);

    const float dx     = b.x - a.x;
    const float dy     = b.y - a.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0f) return;

    const float inv_len = 1.0f / length;
    const float nx      = -dy * inv_len;     // perpendicular unit vector
    const float ny      =  dx * inv_len;
    // Core half-thickness + 0.5 px feather on each side. The feather
    // verts fade alpha to 0 so the rasterizer's MSAA-equivalent blend
    // gives a sub-pixel-smooth edge without any shader changes.
    const float half      = thickness * 0.5f;
    const float feather   = 0.5f;
    const float hx_in     = nx * half;
    const float hy_in     = ny * half;
    const float hx_out    = nx * (half + feather);
    const float hy_out    = ny * (half + feather);
    const color c_fade    = c.with_alpha(0);

    // 8 verts: 4 inner (opaque) + 4 outer (transparent). 18 indices.
    //
    //   a-outer-top    b-outer-top         (v4, v5) alpha 0
    //   a-inner-top    b-inner-top         (v0, v1) opaque
    //   a-inner-bot    b-inner-bot         (v2, v3) opaque
    //   a-outer-bot    b-outer-bot         (v6, v7) alpha 0
    const auto base = active_->prim_reserve(8, 18, k_no_texture, clips_.top());
    vertex*  v = active_->vertices().data() + base;
    index_t* i = active_->indices().data()  + (active_->indices().size() - 18);

    write_vertex(&v[0], a.x + hx_in,  a.y + hy_in,  c);       // a inner +
    write_vertex(&v[1], b.x + hx_in,  b.y + hy_in,  c);       // b inner +
    write_vertex(&v[2], b.x - hx_in,  b.y - hy_in,  c);       // b inner -
    write_vertex(&v[3], a.x - hx_in,  a.y - hy_in,  c);       // a inner -
    write_vertex(&v[4], a.x + hx_out, a.y + hy_out, c_fade);  // a outer +
    write_vertex(&v[5], b.x + hx_out, b.y + hy_out, c_fade);  // b outer +
    write_vertex(&v[6], b.x - hx_out, b.y - hy_out, c_fade);  // b outer -
    write_vertex(&v[7], a.x - hx_out, a.y - hy_out, c_fade);  // a outer -

    auto idx = [&](int slot, int vi) { i[slot] = static_cast<index_t>(base + vi); };
    // Core quad.
    idx( 0, 0); idx( 1, 1); idx( 2, 2);
    idx( 3, 0); idx( 4, 2); idx( 5, 3);
    // Top feather (outer-top -> inner-top).
    idx( 6, 4); idx( 7, 5); idx( 8, 1);
    idx( 9, 4); idx(10, 1); idx(11, 0);
    // Bottom feather (inner-bot -> outer-bot).
    idx(12, 3); idx(13, 2); idx(14, 6);
    idx(15, 3); idx(16, 6); idx(17, 7);
}

// ============================================================================
// Phase 2b primitives
// ============================================================================

// Rounded rects are emitted as a single closed perimeter path with a
// 0.5 px alpha feather (same trick as render_line). The previous
// 9-slice + hard-edged corner fans left un-rasterized notches at small
// radii (1-3 px — exactly the range the binary's floor(scale * 2.75)
// rounding produces): along the 45° diagonal no pixel center falls
// inside a 1 px annulus, so stacked concentric fills (menu chrome
// border) showed broken corners.
namespace {

struct path_pt { float x, y, nx, ny; };  // point + outward unit normal

// Builds the clockwise rounded-rect perimeter: 4 corner arcs of
// (seg + 1) points each. Straight edges fall out of connecting
// consecutive ring points (the two tangent points share a normal).
// Per-corner radii, order TL, TR, BR, BL; a radius of 0 degenerates
// that arc to (seg + 1) coincident points at the sharp corner whose
// normals still sweep the quarter turn — the feather fans correctly
// around the point and downstream strip indexing stays uniform.
// Returns the point count written (max 4 * (seg + 1)).
inline int build_rounded_path4(rect r, const float rad[4], int seg, path_pt* out)
{
    const float L = r.x;
    const float T = r.y;
    const float R = r.x + r.w;
    const float B = r.y + r.h;
    const float step = (std::numbers::pi_v<float> * 0.5f) / seg;

    struct corner { float cx, cy, a0, cr; };
    const corner corners[4] = {
        {L + rad[0], T + rad[0], std::numbers::pi_v<float>,        rad[0]}, // TL 180..270
        {R - rad[1], T + rad[1], std::numbers::pi_v<float> * 1.5f, rad[1]}, // TR 270..360
        {R - rad[2], B - rad[2], 0.0f,                             rad[2]}, // BR   0..90
        {L + rad[3], B - rad[3], std::numbers::pi_v<float> * 0.5f, rad[3]}, // BL  90..180
    };

    int n = 0;
    for (const auto& k : corners) {
        for (int s = 0; s <= seg; ++s) {
            const float a  = k.a0 + step * s;
            const float ca = std::cos(a);
            const float sa = std::sin(a);
            out[n++] = {k.cx + ca * k.cr, k.cy + sa * k.cr, ca, sa};
        }
    }
    return n;
}

inline int build_rounded_path(rect r, float radius, int seg, path_pt* out)
{
    const float rad[4] = {radius, radius, radius, radius};
    return build_rounded_path4(r, rad, seg, out);
}

// Scale radii down so adjacent pairs fit their shared edge (per-corner
// rects can otherwise self-intersect when one radius approaches the
// full edge length).
inline void clamp_radii4(rect r, float rad[4])
{
    for (int i = 0; i < 4; ++i) rad[i] = std::max(0.0f, rad[i]);
    float s = 1.0f;
    auto fit = [&](float a, float b, float edge) {
        if (a + b > edge && a + b > 0.0f) s = std::min(s, edge / (a + b));
    };
    fit(rad[0], rad[1], r.w);   // top
    fit(rad[3], rad[2], r.w);   // bottom
    fit(rad[0], rad[3], r.h);   // left
    fit(rad[1], rad[2], r.h);   // right
    if (s < 1.0f)
        for (int i = 0; i < 4; ++i) rad[i] *= s;
}

constexpr float k_feather = 0.5f;
constexpr int   k_max_seg = 48;

// Snap a rect's edges to integer pixels. The binary computes integer
// rects + integer radii throughout; feeding half-pixel rects into the
// feathered path makes every edge semi-transparent (the 0.5 px alpha
// ramp straddles the pixel centers) — borders read as washed out.
inline rect snap_rect(rect r) {
    const float L = std::round(r.x);
    const float T = std::round(r.y);
    const float R = std::round(r.x + r.w);
    const float B = std::round(r.y + r.h);
    return rect{L, T, R - L, B - T};
}

} // anon

void c_renderer_core::render_filled_rounded_rect(rect r, color c, float radius, int seg_per_corner)
{
    const float rad[4] = {radius, radius, radius, radius};
    render_filled_rounded_rect_ex(r, c, rad, seg_per_corner);
}

void c_renderer_core::render_filled_rounded_rect_ex(rect r, color c, const float radii[4], int seg_per_corner)
{
    if (clips_.empty_top() || r.empty()) return;
    c = apply_alpha(c);
    float rad[4] = {radii[0], radii[1], radii[2], radii[3]};
    if (rad[0] <= 0.0f && rad[1] <= 0.0f && rad[2] <= 0.0f && rad[3] <= 0.0f) {
        render_filled_rect(r, c);
        return;
    }
    r = snap_rect(r);
    clamp_radii4(r, rad);

    seg_per_corner = std::clamp(seg_per_corner, 1, k_max_seg);
    path_pt path[4 * (k_max_seg + 1)];
    const int P = build_rounded_path4(r, rad, seg_per_corner, path);

    // Layout: [0] center, [1..P] opaque perimeter (inset 0.5),
    // [P+1..2P] feather ring (outset 0.5, alpha 0).
    const int verts = 1 + 2 * P;
    const int idx   = 3 * P + 6 * P;          // center fan + feather strip
    const auto base = active_->prim_reserve(verts, idx, k_no_texture, clips_.top());
    vertex*  v = active_->vertices().data() + base;
    index_t* i = active_->indices().data()  + (active_->indices().size() - idx);

    const color c_fade = c.with_alpha(0);
    // Opaque perimeter sits ON the path (inset 0), feather extends only
    // OUTWARD. This makes a filled rect's solid edge coincide with a
    // 1-px border drawn on the same rect (render_rounded_rect's core is
    // also on the path) — previously the fill was inset 0.5px, so a
    // fill+border pair (slider track, groupbox) showed the border ~1px
    // off the fill (user 2026-06-10).
    write_vertex(&v[0], r.x + r.w * 0.5f, r.y + r.h * 0.5f, c);
    for (int s = 0; s < P; ++s) {
        const auto& p = path[s];
        write_vertex(&v[1 + s],     p.x,                    p.y,                    c);
        write_vertex(&v[1 + P + s], p.x + p.nx * k_feather, p.y + p.ny * k_feather, c_fade);
    }

    int io = 0;
    for (int s = 0; s < P; ++s) {
        const int n  = (s + 1) % P;
        const int in0 = base + 1 + s,     in1 = base + 1 + n;
        const int ou0 = base + 1 + P + s, ou1 = base + 1 + P + n;
        // Fan triangle (center -> opaque ring).
        i[io++] = static_cast<index_t>(base);
        i[io++] = static_cast<index_t>(in0);
        i[io++] = static_cast<index_t>(in1);
        // Feather quad (opaque ring -> transparent ring).
        i[io++] = static_cast<index_t>(in0);
        i[io++] = static_cast<index_t>(ou0);
        i[io++] = static_cast<index_t>(ou1);
        i[io++] = static_cast<index_t>(in0);
        i[io++] = static_cast<index_t>(ou1);
        i[io++] = static_cast<index_t>(in1);
    }
}

void c_renderer_core::render_rounded_rect(rect r, color c, float radius,
                                          float thickness, int seg_per_corner)
{
    const float rad[4] = {radius, radius, radius, radius};
    render_rounded_rect_ex(r, c, rad, thickness, seg_per_corner);
}

void c_renderer_core::render_rounded_rect_ex(rect r, color c, const float radii[4],
                                             float thickness, int seg_per_corner)
{
    float rad[4] = {radii[0], radii[1], radii[2], radii[3]};
    if (rad[0] <= 0.0f && rad[1] <= 0.0f && rad[2] <= 0.0f && rad[3] <= 0.0f) {
        render_rect(r, c, thickness);
        return;
    }
    if (clips_.empty_top() || thickness <= 0.0f || r.empty()) return;
    c = apply_alpha(c);
    r = snap_rect(r);
    clamp_radii4(r, rad);

    seg_per_corner = std::clamp(seg_per_corner, 1, k_max_seg);
    path_pt path[4 * (k_max_seg + 1)];
    path_pt path_in[4 * (k_max_seg + 1)];
    const int P = build_rounded_path4(r, rad, seg_per_corner, path);

    // Inner stroke edge: a second concentric path on the rect inset by
    // the stroke thickness, radii reduced by t (clamped at 0 so sharp
    // and small-radius corners degrade to a solid corner instead of
    // folding the ring inside-out). Unlike a fixed normal offset this
    // keeps the stroke exactly `t` thick along straight edges even when
    // a corner radius is 0.
    const float t  = thickness;
    const float it = std::min(t, (r.w < r.h ? r.w : r.h) * 0.5f);
    const rect  ir{r.x + it, r.y + it,
                   std::max(0.0f, r.w - 2.0f * it),
                   std::max(0.0f, r.h - 2.0f * it)};
    float rad_in[4];
    for (int s = 0; s < 4; ++s) rad_in[s] = std::max(0.0f, rad[s] - it);
    build_rounded_path4(ir, rad_in, seg_per_corner, path_in);

    // Four concentric rings around the stroke core:
    //   [0..P)    outer feather (outer path + 0.5, alpha 0)
    //   [P..2P)   outer core    (outer path,       opaque)
    //   [2P..3P)  inner core    (inner path,       opaque)
    //   [3P..4P)  inner feather (inner path - 0.5, alpha 0)
    const int verts = 4 * P;
    const int idx   = 3 * 6 * P;
    const auto base = active_->prim_reserve(verts, idx, k_no_texture, clips_.top());
    vertex*  v = active_->vertices().data() + base;
    index_t* i = active_->indices().data()  + (active_->indices().size() - idx);

    const color c_fade = c.with_alpha(0);
    for (int s = 0; s < P; ++s) {
        const auto& p  = path[s];
        const auto& pi = path_in[s];
        write_vertex(&v[s],         p.x + p.nx * k_feather,   p.y + p.ny * k_feather,   c_fade);
        write_vertex(&v[P + s],     p.x,                      p.y,                      c);
        write_vertex(&v[2 * P + s], pi.x,                     pi.y,                     c);
        write_vertex(&v[3 * P + s], pi.x - pi.nx * k_feather, pi.y - pi.ny * k_feather, c_fade);
    }

    int io = 0;
    auto strip = [&](int ring_a, int ring_b) {
        for (int s = 0; s < P; ++s) {
            const int n  = (s + 1) % P;
            const int a0 = base + ring_a * P + s, a1 = base + ring_a * P + n;
            const int b0 = base + ring_b * P + s, b1 = base + ring_b * P + n;
            i[io++] = static_cast<index_t>(a0);
            i[io++] = static_cast<index_t>(a1);
            i[io++] = static_cast<index_t>(b1);
            i[io++] = static_cast<index_t>(a0);
            i[io++] = static_cast<index_t>(b1);
            i[io++] = static_cast<index_t>(b0);
        }
    };
    strip(0, 1);   // outer feather -> outer core
    strip(1, 2);   // stroke core
    strip(2, 3);   // inner core -> inner feather
}

void c_renderer_core::render_gradient_rect(rect r, color tl, color tr, color br, color bl)
{
    if (clips_.empty_top() || r.empty()) return;
    tl = apply_alpha(tl);
    tr = apply_alpha(tr);
    br = apply_alpha(br);
    bl = apply_alpha(bl);

    const auto base = active_->prim_reserve(4, 6, k_no_texture, clips_.top());
    vertex*  v = active_->vertices().data() + base;
    index_t* i = active_->indices().data()  + (active_->indices().size() - 6);

    write_vertex(&v[0], r.x,       r.y,       tl);
    write_vertex(&v[1], r.x + r.w, r.y,       tr);
    write_vertex(&v[2], r.x + r.w, r.y + r.h, br);
    write_vertex(&v[3], r.x,       r.y + r.h, bl);

    i[0] = static_cast<index_t>(base + 0);
    i[1] = static_cast<index_t>(base + 1);
    i[2] = static_cast<index_t>(base + 2);
    i[3] = static_cast<index_t>(base + 0);
    i[4] = static_cast<index_t>(base + 2);
    i[5] = static_cast<index_t>(base + 3);
}

void c_renderer_core::render_texture(rect r, texture_handle tex, color tint)
{
    if (clips_.empty_top() || r.empty() || !tex.srv) return;
    tint = apply_alpha(tint);

    const auto base = active_->prim_reserve(4, 6, tex, clips_.top());
    vertex*  v = active_->vertices().data() + base;
    index_t* i = active_->indices().data()  + (active_->indices().size() - 6);
    const auto packed = tint.packed();

    v[0].pos[0] = r.x;       v[0].pos[1] = r.y;       v[0].uv[0] = 0.0f; v[0].uv[1] = 0.0f; v[0].col = packed;
    v[1].pos[0] = r.x + r.w; v[1].pos[1] = r.y;       v[1].uv[0] = 1.0f; v[1].uv[1] = 0.0f; v[1].col = packed;
    v[2].pos[0] = r.x + r.w; v[2].pos[1] = r.y + r.h; v[2].uv[0] = 1.0f; v[2].uv[1] = 1.0f; v[2].col = packed;
    v[3].pos[0] = r.x;       v[3].pos[1] = r.y + r.h; v[3].uv[0] = 0.0f; v[3].uv[1] = 1.0f; v[3].col = packed;

    i[0] = static_cast<index_t>(base + 0);
    i[1] = static_cast<index_t>(base + 1);
    i[2] = static_cast<index_t>(base + 2);
    i[3] = static_cast<index_t>(base + 0);
    i[4] = static_cast<index_t>(base + 2);
    i[5] = static_cast<index_t>(base + 3);
}

void c_renderer_core::render_rotated_texture(rect r, texture_handle tex,
                                              color tint, float rotation_deg)
{
    if (clips_.empty_top() || r.empty() || !tex.srv) return;
    tint = apply_alpha(tint);

    const auto base = active_->prim_reserve(4, 6, tex, clips_.top());
    vertex*  v = active_->vertices().data() + base;
    index_t* i = active_->indices().data()  + (active_->indices().size() - 6);
    const auto packed = tint.packed();

    const float cx = r.x + r.w * 0.5f;
    const float cy = r.y + r.h * 0.5f;
    const float rad = rotation_deg * 0.017453292f;
    const float ca = std::cos(rad);
    const float sa = std::sin(rad);
    auto rot = [&](float lx, float ly, float& ox, float& oy) {
        ox = cx + lx * ca - ly * sa;
        oy = cy + lx * sa + ly * ca;
    };
    const float hw = r.w * 0.5f;
    const float hh = r.h * 0.5f;
    rot(-hw, -hh, v[0].pos[0], v[0].pos[1]); v[0].uv[0]=0.f; v[0].uv[1]=0.f; v[0].col=packed;
    rot( hw, -hh, v[1].pos[0], v[1].pos[1]); v[1].uv[0]=1.f; v[1].uv[1]=0.f; v[1].col=packed;
    rot( hw,  hh, v[2].pos[0], v[2].pos[1]); v[2].uv[0]=1.f; v[2].uv[1]=1.f; v[2].col=packed;
    rot(-hw,  hh, v[3].pos[0], v[3].pos[1]); v[3].uv[0]=0.f; v[3].uv[1]=1.f; v[3].col=packed;

    i[0] = static_cast<index_t>(base + 0);
    i[1] = static_cast<index_t>(base + 1);
    i[2] = static_cast<index_t>(base + 2);
    i[3] = static_cast<index_t>(base + 0);
    i[4] = static_cast<index_t>(base + 2);
    i[5] = static_cast<index_t>(base + 3);
}

void c_renderer_core::render_filled_circle(vec2 ctr, float radius, color c, int segments)
{
    if (clips_.empty_top() || radius <= 0.0f) return;
    c = apply_alpha(c);
    if (segments <= 0) segments = auto_segments(radius);

    // 1 center + N inner rim (opaque, at `radius`)
    //                + N outer rim (transparent, at `radius + 0.5px`)
    // Triangulated as N inner fan triangles + N quad strips between
    // inner and outer rims for the 0.5-px feathered edge.
    const float feather = 0.5f;
    const auto n_verts  = static_cast<std::uint32_t>(1 + segments * 2);
    const auto n_idx    = static_cast<std::uint32_t>(segments * 3      // inner fan
                                                   + segments * 6);    // feather quads
    const auto base = active_->prim_reserve(n_verts, n_idx, k_no_texture, clips_.top());

    vertex*  v = active_->vertices().data() + base;
    index_t* i = active_->indices().data()  + (active_->indices().size() - n_idx);

    const color c_fade = c.with_alpha(0);
    const float step   = 2.0f * std::numbers::pi_v<float> / segments;

    write_vertex(&v[0], ctr.x, ctr.y, c);                          // 0    center
    for (int s = 0; s < segments; ++s) {                            // 1..N  inner rim
        const float a = step * s;
        write_vertex(&v[1 + s],
                     ctr.x + std::cos(a) * radius,
                     ctr.y + std::sin(a) * radius, c);
    }
    for (int s = 0; s < segments; ++s) {                            // N+1..2N outer rim
        const float a = step * s;
        write_vertex(&v[1 + segments + s],
                     ctr.x + std::cos(a) * (radius + feather),
                     ctr.y + std::sin(a) * (radius + feather), c_fade);
    }

    // Inner fan + feather strip per segment.
    int io = 0;
    for (int s = 0; s < segments; ++s) {
        const int next = (s + 1) % segments;
        // inner fan
        i[io++] = static_cast<index_t>(base);
        i[io++] = static_cast<index_t>(base + 1 + s);
        i[io++] = static_cast<index_t>(base + 1 + next);
        // feather quad (inner_s, outer_s, outer_next, inner_next)
        const int inner_s    = base + 1 + s;
        const int inner_n    = base + 1 + next;
        const int outer_s    = base + 1 + segments + s;
        const int outer_n    = base + 1 + segments + next;
        i[io++] = static_cast<index_t>(inner_s);
        i[io++] = static_cast<index_t>(outer_s);
        i[io++] = static_cast<index_t>(outer_n);
        i[io++] = static_cast<index_t>(inner_s);
        i[io++] = static_cast<index_t>(outer_n);
        i[io++] = static_cast<index_t>(inner_n);
    }
}

void c_renderer_core::render_circle(vec2 ctr, float radius, color c,
                                    float thickness, int segments)
{
    if (clips_.empty_top() || radius <= 0.0f) return;
    if (segments <= 0) segments = auto_segments(radius);

    // Render as a polyline closing back on itself.
    const float step = 2.0f * std::numbers::pi_v<float> / segments;
    vec2 prev{ctr.x + radius, ctr.y};
    for (int s = 1; s <= segments; ++s) {
        const float a = step * s;
        vec2 cur{ctr.x + std::cos(a) * radius, ctr.y + std::sin(a) * radius};
        render_line(prev, cur, c, thickness);
        prev = cur;
    }
}

void c_renderer_core::render_gradient_circle(vec2 ctr, float radius,
                                             color inner, color outer, int segments)
{
    if (clips_.empty_top() || radius <= 0.0f) return;
    inner = apply_alpha(inner);
    outer = apply_alpha(outer);
    if (segments <= 0) segments = auto_segments(radius);

    const auto n_verts = static_cast<std::uint32_t>(segments + 1);
    const auto n_idx   = static_cast<std::uint32_t>(segments * 3);
    const auto base    = active_->prim_reserve(n_verts, n_idx, k_no_texture, clips_.top());

    vertex*  v = active_->vertices().data() + base;
    index_t* i = active_->indices().data()  + (active_->indices().size() - n_idx);

    write_vertex(&v[0], ctr.x, ctr.y, inner);
    const float step = 2.0f * std::numbers::pi_v<float> / segments;
    for (int s = 0; s < segments; ++s) {
        const float a = step * s;
        write_vertex(&v[1 + s], ctr.x + std::cos(a) * radius,
                                ctr.y + std::sin(a) * radius, outer);
    }
    for (int s = 0; s < segments; ++s) {
        const int next = (s + 1) % segments;
        i[s * 3 + 0] = static_cast<index_t>(base);
        i[s * 3 + 1] = static_cast<index_t>(base + 1 + s);
        i[s * 3 + 2] = static_cast<index_t>(base + 1 + next);
    }
}

void c_renderer_core::render_filled_polygon(const vec2* pts, int count, color c)
{
    if (clips_.empty_top() || count < 3) return;
    c = apply_alpha(c);

    // Triangle fan rooted at pts[0] --- correct for convex polygons.
    const auto n_verts = static_cast<std::uint32_t>(count);
    const auto n_idx   = static_cast<std::uint32_t>((count - 2) * 3);
    const auto base    = active_->prim_reserve(n_verts, n_idx, k_no_texture, clips_.top());

    vertex*  v = active_->vertices().data() + base;
    index_t* i = active_->indices().data()  + (active_->indices().size() - n_idx);

    for (int s = 0; s < count; ++s) write_vertex(&v[s], pts[s].x, pts[s].y, c);
    for (int s = 0; s < count - 2; ++s) {
        i[s * 3 + 0] = static_cast<index_t>(base);
        i[s * 3 + 1] = static_cast<index_t>(base + s + 1);
        i[s * 3 + 2] = static_cast<index_t>(base + s + 2);
    }
}

void c_renderer_core::render_polygon(const vec2* pts, int count, color c, float thickness)
{
    if (clips_.empty_top() || count < 2) return;
    for (int s = 0; s < count; ++s)
        render_line(pts[s], pts[(s + 1) % count], c, thickness);
}

void c_renderer_core::render_bezier_quadratic(vec2 p0, vec2 p1, vec2 p2,
                                              color c, float thickness, int segments)
{
    if (segments < 2) segments = 2;
    vec2 prev = p0;
    for (int s = 1; s <= segments; ++s) {
        const float t = static_cast<float>(s) / segments;
        const float u = 1.0f - t;
        const vec2 cur{
            u * u * p0.x + 2 * u * t * p1.x + t * t * p2.x,
            u * u * p0.y + 2 * u * t * p1.y + t * t * p2.y,
        };
        render_line(prev, cur, c, thickness);
        prev = cur;
    }
}

void c_renderer_core::render_bezier_cubic(vec2 p0, vec2 p1, vec2 p2, vec2 p3,
                                          color c, float thickness, int segments)
{
    if (segments < 2) segments = 2;
    vec2 prev = p0;
    for (int s = 1; s <= segments; ++s) {
        const float t  = static_cast<float>(s) / segments;
        const float u  = 1.0f - t;
        const float uu = u * u, tt = t * t;
        const float uuu = uu * u, ttt = tt * t;
        const vec2 cur{
            uuu * p0.x + 3 * uu * t * p1.x + 3 * u * tt * p2.x + ttt * p3.x,
            uuu * p0.y + 3 * uu * t * p1.y + 3 * u * tt * p2.y + ttt * p3.y,
        };
        render_line(prev, cur, c, thickness);
        prev = cur;
    }
}

// ============================================================================
// Phase 2c: text
// ============================================================================

void c_renderer_core::render_text(const font& f, vec2 pos, color c, const char* text)
{
    if (clips_.empty_top() || !text || !*text) return;
    c = apply_alpha(c);

    const texture_handle atlas = f.atlas_handle();
    if (!atlas.srv) return;

    const std::uint32_t packed_col = c.packed();
    // NOTE: do NOT snap glyph positions to the pixel grid. The atlas is
    // baked with 2x2 oversampling (font.cpp stbtt_PackSetOversampling)
    // and sampled linear, which reconstructs crisp edges precisely
    // BECAUSE quads sit at fractional positions. Snapping defeated the
    // oversampling and made text blurrier + shifted baselines smaller.
    const float baseline_y = pos.y + f.ascent();

    const char* s   = text;
    const char* end = text + std::strlen(text);
    float pen_x = pos.x;

    while (s < end) {
        const auto cp = utf8_next(s, end);
        if (!cp) break;
        if (cp == '\n') {
            pen_x = pos.x;
            pos.y += f.line_advance();
            continue;
        }

        const glyph* g = f.find_glyph(cp);
        if (!g) {
            // Try the substitution character.
            g = f.find_glyph(0xFFFD);
            if (!g) continue;
        }

        const float x0 = pen_x      + g->bbox.x;
        const float y0 = baseline_y + g->bbox.y;
        const float x1 = x0 + g->bbox.w;
        const float y1 = y0 + g->bbox.h;

        const auto base = active_->prim_reserve(4, 6, atlas, clips_.top());
        vertex*  v = active_->vertices().data() + base;
        index_t* i = active_->indices ().data() + (active_->indices().size() - 6);

        // Vertices: matched UVs to atlas glyph box. The fill is solid
        // colour multiplied by atlas coverage --- per the inline PS.
        v[0].pos[0] = x0; v[0].pos[1] = y0; v[0].uv[0] = g->uv.x;           v[0].uv[1] = g->uv.y;           v[0].col = packed_col;
        v[1].pos[0] = x1; v[1].pos[1] = y0; v[1].uv[0] = g->uv.x + g->uv.w; v[1].uv[1] = g->uv.y;           v[1].col = packed_col;
        v[2].pos[0] = x1; v[2].pos[1] = y1; v[2].uv[0] = g->uv.x + g->uv.w; v[2].uv[1] = g->uv.y + g->uv.h; v[2].col = packed_col;
        v[3].pos[0] = x0; v[3].pos[1] = y1; v[3].uv[0] = g->uv.x;           v[3].uv[1] = g->uv.y + g->uv.h; v[3].col = packed_col;

        i[0] = static_cast<index_t>(base + 0);
        i[1] = static_cast<index_t>(base + 1);
        i[2] = static_cast<index_t>(base + 2);
        i[3] = static_cast<index_t>(base + 0);
        i[4] = static_cast<index_t>(base + 2);
        i[5] = static_cast<index_t>(base + 3);

        pen_x += g->advance;
    }
}

void c_renderer_core::render_text_drop_shadow(const font& f, vec2 pos,
                                              color c, color shadow, const char* text)
{
    c = apply_alpha(c);
    shadow = apply_alpha(shadow);
    render_text(f, vec2{pos.x + 1, pos.y + 1}, shadow, text);
    render_text(f, pos, c, text);
}

} // namespace Renderer
