#pragma once
// =============================================================================
// GUI::gui_engine_interface — abstract bridge to the host renderer/input
//
// Vtable layout recovered from GUI_64_DX11.exe g_engine @ 0x14023A680.
// See HOW_IT_WORKS.md §3 for full slot table with verified signatures.
//
// Every render function in the binary calls through this vtable:
//   void* vt = *(void**)g_engine;
//   auto fn = (fn_ptr)*(void**)((char*)vt + OFFSET);
//   fn(g_engine, ...args);
//
// Reserved slots maintain layout stability — promote them as more renders
// are decoded.
// =============================================================================

#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;

namespace GUI {

struct engine_rect  { float x, y, w, h; };

struct dx11_handles {
    ID3D11Device*        device  = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain*      swap    = nullptr;
};

class gui_engine_interface {
public:
    virtual ~gui_engine_interface() = default;

    // --- slot 0: dtor ---
    // (inherited destructor)

    // --- slot 1 (+0x08): acquire ---
    virtual dx11_handles acquire() = 0;

    // --- slot 2 (+0x10): shutdown / dispatch_overlay_entry ---
    virtual void shutdown() = 0;

    // slots 3-4 unobserved
    virtual void reserved_3() {}
    virtual void reserved_4() {}

    // --- slot 5 (+0x28): begin_frame ---
    virtual void begin_frame() = 0;

    // --- slot 6 (+0x30): end_frame ---
    virtual void end_frame() = 0;

    // --- slot 7 (+0x38): handle_message ---
    virtual std::int32_t handle_message(void* hwnd, std::uint32_t msg,
                                        std::uint64_t wparam, std::int64_t lparam,
                                        int* out_handled) {
        (void)hwnd; (void)msg; (void)wparam; (void)lparam; (void)out_handled;
        return 0;
    }

    // slots 8-11 unobserved
    virtual void reserved_8 () {}
    virtual void reserved_9 () {}
    virtual void reserved_10() {}
    virtual void reserved_11() {}

    // --- slot 12 (+0x60): get_viewport_size ---
    virtual void get_viewport_size(int* out_w, int* out_h) = 0;

    // slots 13-17 unobserved
    virtual void reserved_13() {}
    virtual void reserved_14() {}
    virtual void reserved_15() {}
    virtual void reserved_16() {}
    virtual void reserved_17() {}

    // --- slot 18 (+0x90): push_id ---
    virtual void push_id(const char* name) { (void)name; }

    virtual void reserved_19() {}

    // --- slot 20 (+0xA0): push_layout ---
    virtual void push_layout(engine_rect const* r, std::uint32_t flag) { (void)r; (void)flag; }

    // --- slot 21 (+0xA8): pop_layout ---
    virtual void pop_layout(const char* name, std::uint32_t flag) { (void)name; (void)flag; }

    // slots 22-29 unobserved
    virtual void reserved_22() {}
    virtual void reserved_23() {}
    virtual void reserved_24() {}
    virtual void reserved_25() {}
    virtual void reserved_26() {}
    virtual void reserved_27() {}
    virtual void reserved_28() {}
    virtual void reserved_29() {}

    // --- slot 30 (+0xF0): render_filled_rect_simple ---
    virtual void render_filled_rect_simple(engine_rect const* r, std::uint32_t unknown,
                                           std::uint32_t color) {
        (void)r; (void)unknown; (void)color;
    }

    // --- slot 31 (+0xF8): render_filled_rect_1px ---
    // Used for 1-pixel separator/border lines
    virtual void render_filled_rect_1px(engine_rect const* r, std::uint32_t color) {
        (void)r; (void)color;
    }

    virtual void reserved_32() {}

    // --- slot 33 (+0x108): push_clip ---
    virtual void push_clip(engine_rect const* r, std::uint32_t flag) { (void)r; (void)flag; }

    virtual void reserved_34() {}
    virtual void reserved_35() {}

    // --- slot 36 (+0x120): render_filled_rect ---
    virtual void render_filled_rect(engine_rect const* r, std::uint32_t unknown,
                                    std::uint32_t color, float rounding) {
        (void)r; (void)unknown; (void)color; (void)rounding;
    }

    // --- slot 37 (+0x128): render_rect_outline ---
    virtual void render_rect_outline(engine_rect const* r) { (void)r; }

