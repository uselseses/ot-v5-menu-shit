#pragma once
// watermark_flag_e --- recovered from string cluster
//   0x1401fb128..0x1401fb1e8  ("Build name", "Frames per second",
//   "Latency to server", "System time", "Server map", "Server type",
//   "Server tickrate", "Username")
// and the changelog (Aug 10-28): "added dropdown to select options for
// watermark (see "watermark_flag_e" enum)".
//
// The binary literally contains "@TODO: add remaining flags" so this
// list is intentionally short --- matches the original author's state.
#include <cstdint>

namespace GUI {

enum class watermark_flag_e : std::uint32_t {
    none              = 0,
    build_name        = 1u << 0, // "Build name"
    username          = 1u << 1, // "Username"
    latency_to_server = 1u << 2, // "Latency to server"
    frames_per_second = 1u << 3, // "Frames per second"
    system_time       = 1u << 4, // "System time"
    server_map        = 1u << 5, // "Server map"
    server_type       = 1u << 6, // "Server type"
    server_tickrate   = 1u << 7, // "Server tickrate"
};

constexpr watermark_flag_e operator|(watermark_flag_e a, watermark_flag_e b) {
    return static_cast<watermark_flag_e>(
        static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
constexpr watermark_flag_e operator&(watermark_flag_e a, watermark_flag_e b) {
    return static_cast<watermark_flag_e>(
        static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

} // namespace GUI
