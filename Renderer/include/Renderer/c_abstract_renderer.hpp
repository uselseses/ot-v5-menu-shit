#pragma once
// =============================================================================
// Renderer::c_abstract_renderer
//
// Backend-agnostic interface. The only production backend is
// c_dx11_renderer; the abstraction exists because the original binary's
// RTTI exposes it (.?AVc_abstract_renderer@Renderer@@) and the framework
// holds the renderer through this pointer type.
//
// Lifecycle:
//   initialize()       --- create device-side resources
//   shutdown()         --- release them
//   on_resize(w, h)    --- recreate backbuffer-dependent resources
//   begin_frame()      --- clear the recording draw list, set viewport
//   end_frame()        --- upload + flush + present
// =============================================================================

#include <cstdint>

namespace Renderer {

class draw_list;

class c_abstract_renderer {
public:
    virtual ~c_abstract_renderer() = default;

    virtual bool initialize() = 0;
    virtual void shutdown()   = 0;

    virtual void on_resize(std::uint32_t width, std::uint32_t height) = 0;

    virtual void begin_frame() = 0;
    virtual void end_frame()   = 0;

    // The recording surface --- shared with c_renderer_core. Primitives
    // emit through here.
    virtual draw_list&       draws()       = 0;
    virtual const draw_list& draws() const = 0;

    // Backbuffer dimensions in pixels (after the last on_resize).
    virtual std::uint32_t width () const noexcept = 0;
    virtual std::uint32_t height() const noexcept = 0;
};

} // namespace Renderer
