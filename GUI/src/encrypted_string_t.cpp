// =============================================================================
// encrypted_string_t.cpp --- ring-buffer scratch slot allocator.
//
// One ring per thread (TLS). On overflow we wrap around AND, when the
// debug build asks for it, log the binary's literal overflow guard
// (recovered from .rdata at 0x1401f76f8 in the original).
// =============================================================================

#include <GUI/string/encrypted_string_t.hpp>

#include <cstdio>

namespace GUI::detail {

namespace {
    thread_local char  s_buffer[k_decrypt_buf_size];
    thread_local std::size_t s_cursor = 0;
}

char* acquire_decrypt_slot(std::size_t needed) noexcept
{
    if (needed > k_decrypt_buf_size) {
        // The binary literally contains this string at the overflow
        // path; preserve the message so behavior matches.
        std::fprintf(stderr, " ... XOR ENC BUF FULL ... (%zu bytes)\n", needed);
        return nullptr;
    }
    if (s_cursor + needed > k_decrypt_buf_size) s_cursor = 0;
    char* dst = &s_buffer[s_cursor];
    s_cursor += needed;
    return dst;
}

} // namespace GUI::detail
