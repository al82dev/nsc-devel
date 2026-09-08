#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::driver {
inline constexpr unsigned kInstallationFileListFormatVersion = 1;
inline constexpr unsigned kInstallationManifestFormatVersion = 1;

bool writeInstallationManifest(const std::filesystem::path &prefix,
                               const std::filesystem::path &expectedFiles,
                               const std::filesystem::path &destination,
                               std::string &error);

bool verifyInstallationManifest(const std::filesystem::path &prefix,
                                const std::filesystem::path &manifest,
                                std::string &error);
} // namespace nativesharp::driver
