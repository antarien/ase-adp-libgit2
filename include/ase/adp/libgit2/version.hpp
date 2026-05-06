#pragma once

/**
 * @file        version.hpp
 * @brief       Version constants for ase-adp-libgit2
 * @description Pure-constexpr identity tag for the adapter, mirroring
 *              the VERSION SSOT at the repo root.
 *
 * @module      ase-adp-libgit2
 * @layer       adapter
 */

#include <cstdint>

namespace ase::adp::libgit2 {

constexpr uint8_t  VERSION_MAJOR = 0;
constexpr uint8_t  VERSION_MINOR = 0;
constexpr uint8_t  VERSION_PATCH = 1;
constexpr uint32_t VERSION_BUILD = 1;

constexpr const char* VERSION_STRING = "00.00.01.00001";
constexpr const char* VERSION_STATUS = "seed";

}  // namespace ase::adp::libgit2
