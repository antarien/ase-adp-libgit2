/**
 * @file        example_main.cpp
 * @brief       Tiny self-test of the ase-adp-libgit2 adapter
 * @description Opens the directory passed on argv[1] (defaults to ".")
 *              as a libgit2 repository, prints HEAD short SHA, runs a
 *              status scan with default options, prints the summary,
 *              and lists submodules with their initialization state.
 *              Doubles as smoke test + reference usage example.
 *
 *              Output goes through ase-log, not printf: a direct write to
 *              stdout bypasses the category system, shows up in no --log
 *              selection and lands in no tier log file.
 *
 *              The categories are derived from the state, not guessed.
 *              Repository::open() fails when the named path is not a
 *              repository at all — the target was never there, so it is
 *              RESOURCE_UNAVAIL. The library init, the status scan and the
 *              submodule walk are calls that RAN and returned a failure,
 *              which is the sharp distinction that puts them under
 *              HOST_OP_FAILED. Every one of them ends the run (return 1),
 *              and that is what places them on the error level.
 *
 *              Build standalone:
 *                cmake -B build && cmake --build build
 *                ./build/ase-adp-libgit2-example /path/to/repo
 *
 * @module      ase-adp-libgit2
 * @layer       adapter
 */

#include <ase/adp/libgit2/init.hpp>
#include <ase/adp/libgit2/repository.hpp>
#include <ase/adp/libgit2/status.hpp>
#include <ase/adp/libgit2/submodule.hpp>
#include <ase/adp/libgit2/version.hpp>

#include <ase/log/log.hpp>

#include <string>

int main(int argc, char** argv) {
    ase::log::info("[AdpLibgit2Example] ase-adp-libgit2 v{} [{}]",
                   ase::adp::libgit2::VERSION_STRING,
                   ase::adp::libgit2::VERSION_STATUS);

    ase::adp::libgit2::Library lib;
    if (!lib.initialised()) {
        ase::log::error(ase::log::ERR::CAT::HOST_OP_FAILED,
                        "AdpLibgit2Example", "git_libgit2_init",
                        static_cast<float>(lib.refcount()));
        return 1;
    }

    const std::string path = (argc > 1) ? argv[1] : ".";
    auto opened = ase::adp::libgit2::Repository::open(path);
    if (opened.is_err()) {
        const auto& err = opened.unwrap_err();
        const std::string detail = "code=" + std::to_string(err.code)
                                 + " klass=" + std::to_string(err.klass)
                                 + " msg=" + err.message;
        ase::log::error(ase::log::ERR::CAT::RESOURCE_UNAVAIL,
                        "AdpLibgit2Example", path.c_str(), detail.c_str());
        return 1;
    }
    auto& repo = opened.unwrap();

    ase::log::info("[AdpLibgit2Example] workdir : {}", repo.workdir());
    ase::log::info("[AdpLibgit2Example] HEAD    : {}{}",
                   repo.head_short(),
                   repo.head_detached() ? " (detached)" : "");

    auto scanned = ase::adp::libgit2::scan(repo);
    if (scanned.is_err()) {
        ase::log::error(ase::log::ERR::CAT::HOST_OP_FAILED,
                        "AdpLibgit2Example", "git_status_list_new",
                        scanned.unwrap_err().message.c_str());
        return 1;
    }
    const auto& result = scanned.unwrap();
    const auto& s = result.summary;
    ase::log::info("[AdpLibgit2Example] status  : M={} A={} D={} R={} ?={} !={} "
                   "ignored={} dirty_total={} entries={}",
                   s.modified, s.added, s.deleted, s.renamed,
                   s.untracked, s.conflicted, s.ignored, s.dirty_total,
                   result.entries.size());

    auto subs = ase::adp::libgit2::submodule::list(repo);
    if (subs.is_err()) {
        ase::log::error(ase::log::ERR::CAT::HOST_OP_FAILED,
                        "AdpLibgit2Example", "git_submodule_foreach",
                        subs.unwrap_err().message.c_str());
        return 1;
    }
    const auto& v = subs.unwrap();
    ase::log::info("[AdpLibgit2Example] submods : {} registered", v.size());
    size_t initialised = 0;
    for (const auto& info : v) if (info.initialized) initialised += 1;
    ase::log::info("[AdpLibgit2Example]         : {} initialised in working tree",
                   initialised);
    return 0;
}
