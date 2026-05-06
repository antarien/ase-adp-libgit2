/**
 * @file        status.cpp
 * @brief       Implementation of scan()
 * @description Builds a git_status_options from the supplied options,
 *              walks the resulting git_status_list, decodes each entry
 *              into a StatusEntry, and tallies the per-bit aggregate
 *              counts as it goes. The list handle is freed before
 *              return.
 *
 * @module      ase-adp-libgit2
 * @layer       adapter
 */

#include <ase/adp/libgit2/status.hpp>

#include <git2/status.h>
#include <git2/diff.h>

namespace ase::adp::libgit2 {

namespace {

// Decode a single libgit2 entry's bit-set into our StatusEntry. The
// path comes from the index_to_workdir delta when present (covers
// modified, deleted, typechanged), otherwise from the head_to_index
// delta (covers added, indexed-renamed). At least one of the two
// deltas is always populated for an entry that survived the filter.
StatusEntry decode_entry(const git_status_entry* e) {
    StatusEntry out;
    out.flags = static_cast<uint32_t>(e->status);

    const git_diff_delta* delta = nullptr;
    if (e->index_to_workdir != nullptr) {
        delta = e->index_to_workdir;
    } else if (e->head_to_index != nullptr) {
        delta = e->head_to_index;
    }

    if (delta != nullptr) {
        if (delta->new_file.path != nullptr) {
            out.path = delta->new_file.path;
        }
        // Renames carry the prior path in old_file.
        const bool renamed_idx = (out.flags & StatusIndexRenamed) != 0;
        const bool renamed_wt  = (out.flags & StatusWtRenamed) != 0;
        if ((renamed_idx || renamed_wt) &&
            delta->old_file.path != nullptr &&
            delta->old_file.path != delta->new_file.path) {
            out.old_path = delta->old_file.path;
        }
    }
    return out;
}

void update_summary(StatusSummary& s, uint32_t flags) {
    // A single path may carry multiple bits (e.g. INDEX_MODIFIED +
    // WT_MODIFIED). We count it once per *category*, not once per bit:
    //  - "modified" if any modify bit set
    //  - "added"    if INDEX_NEW
    //  - "deleted"  if any delete bit set
    //  - "renamed"  if any rename bit set
    //  - "untracked"if WT_NEW (and not also INDEX_NEW)
    //  - "conflicted" / "ignored" mirror their bits
    bool counted_dirty = false;
    auto bump = [&](uint32_t& c) { c += 1; counted_dirty = true; };

    if (flags & StatusConflicted) {
        bump(s.conflicted);
    } else if (flags & StatusIgnored) {
        s.ignored += 1;
        // ignored is intentionally excluded from dirty_total
    } else {
        if (flags & (StatusIndexModified | StatusWtModified |
                     StatusIndexTypechange | StatusWtTypechange)) {
            bump(s.modified);
        }
        if (flags & StatusIndexNew) {
            bump(s.added);
        } else if (flags & StatusWtNew) {
            bump(s.untracked);
        }
        if (flags & (StatusIndexDeleted | StatusWtDeleted)) {
            bump(s.deleted);
        }
        if (flags & (StatusIndexRenamed | StatusWtRenamed)) {
            bump(s.renamed);
        }
    }
    if (counted_dirty) s.dirty_total += 1;
}

}  // namespace

std::variant<StatusResult, Error> scan(Repository& repo,
                                       const StatusOptions& opts)
{
    if (!repo.valid()) {
        Error e;
        e.code = -1;
        e.message = "scan() called on invalid Repository";
        return e;
    }

    git_status_options sopt = GIT_STATUS_OPTIONS_INIT;
    sopt.show  = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    sopt.flags = GIT_STATUS_OPT_INCLUDE_UNMODIFIED * 0;  // exclude clean

    if (opts.include_untracked) {
        sopt.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
    }
    if (opts.recurse_untracked) {
        sopt.flags |= GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;
    }
    if (opts.include_ignored) {
        sopt.flags |= GIT_STATUS_OPT_INCLUDE_IGNORED;
    }
    if (opts.renames_index) {
        sopt.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
    }
    if (opts.renames_workdir) {
        sopt.flags |= GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;
    }
    sopt.flags |= GIT_STATUS_OPT_EXCLUDE_SUBMODULES;  // scanned per-submodule

    git_status_list* list = nullptr;
    const int rc = git_status_list_new(&list, repo.native_handle(), &sopt);
    if (rc != 0 || list == nullptr) {
        return capture_last_error(rc);
    }

    StatusResult result;
    const size_t n = git_status_list_entrycount(list);
    result.entries.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        const git_status_entry* e = git_status_byindex(list, i);
        if (e == nullptr) continue;
        // Skip entries flagged CURRENT — sopt should already exclude
        // them, but the bit is reachable on rare edge cases (e.g.
        // typechange that resolves identical content).
        if (e->status == GIT_STATUS_CURRENT) continue;
        StatusEntry decoded = decode_entry(e);
        update_summary(result.summary, decoded.flags);
        result.entries.push_back(std::move(decoded));
    }
    git_status_list_free(list);
    return result;
}

}  // namespace ase::adp::libgit2
