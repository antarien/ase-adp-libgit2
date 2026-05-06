#pragma once

/**
 * @file        init.hpp
 * @brief       Process-wide libgit2 init/shutdown
 * @description libgit2 maintains a per-process refcount via
 *              git_libgit2_init() / git_libgit2_shutdown(). The adapter
 *              exposes a Library RAII guard so callers don't have to
 *              pair the calls manually. Construction increments the
 *              refcount; destruction decrements it. Multiple guards
 *              can safely coexist — libgit2 initialises once at
 *              refcount 0→1 and tears down once at N→0.
 *
 *              Threading: every thread that calls libgit2 functions
 *              must do so on a process where the refcount is > 0.
 *              Holding a single Library guard for the lifetime of
 *              main() is the simplest pattern.
 *
 * @module      ase-adp-libgit2
 * @layer       adapter
 */

namespace ase::adp::libgit2 {

class Library {
public:
    Library();
    ~Library();

    // Non-copyable, non-movable: refcount bookkeeping is one-shot per
    // instance. Hold one guard at top-level scope.
    Library(const Library&)            = delete;
    Library& operator=(const Library&) = delete;
    Library(Library&&)                 = delete;
    Library& operator=(Library&&)      = delete;

    /** True if git_libgit2_init() returned a positive refcount. */
    bool initialised() const noexcept { return m_refcount > 0; }

    /** Refcount returned by git_libgit2_init() — useful for diagnostics. */
    int refcount() const noexcept { return m_refcount; }

private:
    int m_refcount = 0;
};

}  // namespace ase::adp::libgit2
