#pragma once
// =============================================================================
// Renderer::font --- glyph atlas backed by a single greyscale texture.
//
// The original binary statically links FreeType (RTTI strings include
// "truetype-engine", "hinting-engine", "sfnt-table" etc.). For
// reconstruction Phase 2c we use stb_truetype as a stand-in --- same
// API surface, swap underneath later for byte parity. Both produce
// 8-bit greyscale coverage that we upload to R8_UNORM and sample in
// the pixel shader (the green channel ends up in .r, fine for alpha
// masking via input.col * coverage).
//
// One font owns:
//   - cooked metrics for every codepoint in `range_begin..range_end`
//   - a single greyscale atlas image
//   - a DX11 SRV bound when render_text emits text quads
//
// "Aliased" mode (Aug 5-8 changelog: "Add flag to initialize an
// aliased font") just turns off the linear filter --- we expose the
// flag on the loader.
// =============================================================================

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <Renderer/draw_list.hpp>
#include <Renderer/geometry.hpp>

struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

namespace Renderer {

struct glyph {
    rect  uv {};       // 0..1 atlas coords
    rect  bbox {};     // local pixel box, relative to pen
    float advance = 0; // horizontal advance in pixels
};

class font {
public:
    font() = default;
    ~font();

    // A single contiguous codepoint range to bake into the atlas.
    struct unicode_range {
        std::uint32_t first;   // inclusive
        std::uint32_t last;    // inclusive
    };

    bool load_from_file(ID3D11Device* device,
                        const wchar_t* path,
                        float pixel_height,
                        bool aliased = false);

    // As above, but with an explicit set of disjoint Unicode ranges.
    // Empty `ranges` defaults to ASCII + Latin-1 + Greek + Cyrillic +
    // General Punctuation + Arrows + Block Elements + Geometric Shapes.
    bool load_from_file_with_ranges(ID3D11Device* device,
                                    const wchar_t* path,
                                    float pixel_height,
                                    bool aliased,
                                    const std::vector<unicode_range>& ranges);

    // Bake directly from an in-memory TTF/OTF blob (e.g. an embedded
    // byte array). `data` must remain valid only for the duration of
    // this call. Lets the demo ship self-contained without an asset dir.
    bool load_from_memory_with_ranges(ID3D11Device* device,
                                      const unsigned char* data,
                                      std::size_t size,
                                      float pixel_height,
                                      bool aliased,
                                      const std::vector<unicode_range>& ranges);

    void release();

    float pixel_height() const { return pixel_height_; }
    float ascent      () const { return ascent_; }
    float descent     () const { return descent_; }
    float line_gap    () const { return line_gap_; }
    float line_advance() const { return ascent_ - descent_ + line_gap_; }

    // Returns nullptr if codepoint is not in the cooked range.
    const glyph* find_glyph(std::uint32_t codepoint) const;

    // Width of a UTF-8 string in pixels (no shaping, no kerning).
    float measure_text(const std::string& text) const;

    texture_handle atlas_handle() const { return texture_handle{srv_}; }

private:
    ID3D11Texture2D*          tex_ = nullptr;
    ID3D11ShaderResourceView* srv_ = nullptr;

    float pixel_height_ = 0;
    float ascent_       = 0;
    float descent_      = 0;
    float line_gap_     = 0;

    std::unordered_map<std::uint32_t, glyph> glyphs_;
};

// UTF-8 → codepoint iterator. Returns next codepoint or 0 if `s` is at
// the end; advances `s` past the consumed bytes. Invalid sequences are
// replaced with U+FFFD.
std::uint32_t utf8_next(const char*& s, const char* end);

} // namespace Renderer
