#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace nativesharp::compilation
{
inline constexpr std::uint32_t kSdkManifestFormatVersion = 1;

struct SdkManifest final
{
    std::filesystem::path manifestPath;
    std::string name;
    std::uint32_t formatVersion = kSdkManifestFormatVersion;
};

class SdkManifestLoader final
{
public:
    std::optional<SdkManifest> load(
        const std::filesystem::path& manifestPath,
        std::string& error) const;
};
}
