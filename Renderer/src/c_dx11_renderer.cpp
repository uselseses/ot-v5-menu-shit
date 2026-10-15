// =============================================================================
// c_dx11_renderer.cpp --- DX11 backend.
//
// Pipeline state mirrors what the original sets up (recovered from
// dec_ShaderInit.c + dec_PerFrame.c):
//   - input layout: POSITION(R32G32_FLOAT) / TEXCOORD(R32G32_FLOAT) / COLOR(R8G8B8A8_UNORM)
//   - constant buffer b0: float4x4 ProjectionMatrix
//   - blend  : premultiplied alpha
//   - raster : no cull, scissor enabled
//   - depth  : disabled
//   - sampler: linear, clamp
// =============================================================================

#include <Renderer/c_dx11_renderer.hpp>

#include <Renderer/shaders.hpp>
#include <Renderer/vertex.hpp>

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

namespace Renderer {

namespace {
template <class T>
inline void safe_release(T*& p) { if (p) { p->Release(); p = nullptr; } }
} // anon

c_dx11_renderer::~c_dx11_renderer() { shutdown(); }

void c_dx11_renderer::attach(ID3D11Device* device, ID3D11DeviceContext* context,
                             IDXGISwapChain* swap, ID3D11RenderTargetView* rtv,
                             std::uint32_t w, std::uint32_t h)
{
    device_  = device;
    context_ = context;
    swap_    = swap;
    rtv_     = rtv;
    width_   = w;
    height_  = h;
}

bool c_dx11_renderer::initialize()
{
    if (!device_ || !context_) return false;
    if (!compile_shaders())     return false;
    if (!create_states())       return false;
    if (!create_white_texture())return false;
    // Start with a modest buffer; grow on demand.
    grow_buffers(/*verts=*/4096, /*idx=*/8192);
    return true;
}

void c_dx11_renderer::shutdown()
{
    safe_release(white_srv_);
    safe_release(white_tex_);
    safe_release(depth_);
    safe_release(rasterizer_);
    safe_release(blend_);
    safe_release(sampler_);
    safe_release(cb_proj_);
    safe_release(ib_);
    safe_release(vb_);
    safe_release(ps_);
    safe_release(vs_);
    safe_release(input_layout_);
    // device / context / swap / rtv are borrowed --- leave alone.
}

void c_dx11_renderer::on_resize(std::uint32_t w, std::uint32_t h)
{
    width_  = w;
    height_ = h;
    // The harness reallocates the swapchain backbuffer + RTV and calls
    // attach() again. We just record the new viewport.
}

bool c_dx11_renderer::compile_shaders()
{
    ID3DBlob* vsb = nullptr; ID3DBlob* err = nullptr;
    HRESULT hr = D3DCompile(shaders::k_vertex_source,
                            std::strlen(shaders::k_vertex_source),
                            nullptr, nullptr, nullptr,
                            "main", shaders::k_vs_target,
                            0, 0, &vsb, &err);
    if (FAILED(hr)) { safe_release(vsb); safe_release(err); return false; }
    safe_release(err);

    hr = device_->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(),
                                     nullptr, &vs_);
    if (FAILED(hr)) { safe_release(vsb); return false; }

    // Input layout --- offsets match Renderer/vertex.hpp.
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(vertex, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(vertex, uv ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, offsetof(vertex, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = device_->CreateInputLayout(layout, _countof(layout),
                                    vsb->GetBufferPointer(), vsb->GetBufferSize(),
                                    &input_layout_);
    safe_release(vsb);
    if (FAILED(hr)) return false;

    ID3DBlob* psb = nullptr;
    hr = D3DCompile(shaders::k_pixel_source,
                    std::strlen(shaders::k_pixel_source),
                    nullptr, nullptr, nullptr,
                    "main", shaders::k_ps_target,
                    0, 0, &psb, &err);
    if (FAILED(hr)) { safe_release(psb); safe_release(err); return false; }
    safe_release(err);

    hr = device_->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(),
                                    nullptr, &ps_);
    safe_release(psb);
    if (FAILED(hr)) return false;

    // Projection constant buffer.
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth      = 16 * sizeof(float);   // float4x4
    cbd.Usage          = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device_->CreateBuffer(&cbd, nullptr, &cb_proj_))) return false;

    return true;
}

