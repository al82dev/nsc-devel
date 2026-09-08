#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::driver {
// Verifies the complete minimum layout delivered by the supported NativeSharp
// installation. File contents owned by the compiler contract are checked
// through ToolchainContract; this layer proves that the package itself did not
// omit one of its required components.
bool verifyInstallation(const std::filesystem::path &prefix,
                        std::string &error);
} // namespace nativesharp::driver
