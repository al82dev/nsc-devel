#pragma once

#include <string_view>

namespace nativesharp
{
namespace detail
{
// CC1-000 "build identity" -- GENERATED. The source of this file is
// include/nativesharp/Basic/BuildId.h.in; CMake regenerates it before every construction de
// nsc_driver. Do not edit the generated copy, and do not replace this with a hand-maintained
// constant: a constant somebody has to remember to bump is exactly the failure being fixed here.
//
// WHY IT EXISTS. `kCompilerReleaseVersion` is the compatibility identity and deliberately does
// not move between builds. `kCompilerVersion` appends this id as SemVer build metadata so the
// version string of two nsc.exe binaries is never the same.
//
inline constexpr std::string_view kGeneratedCompilerBuildId = "03ca780e6debcd15";
inline constexpr std::string_view kGeneratedCompilerVersion = "0.9.0+build.20260907T042535Z.03ca780e6debcd15";
}
}
