#include "nativesharp/Compilation/PrecompiledProductSidecarPath.h"

#include <system_error>

namespace nativesharp::compilation
{
std::filesystem::path precompiledProductSidecarPath(
    const std::filesystem::path& metadataPath)
{
    std::filesystem::path path = metadataPath;
    path.replace_extension(".nsproducts");
    return path;
}

std::filesystem::path availablePrecompiledProductSidecarPath(
    const std::filesystem::path& metadataPath)
{
    const std::filesystem::path candidate =
        precompiledProductSidecarPath(metadataPath);
    std::error_code error;
    if (!std::filesystem::is_regular_file(candidate, error) || error) return {};
    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(candidate, error);
    return error ? std::filesystem::path{} : canonical;
}
}
