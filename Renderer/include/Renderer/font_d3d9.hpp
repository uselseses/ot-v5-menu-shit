#pragma once
// =============================================================================
// Renderer::font_d3d9 — D3D9 parallel of Renderer::font.
//
// Same glyph-baking pipeline (stb_truetype, ASCII + Latin-1 etc.) but
// uploads the greyscale atlas to a D3D9 IDirect3DTexture9 in
// D3DPOOL_MANAGED (so the atlas survives device Reset automatically).
//
// Public API mirrors Renderer::font so widget code can use either one
// by typedef. The only difference is the `load_from_file` parameter:
// it takes an IDirect3DDevice9* instead of an ID3D11Device*.
// =============================================================================

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <Renderer/draw_list.hpp>
#include <Renderer/geometry.hpp>
#include <Renderer/font.hpp>   // re-uses `glyph`, `unicode_range`, utf8_next

struct IDirect3DDevice9;
struct IDirect3DTexture9;

namespace Renderer {

class font_d3d9 {
public:
    font_d3d9() = default;
    ~font_d3d9();

    using unicode_range = Renderer::font::unicode_range;

    bool load_from_file(IDirect3DDevice9* device,
                        const wchar_t* path,
                        float pixel_height,
                        bool aliased = false);

    bool load_from_file_with_ranges(IDirect3DDevice9* device,
                                    const wchar_t* path,
                                    float pixel_height,
                                    bool aliased,
                                    const std::vector<unicode_range>& ranges);

    void release();

    float pixel_height() const { return pixel_height_; }
    float ascent      () const { return ascent_; }
    float descent     () const { return descent_; }
    float line_gap    () const { return line_gap_; }
    float line_advance() const { return ascent_ - descent_ + line_gap_; }

    const glyph* find_glyph(std::uint32_t codepoint) const;
    float        measure_text(const std::string& text) const;

    // c.texture.srv is reinterpreted as IDirect3DTexture9* by the D3D9
    // backend. The void* is opaque from c_renderer_core's perspective.
    texture_handle atlas_handle() const;

private:
    IDirect3DTexture9* tex_ = nullptr;

    float pixel_height_ = 0;
    float ascent_       = 0;
    float descent_      = 0;
    float line_gap_     = 0;

    std::unordered_map<std::uint32_t, glyph> glyphs_;
};

} // namespace Renderer
