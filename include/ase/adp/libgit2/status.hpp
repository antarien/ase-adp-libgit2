#pragma once

/**
 * @file        status.hpp
 * @brief       Working-tree + index status enumeration for a Repository
 * @description The scanner's primary entry point. Builds a libgit2
 *              git_status_list internally, decodes each entry into a
 *              StatusEntry, and returns the lot as a vector. The native
 *              handle does NOT escape this header — consumers iterate
 *              over plain ASE-native data only.
 *
 *              StatusFlags is a bit-field mirroring libgit2's GIT_STATUS_*
 *              enumeration. We keep the same bit values so future
 *              cross-references against libgit2 docs stay readable, but
 *              the enum is declared inside ase::adp::libgit2 with no
 *              dependency on <git2.h> in the public header.
 *
 *              The aggregate counts (modified, added, deleted, …) are
 *              precomputed here so consumers (e.g. the explorer's
 *              per-folder rollup) get O(1) tallies without re-walking
 *              the entries vector.
 *
 * @module      ase-adp-libgit2
 * @layer       adapter
 */

#include <ase/adp/libgit2/error.hpp>
#include <ase/adp/libgit2/repository.hpp>

#include <ase/types/result.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace ase::adp::libgit2 {

/**
 * Per-file status bits. Mirrors GIT_STATUS_* from <git2/status.h> 1:1
 * (deliberate value-stability for cross-reference). A given file may
 * carry both an INDEX_* bit (index vs HEAD) and a WT_* bit (working
 * tree vs index) at the same time — e.g. modified-and-staged.
 */
enum StatusFlags : uint32_t {
    StatusCurrent          = 0,
    StatusIndexNew         = 1u << 0,
    StatusIndexModified    = 1u << 1,
    StatusIndexDeleted     = 1u << 2,
    StatusIndexRenamed     = 1u << 3,
    StatusIndexTypechange  = 1u << 4,
    StatusWtNew            = 1u << 7,   ///< untracked
    StatusWtModified       = 1u << 8,
    StatusWtDeleted        = 1u << 9,
    StatusWtTypechange     = 1u << 10,
    StatusWtRenamed        = 1u << 11,
    StatusWtUnreadable     = 1u << 12,
    StatusIgnored          = 1u << 14,
    StatusConflicted       = 1u << 15,
};

/** Convenience masks. */
constexpr uint32_t StatusMaskIndex =
    StatusIndexNew | StatusIndexModified | StatusIndexDeleted |
    StatusIndexRenamed | StatusIndexTypechange;

constexpr uint32_t StatusMaskWorkTree =
    StatusWtNew | StatusWtModified | StatusWtDeleted |
    StatusWtTypechange | StatusWtRenamed | StatusWtUnreadable;

struct StatusEntry {
    std::string path;        ///< repo-relative POSIX path
    std::string old_path;    ///< populated only on rename, otherwise empty
    uint32_t    flags = 0;   ///< bit-OR of StatusFlags
};

/**
 * Aggregate counts over a status scan. All counts are over distinct
 * paths — a path with both INDEX_MODIFIED and WT_MODIFIED counts once
 * in `modified`. `dirty_total` is the count of paths that are not
 * Current and not Ignored (i.e. anything the user would call "dirty").
 */
struct StatusSummary {
    uint32_t modified    = 0;
    uint32_t added       = 0;
    uint32_t deleted     = 0;
    uint32_t renamed     = 0;
    uint32_t untracked   = 0;
    uint32_t conflicted  = 0;
    uint32_t ignored     = 0;
    uint32_t dirty_total = 0;
};

struct StatusOptions {
    bool include_untracked = true;
    bool include_ignored   = false;
    bool recurse_untracked = true;   ///< descend into untracked directories
    bool renames_index     = false;  ///< detect renames in index (slow)
    bool renames_workdir   = false;  ///< detect renames in worktree (slow)
};

struct StatusResult {
    std::vector<StatusEntry> entries;
    StatusSummary            summary;
};

/**
 * Run git_status_list on `repo` with the supplied options and decode
 * the result into a StatusResult. The libgit2 handle is freed before
 * this function returns; consumers see only owning ASE types.
 *
 * @return StatusResult on success, Error on failure. Check with
 *         is_ok()/is_err(); take the value with unwrap(), the error
 *         with unwrap_err().
 */
ase::types::Result<StatusResult, Error> scan(
    Repository& repo,
    const StatusOptions& opts = {});

}  // namespace ase::adp::libgit2
