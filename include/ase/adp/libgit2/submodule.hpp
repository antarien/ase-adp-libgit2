#pragma once

/**
 * @file        submodule.hpp
 * @brief       Submodule enumeration over a Repository
 * @description Wraps git_submodule_foreach so consumers can list a
 *              repository's submodules as a flat vector of paths +
 *              optional URLs. The native git_submodule* handle never
 *              leaves the .cpp file.
 *
 *              Nested submodules are NOT recursed automatically — call
 *              list() on each child Repository if you need a full tree.
 *              The explorer's scanner does exactly that, level by level,
 *              feeding each path back into the thread pool.
 *
 * @module      ase-adp-libgit2
 * @layer       adapter
 */

#include <ase/adp/libgit2/error.hpp>
#include <ase/adp/libgit2/repository.hpp>

#include <string>
#include <variant>
#include <vector>

namespace ase::adp::libgit2 {

struct SubmoduleInfo {
    std::string path;          ///< parent-relative POSIX path
    std::string url;           ///< configured URL (may be empty)
    std::string branch;        ///< configured branch (may be empty)
    bool        initialized = false;  ///< git submodule init has run
};

namespace submodule {

/** Enumerate every submodule registered in `repo`. */
std::variant<std::vector<SubmoduleInfo>, Error> list(Repository& repo);

}  // namespace submodule
}  // namespace ase::adp::libgit2
