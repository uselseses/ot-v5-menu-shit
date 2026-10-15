// =============================================================================
// font_d3d9.cpp — D3D9 parallel of font.cpp.
//
// Glyph baking (stb_truetype + the same Unicode ranges + 2x2 oversampling)
// is identical to the DX11 path. Only the atlas upload differs — here we
// create an IDirect3DTexture9 in D3DPOOL_MANAGED so it survives device
// Reset without us having to recreate it.
// =============================================================================

#include <Renderer/font_d3d9.hpp>

#include <Windows.h>
#include <d3d9.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

// stb_truetype: with STBTT_STATIC the functions are file-local, so each
// TU needs its own implementation. font.cpp has its own; we include
// our own here to avoid pulling external linkage in.
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

const std::vector<font_d3d9::unicode_range>& default_ranges() {
    static const std::vector<font_d3d9::unicode_range> r = {
        {0x0020, 0x007E}, {0x00A0, 0x00FF}, {0x0100, 0x017F},
        {0x0180, 0x024F}, {0x0370, 0x03FF}, {0x0400, 0x04FF},
        {0x2000, 0x206F}, {0x2070, 0x209F}, {0x20A0, 0x20CF},
        {0x2100, 0x214F}, {0x2150, 0x218F}, {0x2190, 0x21FF},
        {0x2200, 0x22FF}, {0x2500, 0x257F}, {0x2580, 0x259F},
        {0x25A0, 0x25FF}, {0x2600, 0x26FF}, {0x2700, 0x27BF},
        {0xFFFD, 0xFFFD},
    };
    return r;
}

} // anon

font_d3d9::~font_d3d9() { release(); }

void font_d3d9::release() {
    safe_release(tex_);
    glyphs_.clear();
}

texture_handle font_d3d9::atlas_handle() const {
    // c_renderer_core treats texture_handle::srv as an opaque void*.
    // The D3D9 backend (c_d3d9_renderer) reinterprets it as
    // IDirect3DTexture9* in issue_cmds.
    return texture_handle{ reinterpret_cast<void*>(tex_) };
}

bool font_d3d9::load_from_file(IDirect3DDevice9* device, const wchar_t* path,
                               float pixel_height, bool aliased)
{
    return load_from_file_with_ranges(device, path, pixel_height, aliased,
                                      default_ranges());
}

bool font_d3d9::load_from_file_with_ranges(IDirect3DDevice9* device,
                                           const wchar_t* path,
                                           float pixel_height, bool aliased,
                                           const std::vector<unicode_range>& ranges)
{
    if (!device || !path || pixel_height <= 0 || ranges.empty()) return false;

    auto file = read_file(path);
    if (file.empty()) return false;

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, file.data(),
                        stbtt_GetFontOffsetForIndex(file.data(), 0))) return false;

    const float scale = stbtt_ScaleForPixelHeight(&info, pixel_height);
    int asc = 0, desc = 0, gap = 0;
    stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);
    ascent_       = asc  * scale;
    descent_      = desc * scale;
    line_gap_     = gap  * scale;
    pixel_height_ = pixel_height;

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

    int atlas_w = 1024, atlas_h = 1024;
    std::vector<unsigned char> atlas;
    int ok = 0;
    while (atlas_w <= 4096) {
        atlas.assign(static_cast<std::size_t>(atlas_w) * atlas_h, 0);
        stbtt_pack_context pc;
        if (!stbtt_PackBegin(&pc, atlas.data(), atlas_w, atlas_h,
                             atlas_w, 1, nullptr)) {
            return false;
        }
        stbtt_PackSetOversampling(&pc, 2, 2);
        ok = stbtt_PackFontRanges(&pc, file.data(), 0,
                                  pack_ranges.data(),
                                  static_cast<int>(pack_ranges.size()));
        stbtt_PackEnd(&pc);
        if (ok) break;
        if (atlas_w <= atlas_h) atlas_w *= 2; else atlas_h *= 2;
    }
    if (!ok) return false;

    const float inv_w = 1.0f / atlas_w;
    const float inv_h = 1.0f / atlas_h;
    glyphs_.clear();
    glyphs_.reserve(2048);

    for (std::size_t i = 0; i < ranges.size(); ++i) {
        const auto& rg = ranges[i];
        for (std::uint32_t cp = rg.first; cp <= rg.last; ++cp) {
            const auto& pc = pack_chars[i][cp - rg.first];
            if (pc.x0 == pc.x1 || pc.y0 == pc.y1) continue;
            glyph g;
            g.uv      = rect{pc.x0 * inv_w, pc.y0 * inv_h,
                             (pc.x1 - pc.x0) * inv_w,
                             (pc.y1 - pc.y0) * inv_h};
            g.bbox    = rect{pc.xoff, pc.yoff,
                             pc.xoff2 - pc.xoff,
                             pc.yoff2 - pc.yoff};
            g.advance = pc.xadvance;
            glyphs_[cp] = g;
        }
    }

    // Expand R8 coverage -> A8R8G8B8 (white RGB + coverage in alpha).
    // D3D9 uses BGRA on-disk but the SetTexture sampler returns the
    // four components in shader order — the MODULATE op multiplies
    // texture.a by diffuse.a, which is what we want.
    std::vector<std::uint32_t> argb(static_cast<std::size_t>(atlas_w) * atlas_h);
    for (std::size_t p = 0; p < argb.size(); ++p) {
        const std::uint32_t a = atlas[p];
        // D3DFMT_A8R8G8B8 is stored as 0xAARRGGBB little-endian.
        argb[p] = (a << 24) | 0x00FFFFFFu;
    }

    HRESULT hr = device->CreateTexture(
        static_cast<UINT>(atlas_w),
        static_cast<UINT>(atlas_h),
        1,
        0,
        D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED,
        &tex_, nullptr);
    if (FAILED(hr) || !tex_) return false;

    D3DLOCKED_RECT lr{};
    if (FAILED(tex_->LockRect(0, &lr, nullptr, 0))) {
        release();
        return false;
    }
    auto* dst = static_cast<std::uint8_t*>(lr.pBits);
    const std::uint32_t row_bytes = static_cast<std::uint32_t>(atlas_w) * 4;
    for (int y = 0; y < atlas_h; ++y) {
        std::memcpy(dst + static_cast<std::size_t>(y) * lr.Pitch,
                    argb.data() + static_cast<std::size_t>(y) * atlas_w,
                    row_bytes);
    }
    tex_->UnlockRect(0);

    (void)aliased;
    return true;
}

const glyph* font_d3d9::find_glyph(std::uint32_t cp) const {
    auto it = glyphs_.find(cp);
    return it == glyphs_.end() ? nullptr : &it->second;
}

float font_d3d9::measure_text(const std::string& text) const {
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

} // namespace Renderer
