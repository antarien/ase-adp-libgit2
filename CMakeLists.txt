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

             Whitelist:
               adapter/ase-adp-libgit2/ is whitelisted in
               core/ase-validator/ecs_validator/data/third_party_oop.json
               for the EXTERN_C_FORBIDDEN rule (consumers do not need
               their own whitelist entry).

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

# libgit2 itself is a PRIVATE link dep — consumers never see <git2.h>.
target_link_libraries(${PROJECT_NAME}
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
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_executable(ase-adp-libgit2-example example/example_main.cpp)
    target_link_libraries(ase-adp-libgit2-example PRIVATE ${PROJECT_NAME})
endif()
