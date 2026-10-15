#pragma once
// =============================================================================
// Renderer::c_d3d9_renderer
//
// D3D9 backend, fixed-function pipeline. Intended use case: the host
// process owns the IDirect3DDevice9 (a game) and the host's Present /
// Reset hook trampolines call into this renderer.
//
// Vertex format (FVF): D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1
//   - pre-transformed screen-space x, y, z, rhw
//   - DWORD diffuse colour (D3DCOLOR = 0xAARRGGBB in memory layout)
//   - one texcoord (u, v)
//   sizeof(d3d9_vertex) == 28
//
// Pipeline state (fixed-function only):
//   - Z disabled, lighting off, no culling
//   - Alpha blend ON, src=SRC_ALPHA, dst=INV_SRC_ALPHA
//   - Scissor test enabled per command
//   - Stage 0 colour: MODULATE(texture, diffuse)
//   - Stage 0 alpha : MODULATE(texture, diffuse)
//   - Sampler 0: LINEAR min/mag, CLAMP u/v
//
// Lifecycle (caller perspective — most hosts do this from hook trampolines):
//
//   // In your DLL on first Present (or after device creation):
//   renderer.attach(pDevice);
//   renderer.initialize();
//
//   // In your Present hook AFTER your EndScene drawing:
//   renderer.begin_frame();
//   // ... widgets push primitives via c_renderer_core ...
//   renderer.end_frame();
//
//   // In your Reset hook:
//   renderer.on_device_lost();
//   HRESULT hr = pDevice->Reset(&pp);
//   if (SUCCEEDED(hr)) renderer.on_device_reset();
//
// Pool choices:
//   - White texture + glyph atlases + user textures: D3DPOOL_MANAGED
//     (survives Reset automatically, no on_lost/on_reset action needed).
//   - Vertex buffer + index buffer: D3DPOOL_DEFAULT with USAGE_DYNAMIC
//     (must be released on lost / recreated on reset — handled here).
// =============================================================================

#include <Renderer/c_abstract_renderer.hpp>
#include <Renderer/draw_list.hpp>

#include <cstdint>

struct IDirect3DDevice9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
struct IDirect3DTexture9;
struct IDirect3DStateBlock9;

namespace Renderer {

class c_d3d9_renderer final : public c_abstract_renderer {
public:
    c_d3d9_renderer() = default;
    ~c_d3d9_renderer() override;

    // Hand the renderer the device handle. Does NOT take ownership —
    // the host process still releases it. Optionally pass an explicit
    // viewport size; if width/height are 0 the size is read from the
    // current viewport at end_frame() time.
    void attach(IDirect3DDevice9* device,
                std::uint32_t width  = 0,
                std::uint32_t height = 0);

    // Create the D3D9 resources (VB, IB, states, white tex). Safe to
    // call after attach(). Returns false on device failure.
    bool initialize() override;
    void shutdown()   override;

    void on_resize(std::uint32_t width, std::uint32_t height) override;

    // Call from your Reset hook BEFORE invoking the device's Reset.
    // Releases DEFAULT-pool resources. Safe to call multiple times.
    void on_device_lost();

    // Call from your Reset hook AFTER the device's Reset succeeds.
    // Recreates the DEFAULT-pool resources. Returns false on failure.
    bool on_device_reset();

    // Per-frame entry points. Call between BeginScene/EndScene (in
    // practice: from the Present hook, after the host has issued its
    // own draw work and before chaining to the original Present).
    void begin_frame() override;
    void end_frame()   override;

    draw_list&       draws()       override { return dl_;         }
    const draw_list& draws() const override { return dl_;         }
    draw_list&       overlay_draws()       { return overlay_dl_;  }
    const draw_list& overlay_draws() const { return overlay_dl_;  }

    std::uint32_t width () const noexcept override { return width_;  }
    std::uint32_t height() const noexcept override { return height_; }

    IDirect3DDevice9* device() const noexcept { return device_; }

private:
    bool create_default_pool_resources();
    void release_default_pool_resources();
    bool create_managed_pool_resources();
    void release_managed_pool_resources();
    void grow_buffers(std::uint32_t v_capacity, std::uint32_t i_capacity);

    IDirect3DDevice9*       device_   = nullptr;        // borrowed
    std::uint32_t           width_    = 0;
    std::uint32_t           height_   = 0;

    // D3DPOOL_DEFAULT — must be released before host Reset.
    IDirect3DVertexBuffer9* vb_       = nullptr;
    IDirect3DIndexBuffer9*  ib_       = nullptr;
    std::uint32_t           vb_capacity_ = 0;
    std::uint32_t           ib_capacity_ = 0;

    // D3DPOOL_MANAGED — survives Reset.
    IDirect3DTexture9*      white_tex_ = nullptr;

    // State block lets us snapshot / restore the host's renderstate so
    // we don't corrupt their draws (cheat-overlay courtesy).
    IDirect3DStateBlock9*   saved_state_ = nullptr;

    bool initialized_ = false;

    draw_list dl_{};
    draw_list overlay_dl_{};
};

} // namespace Renderer
