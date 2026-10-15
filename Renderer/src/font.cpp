// =============================================================================
// font.cpp --- TTF rasterization via stb_truetype's PackBegin/PackEnd
// path (the higher-quality one) with 2x2 oversampling so small text
// stays crisp.
//
// Multiple disjoint Unicode ranges are packed at once: ASCII,
// Latin-1 Supplement, Latin Extended-A/B, Greek/Coptic, Cyrillic,
// General Punctuation (incl. NBSP, em/en dash, fancy quotes),
// Arrows, Math Operators, Box Drawing, Block Elements, Geometric
// Shapes. That covers everything the unicode_text_demo in the binary
// exercises plus the common control glyphs.
//
// At runtime the atlas is uploaded as RGBA8 (white RGB + coverage in
// alpha) so the verbatim pixel shader from dec_ShaderInit.c works
// unchanged.
// =============================================================================

#include <Renderer/font.hpp>

#include <Windows.h>
#include <d3d11.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4505)
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <stb/stb_truetype.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace Renderer {

namespace {
template <class T>
inline void safe_release(T*& p) { if (p) { p->Release(); p = nullptr; } }

std::vector<unsigned char> read_file(const wchar_t* path) {
    std::vector<unsigned char> data;
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"rb") != 0 || !f) return data;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size > 0) {
        data.resize(static_cast<std::size_t>(size));
        std::fread(data.data(), 1, data.size(), f);
    }
    std::fclose(f);
    return data;
}

const std::vector<font::unicode_range>& default_ranges() {
    static const std::vector<font::unicode_range> r = {
        {0x0020, 0x007E},   // Basic Latin (ASCII printable)
        {0x00A0, 0x00FF},   // Latin-1 Supplement
        {0x0100, 0x017F},   // Latin Extended-A
        {0x0180, 0x024F},   // Latin Extended-B
        {0x0370, 0x03FF},   // Greek + Coptic
        {0x0400, 0x04FF},   // Cyrillic
        {0x2000, 0x206F},   // General Punctuation
        {0x2070, 0x209F},   // Super/Subscripts
        {0x20A0, 0x20CF},   // Currency
        {0x2100, 0x214F},   // Letterlike Symbols
        {0x2150, 0x218F},   // Number Forms (fractions)
        {0x2190, 0x21FF},   // Arrows
        {0x2200, 0x22FF},   // Mathematical Operators
        {0x2500, 0x257F},   // Box Drawing
        {0x2580, 0x259F},   // Block Elements
        {0x25A0, 0x25FF},   // Geometric Shapes
        {0x2600, 0x26FF},   // Misc Symbols
        {0x2700, 0x27BF},   // Dingbats
        {0xFFFD, 0xFFFD},   // Replacement character
    };
    return r;
}
} // anon

font::~font() { release(); }

void font::release() {
    safe_release(srv_);
    safe_release(tex_);
    glyphs_.clear();
}

bool font::load_from_file(ID3D11Device* device, const wchar_t* path,
                          float pixel_height, bool aliased)
{
    return load_from_file_with_ranges(device, path, pixel_height, aliased,
                                      default_ranges());
}

bool font::load_from_file_with_ranges(ID3D11Device* device, const wchar_t* path,
                                      float pixel_height, bool aliased,
                                      const std::vector<unicode_range>& ranges)
{
    if (!device || !path || pixel_height <= 0 || ranges.empty()) return false;

    auto file = read_file(path);
    if (file.empty()) return false;

    return load_from_memory_with_ranges(device, file.data(), file.size(),
                                        pixel_height, aliased, ranges);
}

