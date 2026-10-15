#pragma once
// =============================================================================
// draw_list --- per-frame CPU-side recording of all draw work.
//
// One vertex_buffer + one index_buffer + a list of draw_cmd. Each cmd
// is a contiguous slice of indices that share the same texture and
// scissor rectangle. The backend uploads both buffers once at
// end_frame() and walks the cmd list issuing IASetVertexBuffers /
// RSSetScissorRects / PSSetShaderResources / DrawIndexed.
//
// This shape mirrors what dec_PerFrame.c is doing: the per-frame
// function accumulates state and then hands it to the renderer via the
// engine vtable[+48] "end_frame" slot.
// =============================================================================

#include <cstdint>
#include <vector>

#include <Renderer/vertex.hpp>
#include <Renderer/geometry.hpp>

namespace Renderer {

using index_t = std::uint16_t;

// Opaque handle for the active SRV. nullptr == bind the default 1x1
// white texture (untextured solid color path).
struct texture_handle { void* srv = nullptr; };

struct draw_cmd {
    texture_handle texture {};
    rect           scissor {};         // empty == no clip
    std::uint32_t  index_offset = 0;
    std::uint32_t  index_count  = 0;
};

class draw_list {
public:
    void clear() {
        vertices_.clear();
        indices_.clear();
        commands_.clear();
        current_ = nullptr;
    }

    // Vertex / index buffer accessors --- used by both primitives and the backend.
    const std::vector<vertex>&   vertices() const { return vertices_; }
    const std::vector<index_t>&  indices () const { return indices_;  }
    const std::vector<draw_cmd>& commands() const { return commands_; }

    std::vector<vertex>&   vertices() { return vertices_; }
    std::vector<index_t>&  indices () { return indices_;  }
    std::vector<draw_cmd>& commands() { return commands_; }

    // Begin / extend a draw_cmd. If the active cmd already has the same
    // texture + scissor we keep extending it (avoids a state change).
    draw_cmd& open_cmd(texture_handle tex, rect scissor) {
        if (current_
            && current_->texture.srv == tex.srv
            && current_->scissor.x == scissor.x && current_->scissor.y == scissor.y
            && current_->scissor.w == scissor.w && current_->scissor.h == scissor.h) {
            return *current_;
        }
        commands_.push_back(draw_cmd{
            tex, scissor,
            /*index_offset=*/static_cast<std::uint32_t>(indices_.size()),
            /*index_count =*/0});
        current_ = &commands_.back();
        return *current_;
    }

    // Reserve room in the index/vertex streams. Returns the base
    // vertex index callers add their offsets to.
    std::uint32_t prim_reserve(std::uint32_t n_verts, std::uint32_t n_indices,
                               texture_handle tex, rect scissor) {
        const auto base = static_cast<std::uint32_t>(vertices_.size());
        vertices_.resize(base + n_verts);
        auto& cmd = open_cmd(tex, scissor);
        const auto ibase = indices_.size();
        indices_.resize(ibase + n_indices);
        cmd.index_count += n_indices;
        (void)ibase;
        return base;
    }

private:
    std::vector<vertex>   vertices_;
    std::vector<index_t>  indices_;
    std::vector<draw_cmd> commands_;
    draw_cmd*             current_ = nullptr; // points into commands_ (invalidated by clear)
};

} // namespace Renderer
