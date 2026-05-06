/**
 * @file        error.cpp
 * @brief       Implementation of capture_last_error()
 * @description Snapshots the libgit2 thread-local error slot into an
 *              owning Error so the message survives subsequent calls.
 *
 * @module      ase-adp-libgit2
 * @layer       adapter
 */

#include <ase/adp/libgit2/error.hpp>

#include <git2/errors.h>

namespace ase::adp::libgit2 {

Error capture_last_error(int code) noexcept {
    Error e;
    e.code = code;
    const git_error* g = git_error_last();
    if (g != nullptr) {
        e.klass = g->klass;
        if (g->message != nullptr) {
            // Copying into std::string is intentional — git_error_last()
            // returns a pointer into thread-local storage that the next
            // libgit2 call on this thread will overwrite.
            e.message = g->message;
        }
    }
    return e;
}

}  // namespace ase::adp::libgit2
