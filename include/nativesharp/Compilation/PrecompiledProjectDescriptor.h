#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::compilation
{
struct PrecompiledProjectDescriptor final
{
    std::filesystem::path ownerProjectFile;
    std::filesystem::path metadataPath;
    std::string moduleName;
};
}