    // --- slot 38 (+0x130): render_gradient_rect / render_textured_rect ---
    virtual void render_gradient_rect(engine_rect const* r, std::uint32_t color,
                                      std::uint32_t flag, float f1, float f2) {
        (void)r; (void)color; (void)flag; (void)f1; (void)f2;
    }

    // --- slot 39 (+0x138): render_rounded_gradient / render_textured_rect_scaled ---
    virtual void render_textured_rect_scaled(engine_rect const* pos, void* texture,
                                             std::uint32_t color, float factor,
                                             float radius) {
        (void)pos; (void)texture; (void)color; (void)factor; (void)radius;
    }

    // slots 40-42 unobserved
    virtual void reserved_40() {}
    virtual void reserved_41() {}
    virtual void reserved_42() {}

    // --- slot 43 (+0x158): render_texture ---
    virtual void render_texture(engine_rect const* pos, void* texture,
                                std::uint32_t color) {
        (void)pos; (void)texture; (void)color;
    }

    // slots 44-47 unobserved
    virtual void reserved_44() {}
    virtual void reserved_45() {}
    virtual void reserved_46() {}
    virtual void reserved_47() {}

    // --- slot 48 (+0x180): render_line ---
    // Used for checkbox checkmark, tab separators
    virtual void render_line(engine_rect const* p1, engine_rect const* p2,
                             std::uint32_t color, float thickness) {
        (void)p1; (void)p2; (void)color; (void)thickness;
    }

    // --- slot 49 (+0x188): render_polygon_outline ---
    virtual void render_polygon_outline(engine_rect const* points, std::uint32_t color) {
        (void)points; (void)color;
    }

    // slots 50-55 unobserved
    virtual void reserved_50() {}
    virtual void reserved_51() {}
    virtual void reserved_52() {}
    virtual void reserved_53() {}
    virtual void reserved_54() {}
    virtual void reserved_55() {}

    // --- slot 56 (+0x1C0): measure_text ---
    virtual int measure_text(const char* s, void* out_size,
                             std::uint32_t len, std::uint32_t flag) {
        (void)s; (void)out_size; (void)len; (void)flag; return 0;
    }

    // --- slot 57 (+0x1C8): measure_text_with_font ---
    virtual int measure_text_with_font(void* font, const char* s,
                                       std::uint32_t len, std::uint32_t flag) {
        (void)font; (void)s; (void)len; (void)flag; return 0;
    }

    virtual void reserved_58() {}

    // --- slot 59 (+0x1D8): render_text ---
    virtual void render_text(engine_rect const* pos, engine_rect const* r,
                             void* font, const char* s, std::uint32_t len,
                             std::uint32_t color,
                             std::uint32_t align1, std::uint32_t align2,
                             std::uint32_t flag) {
        (void)pos; (void)r; (void)font; (void)s; (void)len;
        (void)color; (void)align1; (void)align2; (void)flag;
    }

    // --- slot 60 (+0x1E0): render_text_clipped ---
    virtual void render_text_clipped(engine_rect const* pos, engine_rect const* r,
                                     void* font, const char* s, std::uint32_t len,
                                     std::uint32_t color,
                                     std::uint32_t align1, std::uint32_t align2,
                                     std::uint32_t flag) {
        (void)pos; (void)r; (void)font; (void)s; (void)len;
        (void)color; (void)align1; (void)align2; (void)flag;
    }

    // --- slot 61 (+0x1E8): render_text_with_shadow ---
    virtual void render_text_with_shadow(engine_rect const* pos, engine_rect const* r,
                                         void* font, const char* s,
                                         std::uint32_t color,
                                         std::uint32_t align1, std::uint32_t align2,
                                         std::uint32_t flag) {
        (void)pos; (void)r; (void)font; (void)s;
        (void)color; (void)align1; (void)align2; (void)flag;
    }

    // slots 62-64 unobserved
    virtual void reserved_62() {}
    virtual void reserved_63() {}
    virtual void reserved_64() {}

    // --- slot 65 (+0x208): render_filled_circle ---
    // Used for colorpicker cursor
    virtual void render_filled_circle(engine_rect const* pos, std::uint32_t color,
                                      std::uint32_t flag, float radius) {
        (void)pos; (void)color; (void)flag; (void)radius;
    }

    // --- slot 66 (+0x210): pop_clip ---
    virtual void pop_clip() {}
};

} // namespace GUI
