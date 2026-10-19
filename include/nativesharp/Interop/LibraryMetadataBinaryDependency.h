#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace nativesharp::interop
{
struct LibraryMetadataBinaryDependency final
{
    std::string moduleName;
    std::string libraryFileName;
    std::string metadataFileName;
    std::string runtimeFileName;
    std::array<std::uint8_t, 32> abiFingerprint{};
};
}
