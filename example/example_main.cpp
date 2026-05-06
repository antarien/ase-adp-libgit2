/**
 * @file        example_main.cpp
 * @brief       Tiny self-test of the ase-adp-libgit2 adapter
 * @description Opens the directory passed on argv[1] (defaults to ".")
 *              as a libgit2 repository, prints HEAD short SHA, runs a
 *              status scan with default options, prints the summary,
 *              and lists submodules with their initialization state.
 *              Doubles as smoke test + reference usage example.
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

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    std::printf("ase-adp-libgit2 v%s [%s]\n",
                ase::adp::libgit2::VERSION_STRING,
                ase::adp::libgit2::VERSION_STATUS);

    ase::adp::libgit2::Library lib;
    if (!lib.initialised()) {
        std::printf("FAIL: git_libgit2_init() returned %d\n", lib.refcount());
        return 1;
    }

    const std::string path = (argc > 1) ? argv[1] : ".";
    auto opened = ase::adp::libgit2::Repository::open(path);
    if (auto* err = std::get_if<ase::adp::libgit2::Error>(&opened)) {
        std::printf("open(%s) failed: code=%d klass=%d msg=%s\n",
                    path.c_str(), err->code, err->klass, err->message.c_str());
        return 1;
    }
    auto& repo = std::get<ase::adp::libgit2::Repository>(opened);

    std::printf("workdir : %s\n", repo.workdir().c_str());
    std::printf("HEAD    : %s%s\n",
                repo.head_short().c_str(),
                repo.head_detached() ? " (detached)" : "");

    auto scanned = ase::adp::libgit2::scan(repo);
    if (auto* err = std::get_if<ase::adp::libgit2::Error>(&scanned)) {
        std::printf("scan failed: %s\n", err->message.c_str());
        return 1;
    }
    const auto& result = std::get<ase::adp::libgit2::StatusResult>(scanned);
    const auto& s = result.summary;
    std::printf("status  : M=%u A=%u D=%u R=%u ?=%u !=%u "
                "ignored=%u dirty_total=%u entries=%zu\n",
                s.modified, s.added, s.deleted, s.renamed,
                s.untracked, s.conflicted, s.ignored, s.dirty_total,
                result.entries.size());

    auto subs = ase::adp::libgit2::submodule::list(repo);
    if (auto* err = std::get_if<ase::adp::libgit2::Error>(&subs)) {
        std::printf("submodule::list failed: %s\n", err->message.c_str());
        return 1;
    }
    const auto& v = std::get<std::vector<ase::adp::libgit2::SubmoduleInfo>>(subs);
    std::printf("submods : %zu registered\n", v.size());
    size_t initialised = 0;
    for (const auto& info : v) if (info.initialized) initialised += 1;
    std::printf("        : %zu initialised in working tree\n", initialised);
    return 0;
}
