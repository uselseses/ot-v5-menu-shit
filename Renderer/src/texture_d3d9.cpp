// =============================================================================
// texture_d3d9.cpp — D3D9 parallel of texture.cpp.
//
// D3DPOOL_MANAGED so the texture survives device Reset.
// =============================================================================

#include <Renderer/texture_d3d9.hpp>

#include <Windows.h>
#include <d3d9.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace Renderer {

namespace {

template <class T>
inline void safe_release(T*& p) { if (p) { p->Release(); p = nullptr; } }

// Upload a packed BGRA (=D3DFMT_A8R8G8B8) image to a freshly created
// MANAGED texture. Returns the texture or nullptr.
IDirect3DTexture9* upload_argb(IDirect3DDevice9* dev,
                               std::uint32_t w, std::uint32_t h,
                               const std::uint32_t* argb)
{
    IDirect3DTexture9* t = nullptr;
    if (FAILED(dev->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8,
                                  D3DPOOL_MANAGED, &t, nullptr)) || !t) {
        return nullptr;
    }
    D3DLOCKED_RECT lr{};
    if (FAILED(t->LockRect(0, &lr, nullptr, 0))) {
        t->Release();
        return nullptr;
    }
    auto* dst = static_cast<std::uint8_t*>(lr.pBits);
    const std::uint32_t row_bytes = w * 4;
    for (std::uint32_t y = 0; y < h; ++y) {
        std::memcpy(dst + static_cast<std::size_t>(y) * lr.Pitch,
                    argb + static_cast<std::size_t>(y) * w,
                    row_bytes);
    }
    t->UnlockRect(0);
    return t;
}

} // anon

c_texture_d3d9::~c_texture_d3d9() { safe_release(tex_); }

c_texture_d3d9::c_texture_d3d9(c_texture_d3d9&& o) noexcept
    : width_(o.width_), height_(o.height_), tex_(o.tex_) {
    o.tex_ = nullptr; o.width_ = o.height_ = 0;
}

c_texture_d3d9& c_texture_d3d9::operator=(c_texture_d3d9&& o) noexcept {
    if (this != &o) {
        safe_release(tex_);
        width_ = o.width_; height_ = o.height_; tex_ = o.tex_;
        o.tex_ = nullptr; o.width_ = o.height_ = 0;
    }
    return *this;
}

bool c_texture_d3d9::valid() const { return tex_ != nullptr; }

texture_handle c_texture_d3d9::handle() const {
    return texture_handle{ reinterpret_cast<void*>(tex_) };
}

bool c_texture_d3d9::create(IDirect3DDevice9* dev, std::uint32_t w, std::uint32_t h,
                            const std::uint8_t* rgba)
{
    if (!dev || w == 0 || h == 0 || !rgba) return false;
    // Repack RGBA -> A8R8G8B8 (little-endian).
    std::vector<std::uint32_t> argb(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < argb.size(); ++i) {
        const std::uint8_t r = rgba[i * 4 + 0];
        const std::uint8_t g = rgba[i * 4 + 1];
        const std::uint8_t b = rgba[i * 4 + 2];
        const std::uint8_t a = rgba[i * 4 + 3];
        argb[i] = (static_cast<std::uint32_t>(a) << 24)
                | (static_cast<std::uint32_t>(r) << 16)
                | (static_cast<std::uint32_t>(g) << 8)
                |  static_cast<std::uint32_t>(b);
    }
    safe_release(tex_);
    tex_ = upload_argb(dev, w, h, argb.data());
    if (!tex_) return false;
    width_  = w;
    height_ = h;
    return true;
}

bool c_texture_d3d9::load_raw_rgba(IDirect3DDevice9* dev,
                                   const std::filesystem::path& path,
                                   std::uint32_t w, std::uint32_t h)
{
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.wstring().c_str(), L"rb") != 0 || !f) return false;
    std::vector<std::uint8_t> data(static_cast<std::size_t>(w) * h * 4);
    const auto read = std::fread(data.data(), 1, data.size(), f);
    std::fclose(f);
    if (read != data.size()) return false;
    return create(dev, w, h, data.data());
}

bool c_texture_d3d9::create_from_argb(IDirect3DDevice9* dev,
                                      std::uint32_t w, std::uint32_t h,
                                      const std::uint32_t* argb)
{
    if (!dev || w == 0 || h == 0 || !argb) return false;
    safe_release(tex_);
    tex_ = upload_argb(dev, w, h, argb);
    if (!tex_) return false;
    width_  = w;
    height_ = h;
    return true;
}

} // namespace Renderer
