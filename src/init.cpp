/**
 * @file        init.cpp
 * @brief       Implementation of the Library RAII guard
 * @description git_libgit2_init() is refcounted — the first call inside
 *              the process initialises libgit2's globals (OpenSSL state,
 *              libssh2 if linked, the cache subsystem) and increments
 *              the refcount; subsequent calls just bump the count.
 *              git_libgit2_shutdown() decrements; the final 1→0
 *              transition tears the globals down.
 *
 * @module      ase-adp-libgit2
 * @layer       adapter
 */

#include <ase/adp/libgit2/init.hpp>

#include <git2/global.h>

namespace ase::adp::libgit2 {

Library::Library() {
    m_refcount = git_libgit2_init();
}

Library::~Library() {
    if (m_refcount > 0) {
        git_libgit2_shutdown();
    }
}

}  // namespace ase::adp::libgit2
