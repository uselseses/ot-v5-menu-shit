#pragma once
// =============================================================================
// wm::types::thread_safe_t<Read, Write>
//
// SRWLOCK-backed wrapper for cross-thread value access.
//
// RTTI evidence in GUI_64_DX11.exe:
//   .?AU?$thread_safe_t@_N_N@types@wm@@
//       --> wm::types::thread_safe_t<bool,bool>          (checkbox state)
//   .?AU?$thread_safe_t@W4keybinder_mode_e@GUI@@W412@@types@wm@@
//       --> wm::types::thread_safe_t<keybinder_mode_e, keybinder_mode_e>
//
// Imports evidence: the binary uses InitializeSRWLock /
// AcquireSRWLockExclusive / ReleaseSRWLockExclusive --- shared mode is
// only used for read-mostly state, which matches the get()/set() split
// here.
// =============================================================================

#include <type_traits>
#include <utility>

#include <Windows.h>

namespace wm::types {

template <typename Read, typename Write = Read>
class thread_safe_t {
public:
    using read_type  = Read;
    using write_type = Write;

    thread_safe_t() noexcept(std::is_nothrow_default_constructible_v<Read>)
        : value_{} { InitializeSRWLock(&lock_); }

    explicit thread_safe_t(Read initial)
        : value_{std::move(initial)} { InitializeSRWLock(&lock_); }

    // SRWLOCKs are non-copyable; the wrapper is intentionally pinned.
    thread_safe_t(const thread_safe_t&)            = delete;
    thread_safe_t& operator=(const thread_safe_t&) = delete;

    Read get() const {
        AcquireSRWLockShared(&lock_);
        Read copy = value_;
        ReleaseSRWLockShared(&lock_);
        return copy;
    }

    void set(Write value) {
        AcquireSRWLockExclusive(&lock_);
        // The Read-vs-Write distinction lets non-trivial controls
        // (e.g. multi-select dropdowns where Read is a vector and
        // Write is a single index) translate on the way in.
        assign_(std::move(value));
        ReleaseSRWLockExclusive(&lock_);
    }

    // Run `fn(Read&)` while holding the exclusive lock. Used by the
    // GUI for read-modify-write transactions (e.g. flipping a single
    // bit in a flag field).
    template <typename Fn>
    void with_lock(Fn&& fn) {
        AcquireSRWLockExclusive(&lock_);
        std::forward<Fn>(fn)(value_);
        ReleaseSRWLockExclusive(&lock_);
    }

private:
    // Default assign: convert Write to Read via direct assignment.
    // Specialize via overload if the types differ in a non-trivial way.
    template <typename W = Write,
              std::enable_if_t<std::is_assignable_v<Read&, W&&>, int> = 0>
    void assign_(W&& w) { value_ = std::forward<W>(w); }

    Read              value_;
    mutable SRWLOCK   lock_{};
};

} // namespace wm::types
