#pragma once
// =============================================================================
// Renderer::c_dx11_renderer
//
// Concrete c_abstract_renderer backend. Borrows the ID3D11Device /
// Context / SwapChain handles created by the demo harness (they're
// created in WinMain in the original --- see dec_WinMain.c).
// =============================================================================

#include <Renderer/c_abstract_renderer.hpp>
#include <Renderer/draw_list.hpp>

#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;
struct ID3D11InputLayout;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11Buffer;
struct ID3D11SamplerState;
struct ID3D11BlendState;
struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;

namespace Renderer {

class c_dx11_renderer final : public c_abstract_renderer {
public:
    c_dx11_renderer() = default;
    ~c_dx11_renderer() override;

    // Hand the renderer the D3D11 handles the harness already created.
    // The renderer does NOT take ownership --- the harness still calls
    // Release() on its copies at shutdown.
    void attach(ID3D11Device*           device,
                ID3D11DeviceContext*    context,
                IDXGISwapChain*         swap,
                ID3D11RenderTargetView* backbuffer_rtv,
                std::uint32_t           width,
                std::uint32_t           height);

    bool initialize() override;
    void shutdown()   override;
    void on_resize(std::uint32_t width, std::uint32_t height) override;
    void begin_frame() override;
    void end_frame()   override;

    draw_list&       draws()       override { return dl_; }
    const draw_list& draws() const override { return dl_; }

    // Overlay queue --- flushed *after* the normal queue every frame so
    // popups (open dropdown/colorpicker/context menu) sit on top
    // regardless of declaration order. Matches the binary's
    // qword_1402465D8 +0x48 work-queue (see dec_PerFrame.c line ~231).
    draw_list&       overlay_draws()       { return overlay_dl_; }
    const draw_list& overlay_draws() const { return overlay_dl_; }
    std::uint32_t    width () const noexcept override { return width_;  }
    std::uint32_t    height() const noexcept override { return height_; }

private:
    bool compile_shaders();
    bool create_states();
    bool create_white_texture();
    void grow_buffers(std::uint32_t v_capacity, std::uint32_t i_capacity);

    // borrowed (not owned)
    ID3D11Device*           device_      = nullptr;
    ID3D11DeviceContext*    context_     = nullptr;
    IDXGISwapChain*         swap_        = nullptr;
    ID3D11RenderTargetView* rtv_         = nullptr;
    std::uint32_t           width_       = 0;
    std::uint32_t           height_      = 0;

    // owned
    ID3D11InputLayout*       input_layout_ = nullptr;
    ID3D11VertexShader*      vs_           = nullptr;
    ID3D11PixelShader*       ps_           = nullptr;
    ID3D11Buffer*            vb_           = nullptr;
    ID3D11Buffer*            ib_           = nullptr;
    ID3D11Buffer*            cb_proj_      = nullptr;
    ID3D11SamplerState*      sampler_      = nullptr;
    ID3D11BlendState*        blend_        = nullptr;
    ID3D11RasterizerState*   rasterizer_   = nullptr;
    ID3D11DepthStencilState* depth_        = nullptr;
    ID3D11Texture2D*         white_tex_    = nullptr;
    ID3D11ShaderResourceView*white_srv_    = nullptr;

    std::uint32_t            vb_capacity_  = 0;
    std::uint32_t            ib_capacity_  = 0;

    draw_list                dl_{};
    draw_list                overlay_dl_{};
};

} // namespace Renderer
