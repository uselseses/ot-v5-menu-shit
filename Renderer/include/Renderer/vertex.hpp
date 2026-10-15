#pragma once
// =============================================================================
// Vertex layout used by the DX11 renderer.
//
// Reproduced from dec_ShaderInit.c lines 90..101 (input-layout setup
// inside sub_1400A7B90):
//
//   semantic   | format          | offset
//   -----------+-----------------+-------
//   POSITION 0 | R32G32_FLOAT    | 0
//   TEXCOORD 0 | R32G32_FLOAT    | 8
//   COLOR    0 | R8G8B8A8_UNORM  | 16    (fmt id 87)
//
// One sampler + one texture slot. Same packing as Dear ImGui.
// =============================================================================

#include <cstdint>

namespace Renderer {

struct vertex {
    float    pos[2];   // POSITION 0 --- screen-space (pre-projection)
    float    uv [2];   // TEXCOORD 0
    uint32_t col;      // COLOR    0 --- packed R8G8B8A8_UNORM
};

static_assert(sizeof(vertex) == 20, "vertex must match the DX11 input layout exactly");

} // namespace Renderer
