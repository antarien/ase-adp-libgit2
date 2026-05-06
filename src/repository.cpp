/**
 * @file        repository.cpp
 * @brief       Implementation of Repository
 * @description Opens libgit2 repositories and exposes the small set of
 *              accessors the explorer's scanner relies on. Move-only
 *              ownership; git_repository_free() runs in close() so the
 *              destructor and move-assignment are both safe.
 *
 * @module      ase-adp-libgit2
 * @layer       adapter
 */

#include <ase/adp/libgit2/repository.hpp>

#include <git2/repository.h>
#include <git2/refs.h>
#include <git2/oid.h>
#include <git2/buffer.h>
#include <git2/submodule.h>

#include <utility>

namespace ase::adp::libgit2 {

namespace {

// Strip a single trailing slash from `s` if present. libgit2 returns
// workdir() with a trailing slash, but consumers usually want the
// canonical form without one (consistent with std::filesystem::path).
std::string strip_trailing_slash(std::string s) {
    if (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

}  // namespace

std::variant<Repository, Error> Repository::open(const std::string& path) {
    git_repository* repo = nullptr;
    const int rc = git_repository_open(&repo, path.c_str());
    if (rc != 0 || repo == nullptr) {
        return capture_last_error(rc);
    }
    return Repository(repo);
}

std::variant<Repository, Error> Repository::open_submodule(
    const std::string& parent_path,
    const std::string& submodule_path)
{
    auto parent = open(parent_path);
    if (auto* err = std::get_if<Error>(&parent)) {
        return *err;
    }
    auto& parent_repo = std::get<Repository>(parent);

    git_submodule* sm = nullptr;
    int rc = git_submodule_lookup(&sm, parent_repo.native_handle(),
                                  submodule_path.c_str());
    if (rc != 0 || sm == nullptr) {
        return capture_last_error(rc);
    }

    git_repository* sm_repo = nullptr;
    rc = git_submodule_open(&sm_repo, sm);
    git_submodule_free(sm);
    if (rc != 0 || sm_repo == nullptr) {
        return capture_last_error(rc);
    }
    return Repository(sm_repo);
}

Repository::~Repository() { close(); }

Repository::Repository(Repository&& other) noexcept
    : m_repo(other.m_repo) {
    other.m_repo = nullptr;
}

Repository& Repository::operator=(Repository&& other) noexcept {
    if (this != &other) {
        close();
        m_repo = other.m_repo;
        other.m_repo = nullptr;
    }
    return *this;
}

void Repository::close() noexcept {
    if (m_repo != nullptr) {
        git_repository_free(m_repo);
        m_repo = nullptr;
    }
}

std::string Repository::workdir() const {
    if (m_repo == nullptr) return {};
    const char* w = git_repository_workdir(m_repo);
    return w == nullptr ? std::string{} : strip_trailing_slash(w);
}

std::string Repository::gitdir() const {
    if (m_repo == nullptr) return {};
    const char* g = git_repository_path(m_repo);
    return g == nullptr ? std::string{} : strip_trailing_slash(g);
}

bool Repository::is_bare() const {
    return m_repo != nullptr && git_repository_is_bare(m_repo) != 0;
}

bool Repository::head_detached() const {
    return m_repo != nullptr && git_repository_head_detached(m_repo) > 0;
}

std::string Repository::head_short() const {
    if (m_repo == nullptr) return {};
    git_reference* head = nullptr;
    if (git_repository_head(&head, m_repo) != 0 || head == nullptr) {
        return {};
    }
    const git_oid* oid = git_reference_target(head);
    if (oid == nullptr) {
        git_reference_free(head);
        return {};
    }
    char hex[8] = {0};
    git_oid_tostr(hex, sizeof(hex), oid);  // 7 chars + NUL
    git_reference_free(head);
    return std::string{hex};
}

}  // namespace ase::adp::libgit2
