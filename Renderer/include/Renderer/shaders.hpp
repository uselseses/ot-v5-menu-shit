#pragma once
// =============================================================================
// HLSL source strings used by the runtime D3DCompile calls.
//
// Reproduced verbatim from dec_ShaderInit.c:
//   - vertex source  : lines 41..68   (vs_4_0)
//   - pixel  source  : (matching ps_4_0 string, see strings.json)
//
// The original passes flags = 0 and "main" as the entry point. The
// trailing backslash-newlines in the strings are an artifact of how the
// author embedded them as raw HLSL via macros --- they are valid HLSL
// continuations and we keep them so the byte content matches what the
// binary feeds to fxc.
// =============================================================================

namespace Renderer::shaders {

inline constexpr const char* k_vertex_source = R"HLSL(
    cbuffer vertexBuffer : register(b0)                                         \
    {                                                                           \
        float4x4 ProjectionMatrix;                                              \
    };                                                                          \
                                                                                \
    struct VS_INPUT                                                             \
    {                                                                           \
        float2 pos : POSITION;                                                  \
        float4 col : COLOR0;                                                    \
        float2 uv  : TEXCOORD0;                                                 \
    };                                                                          \
                                                                                \
    struct PS_INPUT                                                             \
    {                                                                           \
        float4 pos : SV_POSITION;                                               \
        float4 col : COLOR0;                                                    \
        float2 uv  : TEXCOORD0;                                                 \
    };                                                                          \
                                                                                \
    PS_INPUT main(VS_INPUT input)                                               \
    {                                                                           \
        PS_INPUT output;                                                        \
        output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.0F, 1.0F));   \
        output.col = input.col;                                                 \
        output.uv  = input.uv;                                                  \
        return output;                                                          \
    })HLSL";

inline constexpr const char* k_pixel_source = R"HLSL(
    struct PS_INPUT                                                         \
    {                                                                       \
        float4 pos : SV_POSITION;                                           \
        float4 col : COLOR0;                                                \
        float2 uv  : TEXCOORD0;                                             \
    };                                                                      \
                                                                            \
    sampler   sampler0;                                                     \
    Texture2D texture0;                                                     \
                                                                            \
    float4 main(PS_INPUT input) : SV_Target                                 \
    {                                                                       \
        float4 out_col = input.col * texture0.Sample(sampler0, input.uv);   \
        return out_col;                                                     \
    })HLSL";

inline constexpr const char* k_vs_target = "vs_4_0";
inline constexpr const char* k_ps_target = "ps_4_0";

} // namespace Renderer::shaders
