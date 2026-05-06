/**
 * @file        submodule.cpp
 * @brief       Implementation of submodule::list()
 * @description Walks every submodule registered in a Repository via
 *              git_submodule_foreach and decodes each one into a
 *              SubmoduleInfo. The libgit2 callback type forces a
 *              C-style trampoline; we keep the user_data plumbing
 *              minimal — just the result vector + an optional Error
 *              that pre-empts further iteration.
 *
 * @module      ase-adp-libgit2
 * @layer       adapter
 */

#include <ase/adp/libgit2/submodule.hpp>

#include <git2/submodule.h>

namespace ase::adp::libgit2::submodule {

namespace {

struct WalkState {
    std::vector<SubmoduleInfo>* out = nullptr;
};

// libgit2's foreach callback returns 0 to continue, non-zero to stop.
// We never stop early — collecting every submodule is the whole point.
int collect_cb(git_submodule* sm, const char* name, void* user) {
    auto* st = static_cast<WalkState*>(user);

    SubmoduleInfo info;
    info.path = (name != nullptr) ? name : "";

    const char* url    = git_submodule_url(sm);
    const char* branch = git_submodule_branch(sm);
    if (url    != nullptr) info.url    = url;
    if (branch != nullptr) info.branch = branch;

    // Submodule status: bit 0x4000 (GIT_SUBMODULE_STATUS_IN_CONFIG)
    // is enough to know it's been initialised in .git/config — that's
    // what consumers actually care about.
    unsigned int status = 0;
    if (git_submodule_status(&status, git_submodule_owner(sm), name,
                             GIT_SUBMODULE_IGNORE_NONE) == 0) {
        // GIT_SUBMODULE_STATUS_IN_WD is set when the submodule has a
        // populated working directory checked out.
        info.initialized = (status & GIT_SUBMODULE_STATUS_IN_WD) != 0;
    }

    st->out->push_back(std::move(info));
    return 0;
}

}  // namespace

std::variant<std::vector<SubmoduleInfo>, Error> list(Repository& repo) {
    if (!repo.valid()) {
        Error e;
        e.code = -1;
        e.message = "submodule::list() called on invalid Repository";
        return e;
    }

    std::vector<SubmoduleInfo> result;
    WalkState st{ &result };
    const int rc = git_submodule_foreach(repo.native_handle(), collect_cb, &st);
    if (rc != 0) {
        return capture_last_error(rc);
    }
    return result;
}

}  // namespace ase::adp::libgit2::submodule