bool font::load_from_memory_with_ranges(ID3D11Device* device,
                                        const unsigned char* data,
                                        std::size_t size,
                                        float pixel_height, bool aliased,
                                        const std::vector<unicode_range>& ranges)
{
    if (!device || !data || size == 0 || pixel_height <= 0 || ranges.empty())
        return false;

    // stb reads through `file.data()` below; alias the caller's blob so the
    // rest of the baker (lifted verbatim from the old file path) is unchanged.
    const unsigned char* file_ptr = data;

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, file_ptr,
                        stbtt_GetFontOffsetForIndex(file_ptr, 0))) return false;

    const float scale = stbtt_ScaleForPixelHeight(&info, pixel_height);
    int asc = 0, desc = 0, gap = 0;
    stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);
    ascent_       = asc  * scale;
    descent_      = desc * scale;
    line_gap_     = gap  * scale;
    pixel_height_ = pixel_height;

    // Build the stbtt_pack_range descriptors.
    std::vector<stbtt_pack_range> pack_ranges;
    std::vector<std::vector<stbtt_packedchar>> pack_chars(ranges.size());
    pack_ranges.reserve(ranges.size());
    for (std::size_t i = 0; i < ranges.size(); ++i) {
        const auto& rg = ranges[i];
        const int count = static_cast<int>(rg.last - rg.first + 1);
        pack_chars[i].resize(static_cast<std::size_t>(count));

        stbtt_pack_range pr{};
        pr.font_size                        = pixel_height;
        pr.first_unicode_codepoint_in_range = static_cast<int>(rg.first);
        pr.array_of_unicode_codepoints      = nullptr;
        pr.num_chars                        = count;
        pr.chardata_for_range               = pack_chars[i].data();
        pack_ranges.push_back(pr);
    }

    // Grow the atlas until it fits everything we asked for. PackBegin
    // returns 1 always; PackFontRanges returns 1 if every glyph fit.
    // Start at 1024 so 2x2 oversampling for the default range set has
    // headroom (the earlier 512 would force a grow-and-retry).
    int atlas_w = 1024, atlas_h = 1024;
    std::vector<unsigned char> atlas;
    int ok = 0;
    while (atlas_w <= 4096) {
        atlas.assign(static_cast<std::size_t>(atlas_w) * atlas_h, 0);
        stbtt_pack_context pc;
        if (!stbtt_PackBegin(&pc, atlas.data(), atlas_w, atlas_h,
                             /*stride=*/atlas_w, /*padding=*/1, /*alloc_ctx=*/nullptr)) {
            return false;
        }
        // 2x2 oversampling --- the major reason small text in the
        // original looks crisp. Trades ~4x atlas pixels for sharpness.
        stbtt_PackSetOversampling(&pc, 2, 2);
        ok = stbtt_PackFontRanges(&pc, file_ptr, /*font_index=*/0,
                                  pack_ranges.data(), static_cast<int>(pack_ranges.size()));
        stbtt_PackEnd(&pc);
        if (ok) break;
        if (atlas_w <= atlas_h) atlas_w *= 2; else atlas_h *= 2;
    }
    if (!ok) return false;

    // Translate packed metrics into our glyph map.
    const float inv_w = 1.0f / atlas_w;
    const float inv_h = 1.0f / atlas_h;
    glyphs_.clear();
    glyphs_.reserve(2048);

    // Debug accounting: per-range glyph counts written to font_load.log
    // so we can see which ranges actually contained renderable glyphs.
    FILE* dbg = nullptr;
    if (_wfopen_s(&dbg, L"font_load.log", L"a") == 0 && dbg) {
        std::fprintf(dbg, "=== load <blob %zu bytes> @ %.0fpx atlas=%dx%d ===\n",
                     size, pixel_height, atlas_w, atlas_h);
    }

    for (std::size_t i = 0; i < ranges.size(); ++i) {
        const auto& rg = ranges[i];
        int range_packed = 0;
        for (std::uint32_t cp = rg.first; cp <= rg.last; ++cp) {
            const auto& pc = pack_chars[i][cp - rg.first];
            // Skip glyphs the font doesn't define (zero box, zero advance).
            if (pc.x0 == pc.x1 || pc.y0 == pc.y1) continue;
            ++range_packed;
            glyph g;
            g.uv      = rect{pc.x0 * inv_w, pc.y0 * inv_h,
                             (pc.x1 - pc.x0) * inv_w, (pc.y1 - pc.y0) * inv_h};
            // CRITICAL: with oversampling, x1-x0 is the *atlas*-pixel
            // size (2x with 2x2 oversample), not the screen-pixel size.
            // Use xoff/xoff2/yoff/yoff2 which are real screen-pixel
            // offsets from the pen position.
            g.bbox    = rect{pc.xoff, pc.yoff,
                             pc.xoff2 - pc.xoff,
                             pc.yoff2 - pc.yoff};
            g.advance = pc.xadvance;
            glyphs_[cp] = g;
        }
        if (dbg) {
            std::fprintf(dbg, "  range 0x%04X..0x%04X --> %d glyphs\n",
                         rg.first, rg.last, range_packed);
        }
    }
    if (dbg) { std::fprintf(dbg, "  total glyphs: %zu\n", glyphs_.size()); std::fclose(dbg); }

    // Expand R8 coverage -> RGBA8 white-on-alpha (the verbatim PS
    // from dec_ShaderInit.c samples .rgba and multiplies vertex.col).
    std::vector<std::uint32_t> rgba(static_cast<std::size_t>(atlas_w) * atlas_h);
    for (std::size_t p = 0; p < rgba.size(); ++p) {
        const std::uint32_t a = atlas[p];
        rgba[p] = 0x00FFFFFFu | (a << 24);
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width  = static_cast<UINT>(atlas_w);
    td.Height = static_cast<UINT>(atlas_h);
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage     = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem     = rgba.data();
    init.SysMemPitch = static_cast<UINT>(atlas_w * 4);

    if (FAILED(device->CreateTexture2D(&td, &init, &tex_))) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
    svd.Format              = td.Format;
    svd.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    svd.Texture2D.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(tex_, &svd, &srv_))) {
        release();
        return false;
    }

    (void)aliased;  // Phase 2c: same linear sampler in both modes.
    return true;
}

const glyph* font::find_glyph(std::uint32_t cp) const {
    auto it = glyphs_.find(cp);
    return it == glyphs_.end() ? nullptr : &it->second;
}

float font::measure_text(const std::string& text) const {
    float w = 0;
    const char* s   = text.c_str();
    const char* end = s + text.size();
    while (s < end) {
        const auto cp = utf8_next(s, end);
        if (!cp) break;
        if (const glyph* g = find_glyph(cp)) w += g->advance;
        else if (const glyph* g2 = find_glyph(0xFFFD)) w += g2->advance;
    }
    return w;
}

std::uint32_t utf8_next(const char*& s, const char* end) {
    if (s >= end) return 0;
    const unsigned char c0 = static_cast<unsigned char>(*s++);
    if (c0 < 0x80) return c0;
    auto bad = []{ return 0xFFFDu; };

    auto take = [&](int n) -> std::uint32_t {
        if (s + n > end) return bad();
        std::uint32_t cp = 0;
        switch (n) {
            case 1: cp = c0 & 0x1F; break;
            case 2: cp = c0 & 0x0F; break;
            case 3: cp = c0 & 0x07; break;
        }
        for (int i = 0; i < n; ++i) {
            const unsigned char b = static_cast<unsigned char>(*s++);
            if ((b & 0xC0) != 0x80) return bad();
            cp = (cp << 6) | (b & 0x3F);
        }
        return cp;
    };

    if ((c0 & 0xE0) == 0xC0) return take(1);
    if ((c0 & 0xF0) == 0xE0) return take(2);
    if ((c0 & 0xF8) == 0xF0) return take(3);
    return bad();
}

} // namespace Renderer
