// =============================================================================
// c_d3d9_renderer.cpp — D3D9 fixed-function backend.
//
// Pipeline mirrors the DX11 backend's draw_list shape so c_renderer_core
// emits the same primitives unchanged. Vertex layout differs:
//   DX11 vertex (Renderer::vertex):     pos[2], uv[2], col   (20 bytes)
//   D3D9 vertex (D3DFVF_XYZRHW|...) :   x,y,z,rhw,col,u,v    (28 bytes)
// We translate on the fly during the VB upload in end_frame().
// =============================================================================

#include <Renderer/c_d3d9_renderer.hpp>
#include <Renderer/vertex.hpp>

#include <Windows.h>
#include <d3d9.h>
#include <cstring>

#pragma comment(lib, "d3d9.lib")

namespace Renderer {

namespace {

constexpr DWORD k_d3d9_fvf =
    D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

// One D3D9 fixed-function vertex.
#pragma pack(push, 1)
struct d3d9_vertex {
    float    x, y, z, rhw;   // XYZRHW — pre-transformed
    D3DCOLOR diffuse;        // 0xAARRGGBB
    float    u, v;
};
#pragma pack(pop)
static_assert(sizeof(d3d9_vertex) == 28, "FVF size mismatch");

template <class T>
inline void safe_release(T*& p) { if (p) { p->Release(); p = nullptr; } }

// Renderer::vertex packs colour as 0xAABBGGRR (R8G8B8A8_UNORM little-
// endian). D3DCOLOR wants 0xAARRGGBB. Swap R and B.
inline D3DCOLOR pack_to_d3dcolor(std::uint32_t rgba)
{
    return ((rgba & 0xFF00FF00u))
         | ((rgba & 0x00FF0000u) >> 16)
         | ((rgba & 0x000000FFu) << 16);
}

} // anon

c_d3d9_renderer::~c_d3d9_renderer() { shutdown(); }

void c_d3d9_renderer::attach(IDirect3DDevice9* device,
                             std::uint32_t w, std::uint32_t h)
{
    device_ = device;
    width_  = w;
    height_ = h;
}

bool c_d3d9_renderer::create_managed_pool_resources()
{
    if (!device_) return false;
    if (white_tex_) return true;

    HRESULT hr = device_->CreateTexture(
        1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
        &white_tex_, nullptr);
    if (FAILED(hr) || !white_tex_) return false;

    D3DLOCKED_RECT lr{};
    if (FAILED(white_tex_->LockRect(0, &lr, nullptr, 0))) return false;
    *reinterpret_cast<std::uint32_t*>(lr.pBits) = 0xFFFFFFFFu;
    white_tex_->UnlockRect(0);
    return true;
}

void c_d3d9_renderer::release_managed_pool_resources()
{
    safe_release(white_tex_);
}

bool c_d3d9_renderer::create_default_pool_resources()
{
    if (!device_) return false;
    grow_buffers(/*verts=*/4096, /*idx=*/8192);
    return vb_ != nullptr && ib_ != nullptr;
}

void c_d3d9_renderer::release_default_pool_resources()
{
    safe_release(vb_);
    safe_release(ib_);
    safe_release(saved_state_);
    vb_capacity_ = ib_capacity_ = 0;
}

bool c_d3d9_renderer::initialize()
{
    if (!device_) return false;
    if (initialized_) return true;
    if (!create_managed_pool_resources()) return false;
    if (!create_default_pool_resources()) {
        release_managed_pool_resources();
        return false;
    }
    initialized_ = true;
    return true;
}

void c_d3d9_renderer::shutdown()
{
    release_default_pool_resources();
    release_managed_pool_resources();
    initialized_ = false;
    // device_ borrowed — leave alone.
}

void c_d3d9_renderer::on_resize(std::uint32_t w, std::uint32_t h)
{
    width_  = w;
    height_ = h;
}

void c_d3d9_renderer::on_device_lost()
{
    // Drop DEFAULT-pool resources only; MANAGED textures survive Reset.
    release_default_pool_resources();
}

bool c_d3d9_renderer::on_device_reset()
{
    return create_default_pool_resources();
}

void c_d3d9_renderer::grow_buffers(std::uint32_t v_cap, std::uint32_t i_cap)
{
    if (!device_) return;
    if (vb_ && v_cap <= vb_capacity_ && ib_ && i_cap <= ib_capacity_) return;

    safe_release(vb_);
    safe_release(ib_);

    HRESULT hr;
    hr = device_->CreateVertexBuffer(
        v_cap * sizeof(d3d9_vertex),
        D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
        k_d3d9_fvf,
        D3DPOOL_DEFAULT,
        &vb_, nullptr);
    if (FAILED(hr)) { vb_ = nullptr; vb_capacity_ = 0; return; }

    hr = device_->CreateIndexBuffer(
        i_cap * sizeof(index_t),
        D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
        D3DFMT_INDEX16,
        D3DPOOL_DEFAULT,
        &ib_, nullptr);
    if (FAILED(hr)) { ib_ = nullptr; ib_capacity_ = 0; return; }

    vb_capacity_ = v_cap;
    ib_capacity_ = i_cap;
}

void c_d3d9_renderer::begin_frame()
{
    dl_        .clear();
    overlay_dl_.clear();

    // If host hasn't supplied dimensions, read the current viewport.
    // Common case when injecting into a game: width/height aren't known
    // until the first frame after BeginScene.
    if (device_ && (width_ == 0 || height_ == 0)) {
        D3DVIEWPORT9 vp{};
        if (SUCCEEDED(device_->GetViewport(&vp))) {
            width_  = vp.Width;
            height_ = vp.Height;
        }
    }
}

namespace {

void issue_cmds(const draw_list& dl,
                IDirect3DDevice9* dev,
                IDirect3DTexture9* white_tex,
                std::uint32_t width, std::uint32_t height,
                std::uint32_t index_off,
                std::int32_t  base_vert)
{
    for (const auto& c : dl.commands()) {
        RECT scissor;
        if (c.scissor.empty()) {
            scissor.left = 0; scissor.top = 0;
            scissor.right = static_cast<LONG>(width);
            scissor.bottom = static_cast<LONG>(height);
        } else {
            scissor.left   = static_cast<LONG>(c.scissor.x);
            scissor.top    = static_cast<LONG>(c.scissor.y);
            scissor.right  = static_cast<LONG>(c.scissor.x + c.scissor.w);
            scissor.bottom = static_cast<LONG>(c.scissor.y + c.scissor.h);
        }
        dev->SetScissorRect(&scissor);

        // c.texture.srv is an IDirect3DTexture9* for this backend.
        IDirect3DTexture9* tex = c.texture.srv
            ? reinterpret_cast<IDirect3DTexture9*>(c.texture.srv)
            : white_tex;
        dev->SetTexture(0, tex);

        // 16-bit indices, two triangles per quad. PrimitiveCount = idx/3.
        dev->DrawIndexedPrimitive(
            D3DPT_TRIANGLELIST,
            base_vert,
            /*MinVertexIndex=*/0,
            /*NumVertices=*/0xFFFF,   // upper bound — we don't track per-cmd
            c.index_offset + index_off,
            c.index_count / 3);
    }
}

} // anon

void c_d3d9_renderer::end_frame()
{
    if (!device_ || !initialized_) return;

    const std::size_t total_verts =
        dl_.vertices().size() + overlay_dl_.vertices().size();
    const std::size_t total_idxs  =
        dl_.indices ().size() + overlay_dl_.indices ().size();
    if (total_verts == 0 || total_idxs == 0) return;

    grow_buffers(
        total_verts > vb_capacity_ ? static_cast<std::uint32_t>(total_verts * 2) : vb_capacity_,
        total_idxs  > ib_capacity_ ? static_cast<std::uint32_t>(total_idxs  * 2) : ib_capacity_);
    if (!vb_ || !ib_) return;

    // ---- Snapshot host state so we don't corrupt their next draw -------
    safe_release(saved_state_);
    device_->CreateStateBlock(D3DSBT_ALL, &saved_state_);

    // ---- Upload VB --------------------------------------------------------
    const std::uint32_t n_verts_normal  = static_cast<std::uint32_t>(dl_.vertices().size());
    const std::uint32_t n_verts_overlay = static_cast<std::uint32_t>(overlay_dl_.vertices().size());
    const std::uint32_t n_idxs_normal   = static_cast<std::uint32_t>(dl_.indices ().size());
    const std::uint32_t n_idxs_overlay  = static_cast<std::uint32_t>(overlay_dl_.indices ().size());

    {
        void* vb_data = nullptr;
        if (SUCCEEDED(vb_->Lock(0, (n_verts_normal + n_verts_overlay) * sizeof(d3d9_vertex),
                                 &vb_data, D3DLOCK_DISCARD)) && vb_data) {
            auto* dst = static_cast<d3d9_vertex*>(vb_data);
            auto translate = [&](const vertex* src, std::uint32_t n) {
                for (std::uint32_t i = 0; i < n; ++i) {
                    // D3D9 needs the half-pixel offset (-0.5) for exact
                    // pixel sampling. Same trick as ImGui.
                    dst->x   = src->pos[0] - 0.5f;
                    dst->y   = src->pos[1] - 0.5f;
                    dst->z   = 0.0f;
                    dst->rhw = 1.0f;
                    dst->diffuse = pack_to_d3dcolor(src->col);
                    dst->u = src->uv[0];
                    dst->v = src->uv[1];
                    ++src; ++dst;
                }
            };
            translate(dl_.vertices().data(), n_verts_normal);
            translate(overlay_dl_.vertices().data(), n_verts_overlay);
            vb_->Unlock();
        }
    }

    {
        void* ib_data = nullptr;
        if (SUCCEEDED(ib_->Lock(0, (n_idxs_normal + n_idxs_overlay) * sizeof(index_t),
                                 &ib_data, D3DLOCK_DISCARD)) && ib_data) {
            std::memcpy(ib_data, dl_.indices().data(),
                        n_idxs_normal * sizeof(index_t));
            std::memcpy(static_cast<index_t*>(ib_data) + n_idxs_normal,
                        overlay_dl_.indices().data(),
                        n_idxs_overlay * sizeof(index_t));
            ib_->Unlock();
        }
    }

    // ---- Pipeline state ---------------------------------------------------
    device_->SetStreamSource(0, vb_, 0, sizeof(d3d9_vertex));
    device_->SetIndices(ib_);
    device_->SetFVF(k_d3d9_fvf);
    device_->SetPixelShader(nullptr);
    device_->SetVertexShader(nullptr);

    device_->SetRenderState(D3DRS_CULLMODE,            D3DCULL_NONE);
    device_->SetRenderState(D3DRS_LIGHTING,            FALSE);
    device_->SetRenderState(D3DRS_ZENABLE,             FALSE);
    device_->SetRenderState(D3DRS_ZWRITEENABLE,        FALSE);
    device_->SetRenderState(D3DRS_ALPHATESTENABLE,     FALSE);
    device_->SetRenderState(D3DRS_ALPHABLENDENABLE,    TRUE);
    device_->SetRenderState(D3DRS_BLENDOP,             D3DBLENDOP_ADD);
    device_->SetRenderState(D3DRS_SRCBLEND,            D3DBLEND_SRCALPHA);
    device_->SetRenderState(D3DRS_DESTBLEND,           D3DBLEND_INVSRCALPHA);
    device_->SetRenderState(D3DRS_SCISSORTESTENABLE,   TRUE);
    device_->SetRenderState(D3DRS_FOGENABLE,           FALSE);
    device_->SetRenderState(D3DRS_STENCILENABLE,       FALSE);
    device_->SetRenderState(D3DRS_CLIPPING,            TRUE);

    // Texture stage 0: MODULATE(texture, diffuse) for both colour + alpha.
    device_->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    device_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device_->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
    device_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device_->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_DISABLE);
    device_->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE);

    device_->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device_->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device_->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    device_->SetSamplerState(0, D3DSAMP_ADDRESSU,  D3DTADDRESS_CLAMP);
    device_->SetSamplerState(0, D3DSAMP_ADDRESSV,  D3DTADDRESS_CLAMP);

    // ---- Issue both layers ------------------------------------------------
    issue_cmds(dl_,         device_, white_tex_, width_, height_,
               /*idx_off=*/0, /*base_vert=*/0);
    issue_cmds(overlay_dl_, device_, white_tex_, width_, height_,
               /*idx_off=*/n_idxs_normal,
               /*base_vert=*/static_cast<std::int32_t>(n_verts_normal));

    // ---- Restore host state ----------------------------------------------
    if (saved_state_) {
        saved_state_->Apply();
        safe_release(saved_state_);
    }
}

} // namespace Renderer
