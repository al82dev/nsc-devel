#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::compilation
{
struct SdkLibraryDependency final
{
    std::string name;
    std::filesystem::path relativePath;
};
}
