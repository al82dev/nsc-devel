#pragma once

#include <string>

namespace nativesharp::driver
{
struct BuildPreferences final
{
    std::string configuration = "Debug";
    std::string optimization = "O0";
    std::string debugInformation = "full";
    std::string linker = "NSLink";
    std::string target = "x86_64-pc-windows-msvc";
    bool restoreBeforeBuild = true;
};
} // namespace nativesharp::driver
