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
    // none, full or thin; forwarded to the compiler as -flto=<mode> (optimization part 2, item 41).
    // none, full o thin; se reenvia al compilador como -flto=<modo> (parte 2, punto 41).
    std::string linkTimeOptimization = "none";
    std::string target = "x86_64-pc-windows-msvc";
    bool restoreBeforeBuild = true;
};
} // namespace nativesharp::driver
