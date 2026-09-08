#pragma once

/**
 * @file        repository.hpp
 * @brief       RAII handle around git_repository*
 * @description Move-only owner of a libgit2 repository handle. Opens an
 *              existing repository on disk; never creates one. The
 *              underlying git_repository* is fully encapsulated — the
 *              public API surface is ASE-native.
 *
 *              The class intentionally exposes only the operations the
 *              ASE explorer's git-status scanner needs. New callers
 *              that need additional libgit2 features should add small,
 *              focused methods here rather than leaking native_handle()
 *              through to consumers.
 *
 * @module      ase-adp-libgit2
 * @layer       adapter
 */

#include <ase/adp/libgit2/error.hpp>

#include <ase/types/result.hpp>

#include <string>

// Forward-declare libgit2 types so this header has zero include dep on
// <git2.h>. The actual definitions live in libgit2's headers and are
// only pulled in by the adapter's .cpp files.
struct git_repository;

namespace ase::adp::libgit2 {

class Repository {
public:
    /**
     * Open an existing repository at `path`. `path` may point at the
     * working tree root or at any subdirectory inside it; libgit2's
     * discover logic locates the .git directory.
     *
     * @return Repository on success, Error on failure (path missing,
     *         not a git repo, ambiguous, …). Check with is_ok()/is_err();
     *         take the value with unwrap(), the error with unwrap_err().
     */
    static ase::types::Result<Repository, Error> open(const std::string& path);

    /**
     * Open the repository associated with a submodule located at
     * `submodule_path` inside `parent_path`. Convenience for the
     * scanner: avoids parsing .gitmodules manually.
     *
     * @return Repository on success, Error on failure. Same carrier and
     *         same inspection API as open().
     */
    static ase::types::Result<Repository, Error> open_submodule(
        const std::string& parent_path,
        const std::string& submodule_path);

    Repository() noexcept = default;
    ~Repository();

    Repository(const Repository&)            = delete;
    Repository& operator=(const Repository&) = delete;
    Repository(Repository&& other) noexcept;
    Repository& operator=(Repository&& other) noexcept;

    /** True when this handle owns a valid git_repository*. */
    bool valid() const noexcept { return m_repo != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    /** Working-tree root (with trailing slash stripped). Empty when invalid. */
    std::string workdir() const;

    /** Path to the `.git` directory. Empty when invalid. */
    std::string gitdir() const;

    /** True if this is a bare repository (no working tree). */
    bool is_bare() const;

    /** True if HEAD is detached (not on any branch). */
    bool head_detached() const;

    /**
     * Short SHA of HEAD as 7 hex chars, or empty if HEAD is unborn or
     * the repo is invalid. Useful as a cheap change-marker for the
     * scanner: when this string flips, the worktree is on a new
     * commit and the status cache needs invalidation.
     */
    std::string head_short() const;

    /** Native handle. Adapter-internal; not exposed in installed headers. */
    git_repository* native_handle() noexcept { return m_repo; }
    const git_repository* native_handle() const noexcept { return m_repo; }

private:
    explicit Repository(git_repository* repo) noexcept : m_repo(repo) {}
    void close() noexcept;

    git_repository* m_repo = nullptr;
};

}  // namespace ase::adp::libgit2
