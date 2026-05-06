#pragma once

/**
 * @file        error.hpp
 * @brief       Adapter-local error type for libgit2 failures
 * @description libgit2 returns negative ints on failure and stores the
 *              human-readable message in a thread-local slot reachable
 *              via git_error_last(). The adapter surface returns either
 *              a value or an Error — no exceptions cross the boundary.
 *
 *              Error is a small POD-ish type with the libgit2 error code,
 *              the libgit2 error class, and a copy of the message string
 *              snapshotted at the failure point. Snapshotting is required:
 *              git_error_last() returns a pointer into thread-local
 *              storage that gets clobbered by the next libgit2 call on
 *              that thread.
 *
 * @module      ase-adp-libgit2
 * @layer       adapter
 */

#include <string>

namespace ase::adp::libgit2 {

struct Error {
    int         code     = 0;   ///< libgit2 error code (GIT_OK = 0, < 0 on error)
    int         klass    = 0;   ///< libgit2 error class (GITERR_*) at failure point
    std::string message;        ///< snapshot of git_error_last()->message

    /** True when this Error represents an actual failure (code != 0). */
    bool failed() const noexcept { return code != 0; }
    explicit operator bool() const noexcept { return failed(); }
};

/**
 * Build an Error from the libgit2 thread-local error slot. Call
 * immediately after a libgit2 call returns < 0; later calls on the
 * same thread will overwrite the message.
 */
Error capture_last_error(int code) noexcept;

}  // namespace ase::adp::libgit2
