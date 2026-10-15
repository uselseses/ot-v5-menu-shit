#pragma once
// demo_gui_engine_interface --- the sandbox's bridge from the GUI
// framework to DX11 + Win32. Vtable slot semantics recovered from
// dec_WinMain.c + dec_RendererInit.c + dec_PerFrame.c:
//
//   slot offset  | role                                | evidence
//   -------------+-------------------------------------+--------------------
//   +8           | get_dx11_handles()                  | RendererInit calls vt[1] on qword_140245EB8
//                | (also "init(bool)" on the engine)   | WinMain calls vt[1] on qword_14023A680
//   +16          | shutdown()                          | WinMain shutdown path
//   +40          | begin_frame()                       | PerFrame line ~101
//   +48          | end_frame() / present()             | PerFrame line ~238
//   +96          | get_screen_size(int* w, int* h)     | PerFrame and RendererInit
//
// Phase 0: declaration only.

#include <GUI/core/gui_engine_interface.hpp>

namespace demo {

class demo_gui_engine_interface : public GUI::gui_engine_interface {
    // members filled in Phase 1
};

} // namespace demo
