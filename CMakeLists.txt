cmake_minimum_required(VERSION 3.22)

#[[
ASE LIBGIT2 ADAPTER BUILD CONFIGURATION

@file        CMakeLists.txt
@brief       Build configuration for ase-adp-libgit2
@description RAII C++20 wrapper around libgit2 (system library). Wraps
             git_repository*, git_status_list*, git_submodule*, and
             git_index* in move-only handle classes that own libgit2
             resources, plus a single global init/shutdown helper. The
             extern "C" surface of <git2.h> is confined to the adapter's
             implementation translation units; consumers include only
             ase/adp/libgit2/*.hpp and never touch libgit2's C types.

             Why this adapter exists:
               - libgit2 is a C library; its <git2.h> is a giant
                 extern "C" surface that the ASE validator forbids in
                 regular project code (EXTERN_C_FORBIDDEN rule).
               - Repository / StatusList / Submodule / Index are all
                 owning C handles; manual git_*_free() pairs are easy
                 to leak across error paths. RAII move-only wrappers
                 make the lifetimes explicit and exception-safe.
               - Status-scan throughput in clients/ase-client-explorer
                 hinges on calling libgit2 in-process (no fork+exec per
                 repo). This adapter is the supported entry point for
                 that path; spawning the `git` CLI is forbidden.

             Whitelist — GEMESSEN 2026-08-31, UND DIE LAGE IST NICHT DIE HIER
             FRUEHER BEHAUPTETE:
               Dieser Block sagte, adapter/ase-adp-libgit2/ sei in
               core/ase-validator/ecs_validator/data/third_party_oop.json fuer
               EXTERN_C_FORBIDDEN freigestellt. ES IST NICHT DARIN. Der Bestand
               fuehrt genau drei Pfade — ase-adp-microtex/, ase-adp-gtk/ und
               ase-adp-treesitter/ —, libgit2 ist keiner davon.

               ES FEHLT DERZEIT AUCH NICHTS, und genau deshalb ist der Satz
               gefaehrlich statt bloss falsch: dieser Adapter traegt in KEINER
               .hpp/.cpp ein `extern "C"` — es kommt ausschliesslich aus
               <git2.h> und bleibt hinter den Wrappern. Die Regel hat hier also
               nie gefeuert, und niemand hatte je einen Anlass, die Behauptung
               nachzuschlagen. Zum Vergleich, im selben Zug gemessen:
               ase-adp-treesitter traegt `extern "C"` in echtem Code
               (include/ase/adp/treesitter/grammars.hpp) — und steht deshalb
               zu Recht im Bestand.

               WER HIER `extern "C"` IN DEN CODE SCHREIBT, hat damit KEINE
               Freistellung im Ruecken, auch wenn dieser Block das jahrelang
               versprochen hat. Ob eine noetig wird, entscheidet dann der
               Validator-Betreuer; ein Eintrag im Bestand ist eine WEITUNG und
               wird nicht nebenbei von einer Modul-Session vorgenommen.

@module      ase-adp-libgit2
@layer       adapter (3rd-party isolation, consumed by L1+ code)
@created     2026-05-06
@modified    2026-05-06
@version     00.00.01.00001 [seed]
]]

# Parse local VERSION (SSOT) if not building from root
if(NOT DEFINED ASE_ADP_LIBGIT2_VERSION)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
        message(FATAL_ERROR "VERSION file not found: ${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    endif()
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _VERSION_LINES)
    foreach(_LINE ${_VERSION_LINES})
        if(_LINE MATCHES "^ASE_ADP_LIBGIT2=(.*)$")
            set(ASE_ADP_LIBGIT2_VERSION "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(NOT DEFINED ASE_ADP_LIBGIT2_VERSION)
        message(FATAL_ERROR "ASE_ADP_LIBGIT2 version not found in VERSION file")
    endif()
endif()

project(ase-adp-libgit2 VERSION ${ASE_ADP_LIBGIT2_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

message(STATUS "Configuring ${PROJECT_NAME} v${PROJECT_VERSION}")

# ── System dependency: libgit2 ──────────────────────────────────────
# Discovered via pkg-config so the adapter picks up the installed
# headers + linker flags from the system. Tested against libgit2 1.9.x
# (Arch Linux / Debian Trixie). Earlier versions back to 1.0 should
# also work; the calls used here are part of the long-stable subset.
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED IMPORTED_TARGET libgit2)

# ── Adapter library ─────────────────────────────────────────────────
add_library(${PROJECT_NAME} STATIC
    src/init.cpp
    src/error.cpp
    src/repository.cpp
    src/status.cpp
    src/submodule.cpp

    include/ase/adp/libgit2/init.hpp
    include/ase/adp/libgit2/error.hpp
    include/ase/adp/libgit2/repository.hpp
    include/ase/adp/libgit2/status.hpp
    include/ase/adp/libgit2/submodule.hpp
    include/ase/adp/libgit2/version.hpp
)
add_library(ase::adp::libgit2 ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# ── ase::types (Layer 0) — carries the ok-or-error return type ──────
# WRFL_ASE_STD_FORBIDDEN bans std::variant tree-wide; the adapter's
# open()/scan()/list() return ase::types::Result<T, Error> instead. The
# type appears in PUBLIC signatures, so the link must be PUBLIC too —
# consumers need the header to name the return value.
#
# ase-types is a header-only INTERFACE target on Layer 0. A dependency
# downward to L0 is allowed from every layer, and it adds no build step:
# the adapter stays standalone-buildable. When this CMakeLists is not the
# top (the explorer or a tier pulls it in), the target already exists.
if(NOT TARGET ase::types)
    add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/../../foundation/ase-types
        ${CMAKE_CURRENT_BINARY_DIR}/_ase-types
    )
endif()

# libgit2 itself is a PRIVATE link dep — consumers never see <git2.h>.
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ase::types
    PRIVATE
        PkgConfig::LIBGIT2
)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ase
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# ── Standalone example (only built when this CMakeLists is the top) ──
#
# The example writes through ase-log (WRFL_ASE_LOGGING_COMPLIANCE bans the
# direct stdout call), so it links ase::log on top of the adapter. That is
# an L1 dependency of the EXAMPLE, not of the adapter: the executable is a
# consumer, and consumers may use the layers above this one. The adapter
# target itself stays clean — it links only ase::types and libgit2.
#
# ase-log resolves its own generated header via ASE_ROOT when it is not
# built from the repository root, so the standalone path needs that set.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT TARGET ase::log)
        set(ASE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH
            "Repository root — ase-log locates its generated category header from here")
        add_subdirectory(
            ${CMAKE_CURRENT_SOURCE_DIR}/../../core/ase-log
            ${CMAKE_CURRENT_BINARY_DIR}/_ase-log
        )
    endif()

    add_executable(ase-adp-libgit2-example example/example_main.cpp)
    target_link_libraries(ase-adp-libgit2-example
        PRIVATE
            ${PROJECT_NAME}
            ase::log
    )
endif()
