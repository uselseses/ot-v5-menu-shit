#pragma once
// =============================================================================
// Renderer::c_texture_d3d9 — D3D9 parallel of c_texture.
//
// Wraps an IDirect3DTexture9 in D3DPOOL_MANAGED. Stores its raw pointer
// inside texture_handle.srv so c_renderer_core can bind it through the
// same draw_cmd path as the DX11 backend.
// =============================================================================

#include <cstdint>
#include <filesystem>
#include <vector>

#include <Renderer/draw_list.hpp>

struct IDirect3DDevice9;
struct IDirect3DTexture9;

namespace Renderer {

class c_texture_d3d9 {
public:
    c_texture_d3d9() = default;
    ~c_texture_d3d9();

    c_texture_d3d9(const c_texture_d3d9&) = delete;
    c_texture_d3d9& operator=(const c_texture_d3d9&) = delete;
    c_texture_d3d9(c_texture_d3d9&& other) noexcept;
    c_texture_d3d9& operator=(c_texture_d3d9&& other) noexcept;

    bool create(IDirect3DDevice9* device,
                std::uint32_t width, std::uint32_t height,
                const std::uint8_t* rgba_data);

    bool load_raw_rgba(IDirect3DDevice9* device,
                       const std::filesystem::path& path,
                       std::uint32_t width, std::uint32_t height);

    bool create_from_argb(IDirect3DDevice9* device,
                          std::uint32_t width, std::uint32_t height,
                          const std::uint32_t* argb_data);

    std::uint32_t width () const { return width_;  }
    std::uint32_t height() const { return height_; }
    bool valid() const;

    IDirect3DTexture9* native() const { return tex_; }
    texture_handle handle() const;

private:
    std::uint32_t      width_  = 0;
    std::uint32_t      height_ = 0;
    IDirect3DTexture9* tex_    = nullptr;
};

} // namespace Renderer
