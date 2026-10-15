#pragma once
// =============================================================================
// build_options.hpp — compile-time switches.
//
// The original binary's changelog (Aug 10-28):
//   "extended 'build_options.hpp' (compile-time config)"
//   "isolate framework specific config values to their own file,
//    separate from cheat settings"
//
// This header keeps the framework's compile-time identity in one place.
// Values are intentionally `inline constexpr` (C++17) so they're ODR-safe
// when included from headers and headerless TUs alike.
// =============================================================================

#define GUI_LIBRARY_VERSION_STRING "v0.4.0"
#define GUI_ARCH_NAME_STRING       "64-bit"

// Compile this build as the demo sandbox (mirrors GUI_64_DX11.exe).
// When 0, the credentials below are not embedded.
#ifndef BUILD_DEMO
#define BUILD_DEMO 1
#endif

// Default Segoe UI on Windows. The original loads it from the OS at runtime
// (Aug 10-28: "switched font from 'Noto Sans' to 'Segoe UI', no more font
// bytes in binary").
#ifndef GUI_DEFAULT_FONT_PATH
#define GUI_DEFAULT_FONT_PATH L"C:\\Windows\\Fonts\\segoeui.ttf"
#endif

#if BUILD_DEMO
// Embedded test fixture — original author's cloud account, hard-coded in
// the binary at sub_14006EFC0 (renderer init). Preserved verbatim so the
// reconstructed binary lights up against the live cloud server.
#define DEMO_CLOUD_ID    "04728475a5b5403ba6413d8d9c078ac2"
#define DEMO_CLOUD_KEY   "f95693775df05565ba0973d69c045477"
#define DEMO_PASSWORD    "1234"
#define DEMO_USERNAME    "William"
#define DEMO_SERVER_IP   "52.45.58.3"
#define DEMO_SERVER_PORT 35710
#endif
