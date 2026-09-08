#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::driver {
inline constexpr unsigned kToolchainContractFormatVersion = 1;

// Returns the canonical, deterministic description of the toolchain accepted by
// this compiler. Deliberately excludes the per-build id: compatibility and
// reproducibility are different from identifying one particular invocation of
// the build system.
std::string currentToolchainContract();

bool writeToolchainContract(const std::filesystem::path &destination,
                            std::string &error);

bool verifyToolchainContract(const std::filesystem::path &source,
                             std::string &error);
} // namespace nativesharp::driver
