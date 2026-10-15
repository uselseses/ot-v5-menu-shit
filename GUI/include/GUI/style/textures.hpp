#pragma once
// =============================================================================
// GUI::style::textures — global texture handles mirroring the binary's
// g_texture_arrow (qword_140245F90, 84x48) etc. Set by the host once
// after texture creation; widgets read them during render.
//
// Binary references:
//   g_texture_arrow @ qword_140245F90 — chevron (groupbox/dropdown), 84x48
//   Render call: vt[+304] render_textured_rect(rect, tex, 0xFFD8D8DE,
//                                              scale = inner_pad/84,
//                                              rotation_deg)
//   Cited at decompiled/c_groupbox__render.c:356-362
//        and decompiled/c_dropdown__render.c:430-436
// =============================================================================

#include <Renderer/draw_list.hpp>  // texture_handle

namespace GUI::style {

// Chevron arrow texture handle. Default-empty (.srv = nullptr) until the
// host calls set_chevron_texture(); widgets check `texture_handle.srv`
// and fall back to the procedural triangle if no texture is installed.
inline Renderer::texture_handle& chevron_texture_storage()
{
    static Renderer::texture_handle h{};
    return h;
}

inline void set_chevron_texture(Renderer::texture_handle h)
{
    chevron_texture_storage() = h;
}

inline Renderer::texture_handle chevron_texture()
{
    return chevron_texture_storage();
}

} // namespace GUI::style
