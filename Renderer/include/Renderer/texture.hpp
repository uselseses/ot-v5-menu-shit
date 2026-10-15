#pragma once
// =============================================================================
// Renderer::c_texture --- GPU texture wrapper.
//
// Reversed from sub_140092A50 which generates all textures at runtime:
//   - Logo (320x200) at qword_140245F20
//   - Chevron (84x48) at qword_140245F90
//   - Accent gradient (320x200) at qword_140245FA0
//   - User icon (48x48) at qword_140245FA8
//   - Colorpicker textures (SV picker, hue bar, alpha bar, checkerboard)
//   - Checkbox pattern
//
// Textures are created via engine vtable+224 (create_texture_from_pixels).
// =============================================================================

#include <cstdint>
#include <filesystem>
#include <vector>

#include <Renderer/draw_list.hpp>

#ifdef _WIN32
struct ID3D11Device;
struct ID3D11ShaderResourceView;
#endif

namespace Renderer {

class c_texture {
public:
    c_texture() = default;
    ~c_texture();

    c_texture(const c_texture&) = delete;
    c_texture& operator=(const c_texture&) = delete;
    c_texture(c_texture&& other) noexcept;
    c_texture& operator=(c_texture&& other) noexcept;

    // Create from RGBA pixel data.
    bool create(ID3D11Device* device, std::uint32_t width, std::uint32_t height,
                const std::uint8_t* rgba_data);

    bool load_raw_rgba(ID3D11Device* device, const std::filesystem::path& path,
                       std::uint32_t width, std::uint32_t height);

    // Create from packed 0xAARRGGBB data.
    bool create_from_argb(ID3D11Device* device, std::uint32_t width, std::uint32_t height,
                          const std::uint32_t* argb_data);

    std::uint32_t width() const { return width_; }
    std::uint32_t height() const { return height_; }
    bool valid() const;

#ifdef _WIN32
    ID3D11ShaderResourceView* srv() const { return srv_; }
#endif
    texture_handle handle() const { return texture_handle{srv_}; }

private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;

#ifdef _WIN32
    ID3D11ShaderResourceView* srv_ = nullptr;
#endif
};

// =============================================================================
// Texture generation functions (matching binary's runtime generation)
// =============================================================================

// Generate the logo texture (grayscale, colored via tint)
std::vector<std::uint32_t> generate_logo_pixels(std::uint32_t w, std::uint32_t h);

// Generate chevron (collapse/expand arrow)
std::vector<std::uint32_t> generate_chevron_pixels(std::uint32_t w, std::uint32_t h);

// Generate accent gradient overlay
std::vector<std::uint32_t> generate_accent_gradient_pixels(std::uint32_t w, std::uint32_t h);

// Generate user icon placeholder
std::vector<std::uint32_t> generate_user_icon_pixels(std::uint32_t w, std::uint32_t h);

// Generate checkerboard pattern for transparency preview
std::vector<std::uint32_t> generate_checkerboard_pixels(std::uint32_t w, std::uint32_t h);

// Generate solid color texture
std::vector<std::uint32_t> generate_solid_pixels(std::uint32_t w, std::uint32_t h,
                                                  std::uint32_t color);

// Generate vertical gradient (for hue/alpha bars)
std::vector<std::uint32_t> generate_vgradient_pixels(std::uint32_t w, std::uint32_t h,
                                                      std::uint32_t top_color,
                                                      std::uint32_t bottom_color);

} // namespace Renderer
