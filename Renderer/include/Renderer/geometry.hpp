#pragma once
// Tiny 2-D geometry primitives. Pure value types --- no D3D / OS deps.

namespace Renderer {

struct vec2 {
    float x{}, y{};

    constexpr vec2() = default;
    constexpr vec2(float X, float Y) : x{X}, y{Y} {}

    constexpr vec2 operator+(vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr vec2 operator-(vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct rect {
    float x{}, y{}, w{}, h{};

    constexpr rect() = default;
    constexpr rect(float X, float Y, float W, float H) : x{X}, y{Y}, w{W}, h{H} {}

    constexpr float right () const { return x + w; }
    constexpr float bottom() const { return y + h; }

    constexpr bool empty() const { return w <= 0 || h <= 0; }

    constexpr bool contains(vec2 p) const {
        return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
    }

    // Smallest axis-aligned rect containing both inputs.
    static constexpr rect intersect(rect a, rect b) {
        const float L = a.x > b.x ? a.x : b.x;
        const float T = a.y > b.y ? a.y : b.y;
        const float R = a.right () < b.right () ? a.right () : b.right ();
        const float B = a.bottom() < b.bottom() ? a.bottom() : b.bottom();
        return (R > L && B > T) ? rect{L, T, R - L, B - T} : rect{};
    }
};

} // namespace Renderer
