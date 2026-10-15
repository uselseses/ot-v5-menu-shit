#pragma once
// Packed RGBA color stored as uint32 in memory order R,G,B,A.
// Matches DXGI_FORMAT_R8G8B8A8_UNORM exactly --- on a little-endian box
// the uint32 reads as 0xAABBGGRR. This is what the vertex stream feeds
// the COLOR0 input slot (see Renderer/vertex.hpp + dec_ShaderInit.c).

#include <cstdint>

namespace Renderer {

struct color {
    std::uint8_t r{}, g{}, b{}, a{};

    constexpr color() = default;
    constexpr color(std::uint8_t R, std::uint8_t G, std::uint8_t B,
                    std::uint8_t A = 255) : r{R}, g{G}, b{B}, a{A} {}

    constexpr std::uint32_t packed() const noexcept {
        return  static_cast<std::uint32_t>(r)
             | (static_cast<std::uint32_t>(g) <<  8)
             | (static_cast<std::uint32_t>(b) << 16)
             | (static_cast<std::uint32_t>(a) << 24);
    }

    static constexpr color from_packed(std::uint32_t v) noexcept {
        return color{
            static_cast<std::uint8_t>( v        & 0xFF),
            static_cast<std::uint8_t>((v >>  8) & 0xFF),
            static_cast<std::uint8_t>((v >> 16) & 0xFF),
            static_cast<std::uint8_t>((v >> 24) & 0xFF)};
    }

    constexpr color with_alpha(std::uint8_t new_a) const noexcept {
        return color{r, g, b, new_a};
    }

    // Exact from binary lerp_color @ 0x14000E750
    static color lerp(color a, color b, float t) noexcept {
        if (t <= 0.0f) return a;
        if (t >= 1.0f) return b;
        if (a.packed() == b.packed()) return a;
        const float inv_t = 1.0f - t;
        return color{
            static_cast<std::uint8_t>(a.r * inv_t + b.r * t),
            static_cast<std::uint8_t>(a.g * inv_t + b.g * t),
            static_cast<std::uint8_t>(a.b * inv_t + b.b * t),
            static_cast<std::uint8_t>(a.a * inv_t + b.a * t)
        };
    }
};

// Named convenience constants used by the framework's default palette.
namespace colors {
    inline constexpr color black     {  0,   0,   0};
    inline constexpr color white     {255, 255, 255};
    inline constexpr color red       {255,   0,   0};
    inline constexpr color green     {  0, 255,   0};
    inline constexpr color blue      {  0,   0, 255};
    inline constexpr color transparent{0,   0,   0, 0};
}

} // namespace Renderer
