# ase-adp-libgit2

[![Layer](https://img.shields.io/badge/Layer-Adapter-orange.svg)]()
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)]()
[![Kind](https://img.shields.io/badge/Kind-3rd%20Party%20Isolation-red.svg)]()
[![Status](https://img.shields.io/badge/Status-seed-lightgrey.svg)]()

> RAII C++20 wrapper around [libgit2](https://libgit2.org/) — isolates the C linkage of `<git2.h>` so the rest of the ASE codebase stays pure C++.

Part of [ASE — Antares Simulation Engine](https://github.com/antarien).

---

## Why this adapter exists

ASE's git-status scanner (`clients/ase-client-explorer`) needs to track the worktree state of **~145 nested submodules** and update the explorer's tree-view badges in real time. Two architectural decisions force this adapter:

1. **No `git` CLI fork+exec.** Spawning the `git` binary per submodule costs roughly 5–10 ms × 145 ≈ 1–2 s just for `fork+exec`, before a single byte of the index is read. libgit2 is the in-process alternative — `git_status_list_new()` against a medium repo is typically 2–10 ms; a clean repo is sub-millisecond.
2. **No `extern "C"` in regular project code.** The ASE validator forbids C-linkage declarations project-wide via the `EXTERN_C_FORBIDDEN` rule. `<git2.h>` is one giant `extern "C"` surface, so it can only be included from a whitelisted location. `adapter/ase-adp-libgit2/` is that whitelisted location; consumers include only `ase/adp/libgit2/*.hpp`.

The adapter additionally addresses two libgit2-specific footguns that are easy to get wrong from caller code:

- Every libgit2 handle (`git_repository*`, `git_status_list*`, `git_submodule*`, `git_reference*`, …) needs a paired `git_*_free()`. Manual pairs leak across exception paths and early returns. The wrappers here are move-only RAII owners, so close-on-destruction is automatic.
- `git_error_last()` returns a pointer into thread-local storage that the next libgit2 call on the same thread overwrites. The adapter snapshots the message into an owning `Error::message` at every failure site so consumers can inspect it after subsequent libgit2 work.

## Public API

```cpp
#include <ase/adp/libgit2/init.hpp>
#include <ase/adp/libgit2/repository.hpp>
#include <ase/adp/libgit2/status.hpp>
#include <ase/adp/libgit2/submodule.hpp>

// Process-wide init (refcounted; one guard at top of main is enough)
ase::adp::libgit2::Library lib;

auto opened = ase::adp::libgit2::Repository::open("/path/to/repo");
if (auto* err = std::get_if<ase::adp::libgit2::Error>(&opened)) {
    std::fprintf(stderr, "open failed: %s\n", err->message.c_str());
    return 1;
}
auto& repo = std::get<ase::adp::libgit2::Repository>(opened);

// Status scan with default options
auto scanned = ase::adp::libgit2::scan(repo);
if (auto* result = std::get_if<ase::adp::libgit2::StatusResult>(&scanned)) {
    const auto& s = result->summary;
    std::printf("M=%u A=%u D=%u ?=%u dirty=%u\n",
                s.modified, s.added, s.deleted, s.untracked, s.dirty_total);
    for (const auto& e : result->entries) {
        // e.path, e.flags (StatusFlags bit-OR), e.old_path on rename
    }
}

// Enumerate submodules
auto subs = ase::adp::libgit2::submodule::list(repo);
```

The wrapper surface is intentionally narrow — it covers what the explorer's scanner needs (open, head, status, submodule enumeration). Callers needing additional libgit2 features should add focused methods to the existing classes rather than leaking `git_repository*` through `native_handle()` to consumer translation units.

### Error handling

Every entry point returns `std::variant<T, Error>` where T is the success value. No exceptions cross the adapter boundary. Inspect with `std::get_if<Error>` / `std::get<T>`.

### Thread safety

libgit2 is thread-safe at the **handle** level — distinct `git_repository*` instances may be used concurrently from different threads without further synchronization. A single `Repository` instance must not be mutated from multiple threads at once. The explorer's scanner exploits this by giving each worker thread its own `Repository` per submodule, scanning in parallel without contention.

## Architecture & layer

`adapter/` is a top-level isolation layer that sits orthogonal to the `L0..L5` ECS stack. This adapter depends on system-installed `libgit2` (>= 1.0; tested with 1.9.x on Arch Linux and Debian Trixie). Found via `pkg-config`. The `adapter/ase-adp-libgit2/` path is whitelisted in `core/ase-validator/ecs_validator/data/third_party_oop.json` for the `EXTERN_C_FORBIDDEN` rule — consumers do **not** need their own whitelist entry.

## Build

Standalone (also builds the example):

```bash
cd adapter/ase-adp-libgit2
cmake -B build -G Ninja
ninja -C build
./build/ase-adp-libgit2-example /path/to/repo
```

Example output against the ASE root repo (145 submodules):

```
ase-adp-libgit2 v00.00.01.00001 [seed]
workdir : /mnt/code/src/ase
HEAD    : e330d52
status  : M=1 A=0 D=0 R=0 ?=27 !=0 ignored=0 dirty_total=28 entries=28
submods : 145 registered
        : 145 initialised in working tree
```

Wall clock: ~170 ms cold-scan single-repo + submodule enumeration on a warm filesystem cache. The full multi-repo scanner in `ase-client-explorer` parallelises across N hardware threads on top of this baseline.

As a transitive dep from the ASE root build: linked automatically when a consumer adds `ase::adp::libgit2` to its `target_link_libraries`. `libgit2` itself is a `PRIVATE` link dep of this adapter — `<git2.h>` is never exposed transitively.

## Companion adapter

For the concurrent map that the explorer's scanner publishes status updates into, see [ase-adp-libcuckoo](https://github.com/antarien/ase-adp-libcuckoo). The two adapters are designed to compose: libgit2 produces the per-file status; libcuckoo absorbs the point-updates from N writer threads at O(1) per write without a global lock.

## License

Source code in this adapter (the wrapper headers + .cpp files + CMake glue + example) is released under the AOW Developer License — see [`LIC_AOW_ADG_DE.md`](LIC_AOW_ADG_DE.md). The system-installed `libgit2` library remains under its own GPLv2-with-linking-exception terms, as documented at <https://libgit2.org/>.

Project participation is governed by the AOW ADG project agreements — see [`PJV_AOW_ADG_DE.md`](PJV_AOW_ADG_DE.md) (DE), [`PJV_AOW_ADG_EN.md`](PJV_AOW_ADG_EN.md) (EN), [`PJV_AOW_ADG_PT.md`](PJV_AOW_ADG_PT.md) (PT).