bool c_dx11_renderer::create_states()
{
    // Sampler: linear filter, clamp on both axes.
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD   = D3D11_FLOAT32_MAX;
    if (FAILED(device_->CreateSamplerState(&sd, &sampler_))) return false;

    // Blend: premultiplied-alpha-friendly. SrcAlpha / InvSrcAlpha and
    // straight alpha on the alpha channel.
    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable           = TRUE;
    bd.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device_->CreateBlendState(&bd, &blend_))) return false;

    // Rasterizer: scissor on, no cull, solid fill.
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode        = D3D11_FILL_SOLID;
    rd.CullMode        = D3D11_CULL_NONE;
    rd.ScissorEnable   = TRUE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(device_->CreateRasterizerState(&rd, &rasterizer_))) return false;

    // Depth/stencil: completely off.
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable   = FALSE;
    dsd.StencilEnable = FALSE;
    if (FAILED(device_->CreateDepthStencilState(&dsd, &depth_))) return false;

    return true;
}

bool c_dx11_renderer::create_white_texture()
{
    D3D11_TEXTURE2D_DESC td = {};
    td.Width  = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage     = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    const std::uint32_t pixel = 0xFFFFFFFFu;
    D3D11_SUBRESOURCE_DATA init = { &pixel, 4, 0 };
    if (FAILED(device_->CreateTexture2D(&td, &init, &white_tex_))) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
    svd.Format              = td.Format;
    svd.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    svd.Texture2D.MipLevels = 1;
    if (FAILED(device_->CreateShaderResourceView(white_tex_, &svd, &white_srv_))) return false;
    return true;
}

void c_dx11_renderer::grow_buffers(std::uint32_t v_cap, std::uint32_t i_cap)
{
    if (v_cap <= vb_capacity_ && i_cap <= ib_capacity_) return;

    safe_release(vb_);
    safe_release(ib_);

    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth      = v_cap * sizeof(vertex);
    vbd.Usage          = D3D11_USAGE_DYNAMIC;
    vbd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device_->CreateBuffer(&vbd, nullptr, &vb_);

    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth      = i_cap * sizeof(index_t);
    ibd.Usage          = D3D11_USAGE_DYNAMIC;
    ibd.BindFlags      = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device_->CreateBuffer(&ibd, nullptr, &ib_);

    vb_capacity_ = v_cap;
    ib_capacity_ = i_cap;
}

void c_dx11_renderer::begin_frame()
{
    dl_        .clear();
    overlay_dl_.clear();

    // Clear backbuffer. Eventually the framework's body element does this.
    if (rtv_) {
        const float clear[4] = { 0.094f, 0.106f, 0.129f, 1.0f };
        context_->OMSetRenderTargets(1, &rtv_, nullptr);
        context_->ClearRenderTargetView(rtv_, clear);
    }
}

namespace {
// Issue the commands from `dl` with both index_offset and BaseVertex
// shifted by the supplied offsets --- lets multiple layers share one
// vertex + index buffer that's already been concatenated.
void issue_cmds(const draw_list& dl,
                ID3D11DeviceContext* ctx,
                ID3D11ShaderResourceView* white_srv,
                std::uint32_t width, std::uint32_t height,
                std::uint32_t index_off,
                std::int32_t  base_vert)
{
    for (const auto& c : dl.commands()) {
        D3D11_RECT scissor;
        if (c.scissor.empty()) {
            scissor = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        } else {
            scissor.left   = static_cast<LONG>(c.scissor.x);
            scissor.top    = static_cast<LONG>(c.scissor.y);
            scissor.right  = static_cast<LONG>(c.scissor.x + c.scissor.w);
            scissor.bottom = static_cast<LONG>(c.scissor.y + c.scissor.h);
        }
        ctx->RSSetScissorRects(1, &scissor);

        ID3D11ShaderResourceView* srv = c.texture.srv
            ? static_cast<ID3D11ShaderResourceView*>(c.texture.srv)
            : white_srv;
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->DrawIndexed(c.index_count, c.index_offset + index_off, base_vert);
    }
}
} // anon

