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
inline constexpr std::string_view kGeneratedCompilerBuildId = "98c09f3020efd7e0";
inline constexpr std::string_view kGeneratedCompilerVersion = "0.8.0-alpha+build.20260906T081447Z.98c09f3020efd7e0";
}
}
