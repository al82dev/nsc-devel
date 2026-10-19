#pragma once

#include "nativesharp/Compilation/SdkManifest.h"

#include <filesystem>
#include <optional>
#include <string>

namespace nativesharp::compilation
{
class SdkManifestLoader final
{
public:
    std::optional<SdkManifest> load(
        const std::filesystem::path& manifestPath,
        std::string& error) const;
};
}