void c_dx11_renderer::end_frame()
{
    if (!device_ || !context_ || !swap_) return;

    // Total geometry across BOTH queues drives the upload buffer size.
    const std::size_t total_verts = dl_.vertices().size() + overlay_dl_.vertices().size();
    const std::size_t total_idxs  = dl_.indices ().size() + overlay_dl_.indices ().size();
    // Vsync OFF (sync interval 0) so the host's software fps limiter
    // governs the frame rate (user 2026-06-10: the fps cap couldn't
    // reach 300 because vsync pinned it to the refresh rate).
    if (total_verts == 0) { swap_->Present(0, 0); return; }
    grow_buffers(
        total_verts > vb_capacity_ ? static_cast<std::uint32_t>(total_verts * 2) : vb_capacity_,
        total_idxs  > ib_capacity_ ? static_cast<std::uint32_t>(total_idxs  * 2) : ib_capacity_);

    // Projection: orthographic, top-left origin, pixel coordinates.
    {
        const float L = 0.0f, R = static_cast<float>(width_);
        const float T = 0.0f, B = static_cast<float>(height_);
        const float proj[16] = {
            2.0f / (R - L),     0.0f,               0.0f, 0.0f,
            0.0f,               2.0f / (T - B),     0.0f, 0.0f,
            0.0f,               0.0f,               0.5f, 0.0f,
            (R + L) / (L - R),  (T + B) / (B - T),  0.5f, 1.0f,
        };
        D3D11_MAPPED_SUBRESOURCE map{};
        if (SUCCEEDED(context_->Map(cb_proj_, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
            std::memcpy(map.pData, proj, sizeof(proj));
            context_->Unmap(cb_proj_, 0);
        }
    }

    // Upload both layers' geometry concatenated into the shared VB/IB.
    // Layer offsets (overlay starts past the normal layer's tail) are
    // applied via DrawIndexed's BaseVertex + index_offset parameters in
    // issue_cmds() below.
    const std::uint32_t n_verts_normal  = static_cast<std::uint32_t>(dl_.vertices().size());
    const std::uint32_t n_verts_overlay = static_cast<std::uint32_t>(overlay_dl_.vertices().size());
    const std::uint32_t n_idxs_normal   = static_cast<std::uint32_t>(dl_.indices ().size());
    const std::uint32_t n_idxs_overlay  = static_cast<std::uint32_t>(overlay_dl_.indices ().size());

    {
        D3D11_MAPPED_SUBRESOURCE map{};
        if (SUCCEEDED(context_->Map(vb_, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
            std::memcpy(map.pData, dl_.vertices().data(),
                        n_verts_normal * sizeof(vertex));
            std::memcpy(static_cast<vertex*>(map.pData) + n_verts_normal,
                        overlay_dl_.vertices().data(),
                        n_verts_overlay * sizeof(vertex));
            context_->Unmap(vb_, 0);
        }
        if (SUCCEEDED(context_->Map(ib_, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
            std::memcpy(map.pData, dl_.indices().data(),
                        n_idxs_normal * sizeof(index_t));
            std::memcpy(static_cast<index_t*>(map.pData) + n_idxs_normal,
                        overlay_dl_.indices().data(),
                        n_idxs_overlay * sizeof(index_t));
            context_->Unmap(ib_, 0);
        }
    }

    // Pipeline state (set once for both passes).
    UINT stride = sizeof(vertex), offset = 0;
    context_->IASetInputLayout(input_layout_);
    context_->IASetVertexBuffers(0, 1, &vb_, &stride, &offset);
    context_->IASetIndexBuffer(ib_, DXGI_FORMAT_R16_UINT, 0);
    context_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vs_, nullptr, 0);
    context_->VSSetConstantBuffers(0, 1, &cb_proj_);
    context_->PSSetShader(ps_, nullptr, 0);
    context_->PSSetSamplers(0, 1, &sampler_);
    const float blend_factor[4] = {0,0,0,0};
    context_->OMSetBlendState(blend_, blend_factor, 0xFFFFFFFFu);
    context_->OMSetDepthStencilState(depth_, 0);
    context_->RSSetState(rasterizer_);

    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(width_);
    vp.Height   = static_cast<float>(height_);
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    // Pass 1: normal layer at the start of both buffers.
    issue_cmds(dl_,         context_, white_srv_, width_, height_,
               /*index_off=*/0, /*base_vert=*/0);
    // Pass 2: overlay layer offset past the normal tails.
    issue_cmds(overlay_dl_, context_, white_srv_, width_, height_,
               /*index_off=*/n_idxs_normal,
               /*base_vert=*/static_cast<std::int32_t>(n_verts_normal));

    swap_->Present(0, 0);   // vsync off — software fps limiter governs
}

} // namespace Renderer
