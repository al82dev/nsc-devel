#pragma once

#include <filesystem>

namespace nativesharp::compilation
{
std::filesystem::path precompiledProductSidecarPath(
    const std::filesystem::path& metadataPath);

std::filesystem::path availablePrecompiledProductSidecarPath(
    const std::filesystem::path& metadataPath);
}
