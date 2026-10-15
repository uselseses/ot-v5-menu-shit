#pragma once
// demo::d3d_setup --- DX11 device + swap chain bring-up.
// 1:1 with the D3D11CreateDeviceAndSwapChain call inside WinMain
// (see analysis/dec_WinMain.c lines 19..45).

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

namespace demo {

struct d3d_handles {
    IDXGISwapChain*       swap_chain    = nullptr;
    ID3D11Device*         device        = nullptr;
    ID3D11DeviceContext*  context       = nullptr;
    ID3D11RenderTargetView* rtv         = nullptr;   // backbuffer view
    D3D_FEATURE_LEVEL     feature_level = D3D_FEATURE_LEVEL_11_0;
};

// Creates device, swap chain and the backbuffer RTV. Returns false on
// any failure; on success caller owns the COM refs in `out`.
bool init_d3d11(HWND hWnd, d3d_handles& out);

// Releases everything in `h` (idempotent --- nulls each pointer).
void shutdown_d3d11(d3d_handles& h);

// Resizes the swapchain backbuffers to the new client size and
// recreates the backbuffer RTV (the old one must not be bound).
// Returns false if ResizeBuffers or RTV creation fails.
bool resize_swapchain(d3d_handles& h, UINT width, UINT height);

} // namespace demo
