#include <Renderer/texture.hpp>

#include <Renderer/draw_list.hpp>

#ifdef _WIN32
#include <d3d11.h>
#endif

#include <algorithm>
#include <cmath>
#include <fstream>

namespace Renderer {

namespace {
template <class T>
inline void safe_release(T*& p) { if (p) { p->Release(); p = nullptr; } }
} // anon

c_texture::~c_texture()
{
#ifdef _WIN32
    safe_release(srv_);
#endif
}

c_texture::c_texture(c_texture&& other) noexcept
{
    *this = std::move(other);
}

c_texture& c_texture::operator=(c_texture&& other) noexcept
{
    if (this == &other) return *this;
#ifdef _WIN32
    safe_release(srv_);
    srv_ = other.srv_;
    other.srv_ = nullptr;
#endif
    width_ = other.width_;
    height_ = other.height_;
    other.width_ = 0;
    other.height_ = 0;
    return *this;
}

bool c_texture::create(ID3D11Device* device, std::uint32_t width, std::uint32_t height,
                       const std::uint8_t* rgba_data)
{
#ifndef _WIN32
    (void)device; (void)width; (void)height; (void)rgba_data;
    return false;
#else
    if (!device || !rgba_data || width == 0 || height == 0) return false;
    safe_release(srv_);

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = rgba_data;
    init.SysMemPitch = width * 4;

    ID3D11Texture2D* texture = nullptr;
    if (FAILED(device->CreateTexture2D(&td, &init, &texture))) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
    svd.Format = td.Format;
    svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    svd.Texture2D.MipLevels = 1;
    const HRESULT hr = device->CreateShaderResourceView(texture, &svd, &srv_);
    texture->Release();
    if (FAILED(hr)) return false;

    width_ = width;
    height_ = height;
    return true;
#endif
}

bool c_texture::load_raw_rgba(ID3D11Device* device, const std::filesystem::path& path,
                              std::uint32_t width, std::uint32_t height)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(width) * height * 4);
    f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (f.gcount() != static_cast<std::streamsize>(bytes.size())) return false;
    return create(device, width, height, bytes.data());
}

bool c_texture::create_from_argb(ID3D11Device* device, std::uint32_t width, std::uint32_t height,
                                 const std::uint32_t* argb_data)
{
    if (!argb_data) return false;
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i) {
        const std::uint32_t c = argb_data[i];
        rgba[i * 4 + 0] = static_cast<std::uint8_t>((c >> 16) & 0xFF);
        rgba[i * 4 + 1] = static_cast<std::uint8_t>((c >>  8) & 0xFF);
        rgba[i * 4 + 2] = static_cast<std::uint8_t>( c        & 0xFF);
        rgba[i * 4 + 3] = static_cast<std::uint8_t>((c >> 24) & 0xFF);
    }
    return create(device, width, height, rgba.data());
}

bool c_texture::valid() const
{
#ifdef _WIN32
    return srv_ != nullptr;
#else
    return false;
#endif
}

// ============================================================================
// Procedural texture generators
//
// The binary loads a packed/compressed PNG-ish blob at `qword_14023A4C0`
// (84x48 g_texture_arrow) and unpacks it via `sub_140093440` / `sub_1400A2340`
// (see `decompiled/gui_init_textures.c:76-88, 280-288`). Without the unpacker
// source we can't reproduce its exact pixels; the procedural V chevron below
// matches the visual shape the binary's texture renders.
// ============================================================================

namespace {
inline float dist_to_segment(float px, float py,
                             float ax, float ay,
                             float bx, float by)
{
    const float dx = bx - ax, dy = by - ay;
    const float len2 = dx * dx + dy * dy;
    if (len2 < 1e-6f) {
        const float ex = px - ax, ey = py - ay;
        return std::sqrt(ex * ex + ey * ey);
    }
    float t = ((px - ax) * dx + (py - ay) * dy) / len2;
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
    const float cx = ax + dx * t, cy = ay + dy * t;
    const float ex = px - cx, ey = py - cy;
    return std::sqrt(ex * ex + ey * ey);
}
} // anon

std::vector<std::uint32_t> generate_chevron_pixels(std::uint32_t w,
                                                    std::uint32_t h)
{
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(w) * h, 0u);

    // Down-pointing V chevron. Coordinates calibrated to fit comfortably
    // inside the texture with a small margin so the tip + wings clear
    // their respective edges even after rotation.
    const float fw = static_cast<float>(w);
    const float fh = static_cast<float>(h);
    const float thickness = std::min(fw, fh) * 0.28f;
    const float half_t    = thickness * 0.5f;

    const float p_left_x  = fw * 0.18f, p_left_y  = fh * 0.28f;
    const float p_right_x = fw * 0.82f, p_right_y = fh * 0.28f;
    const float p_tip_x   = fw * 0.50f, p_tip_y   = fh * 0.78f;

    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            const float d1 = dist_to_segment(px, py,
                                             p_left_x, p_left_y,
                                             p_tip_x,  p_tip_y);
            const float d2 = dist_to_segment(px, py,
                                             p_tip_x,   p_tip_y,
                                             p_right_x, p_right_y);
            const float d  = std::min(d1, d2);
            // 1-px soft edge centred on the stroke radius.
            float alpha = half_t + 0.5f - d;
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            const std::uint8_t a = static_cast<std::uint8_t>(alpha * 255.0f);
            // ARGB: white RGB + mask in alpha so the tint colour passed to
            // render_rotated_texture multiplies straight through.
            pixels[static_cast<std::size_t>(y) * w + x] =
                (static_cast<std::uint32_t>(a) << 24) | 0x00FFFFFFu;
        }
    }
    return pixels;
}

} // namespace Renderer
