// =============================================================================
// demo/d3d_setup.cpp --- exact reproduction of the swap-chain descriptor
// the original binary builds in WinMain (sub_140007DE0, lines 19..45).
//
// Original constants (verified by grep against dec_WinMain.c):
//   BufferDesc.Width            = 0   (use window client size)
//   BufferDesc.Height           = 0
//   BufferDesc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM
//   BufferDesc.RefreshRate      = { 60, 1 }
//   BufferDesc.ScanlineOrdering = 0   (DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED)
//   BufferDesc.Scaling          = 0   (DXGI_MODE_SCALING_UNSPECIFIED)
//   SampleDesc                  = { 1, 0 }     (no MSAA)
//   BufferUsage                 = 32  (DXGI_USAGE_RENDER_TARGET_OUTPUT)
//   BufferCount                 = 2
//   OutputWindow                = hWnd
//   Windowed                    = TRUE
//   Flags                       = 2   (DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)
//
// D3D11CreateDeviceAndSwapChain():
//   driver type = HARDWARE
//   flags       = 0
//   levels      = { 11_0, 10_0 }
//   SDKVersion  = 7  (D3D11_SDK_VERSION)
// =============================================================================

#include "d3d_setup.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace demo {

namespace {
template <class T>
inline void safe_release(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}
} // anon

bool init_d3d11(HWND hWnd, d3d_handles& out)
{
    DXGI_SWAP_CHAIN_DESC desc                  = {};
    desc.BufferDesc.Width                      = 0;
    desc.BufferDesc.Height                     = 0;
    desc.BufferDesc.Format                     = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator      = 60;
    desc.BufferDesc.RefreshRate.Denominator    = 1;
    desc.BufferDesc.ScanlineOrdering           = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    desc.BufferDesc.Scaling                    = DXGI_MODE_SCALING_UNSPECIFIED;
    desc.SampleDesc.Count                      = 1;
    desc.SampleDesc.Quality                    = 0;
    desc.BufferUsage                           = DXGI_USAGE_RENDER_TARGET_OUTPUT;   // 32
    desc.BufferCount                           = 2;
    desc.OutputWindow                          = hWnd;
    desc.Windowed                              = TRUE;
    desc.SwapEffect                            = DXGI_SWAP_EFFECT_DISCARD;
    desc.Flags                                 = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // 2

    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                       // pAdapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,                       // Software
        0,                             // Flags --- the original passes 0
        levels,
        static_cast<UINT>(_countof(levels)),
        D3D11_SDK_VERSION,             // 7
        &desc,
        &out.swap_chain,
        &out.device,
        &out.feature_level,
        &out.context);

    if (FAILED(hr) || !out.swap_chain || !out.device || !out.context)
        return false;

    // Backbuffer RTV --- not done by the original here (the engine
    // creates it later) but we need it now so the Phase 1 frame loop
    // can clear something.
    ID3D11Texture2D* backbuffer = nullptr;
    hr = out.swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                   reinterpret_cast<void**>(&backbuffer));
    if (FAILED(hr) || !backbuffer)
        return false;

    hr = out.device->CreateRenderTargetView(backbuffer, nullptr, &out.rtv);
    backbuffer->Release();
    return SUCCEEDED(hr);
}

bool resize_swapchain(d3d_handles& h, UINT width, UINT height)
{
    if (!h.swap_chain || !h.device || !h.context || width == 0 || height == 0)
        return false;

    // The backbuffer RTV must be unbound and released before
    // ResizeBuffers or DXGI fails with outstanding references.
    h.context->OMSetRenderTargets(0, nullptr, nullptr);
    safe_release(h.rtv);

    // 0 buffer count / DXGI_FORMAT_UNKNOWN keep the existing values
    // (2 buffers, B8G8R8A8); flags must match the creation flags.
    HRESULT hr = h.swap_chain->ResizeBuffers(
        0, width, height, DXGI_FORMAT_UNKNOWN,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    if (FAILED(hr))
        return false;

    ID3D11Texture2D* backbuffer = nullptr;
    hr = h.swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                 reinterpret_cast<void**>(&backbuffer));
    if (FAILED(hr) || !backbuffer)
        return false;

    hr = h.device->CreateRenderTargetView(backbuffer, nullptr, &h.rtv);
    backbuffer->Release();
    return SUCCEEDED(hr);
}

void shutdown_d3d11(d3d_handles& h)
{
    safe_release(h.rtv);
    safe_release(h.context);
    safe_release(h.device);
    safe_release(h.swap_chain);
}

} // namespace demo
