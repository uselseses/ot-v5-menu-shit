#pragma once
// =============================================================================
// encrypted_string_t<N>
//
// At construction, XOR-encrypts a literal byte-by-byte against the
// multi-key stream in xor_keys.hpp. decrypt() walks the buffer and
// returns a plain const char*. The decrypted text lives in a static
// scratch buffer guarded by " ... XOR ENC BUF FULL ... (" --- the
// literal overflow guard string from the original binary
// (analysis/all_menu_strings.txt: 0x1401f76f8).
//
// Use in headers via GUI_ENC("text") --- expands to a constexpr
// encrypted_string_t with the right N.
// =============================================================================

#include <cstddef>
#include <cstdint>

#include <GUI/string/xor_keys.hpp>

namespace GUI {

namespace detail {
    inline constexpr std::size_t k_decrypt_buf_size = 4096;

    // Round-robin scratch buffer. Each decrypt() bumps the cursor by
    // the returned string's length+1 so concurrent calls within a
    // single thread don't clobber each other in printf-style use.
    char* acquire_decrypt_slot(std::size_t needed) noexcept;
} // namespace detail

template <std::size_t N>
class encrypted_string_t {
public:
    // Compile-time encrypt of an N-1-long literal (N includes the NUL).
    constexpr encrypted_string_t(const char (&lit)[N]) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] = static_cast<std::uint8_t>(lit[i])
                     ^ GUI::string_::key_byte(i);
        }
    }

    // Decrypt into the shared scratch buffer. Returned pointer is
    // valid until the buffer wraps around (~4 KB of text later).
    const char* decrypt() const {
        char* dst = detail::acquire_decrypt_slot(N);
        if (!dst) return "";
        for (std::size_t i = 0; i < N; ++i) {
            dst[i] = static_cast<char>(
                static_cast<std::uint8_t>(data_[i])
                ^ GUI::string_::key_byte(i));
        }
        return dst;
    }

    constexpr std::size_t size() const noexcept { return N - 1; } // exclude NUL

private:
    std::uint8_t data_[N]{};
};

// Deduction guide --- lets `encrypted_string_t("hi")` pick the right N.
template <std::size_t N>
encrypted_string_t(const char (&)[N]) -> encrypted_string_t<N>;

} // namespace GUI

// Convenience macro --- one source token per encrypted literal.
//   const auto label = GUI_ENC("Antiaim");
//   render_text(font, pos, color, label.decrypt());
#define GUI_ENC(lit) (::GUI::encrypted_string_t{lit})
