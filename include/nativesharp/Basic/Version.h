#pragma once

// CC1-000 "build identity". `kCompilerBuildId` lives in a GENERATED header that CMake rewrites on
// every configure -- see BuildId.h.in for why. It is included from here so that every existing
// consumer of Version.h gets the build id without a second include, and so that "which compiler is
// this" stays one concept with one place to look.
#include "nativesharp/Basic/BuildId.h"

#include <string_view>

namespace nativesharp
{
// RELEASE identity. Deliberately stable across snapshots -- NativeSharp 0.1 is still 0.1 -- and
// therefore NOT the thing that distinguishes two builds. That is kCompilerBuildId.
inline constexpr std::string_view kLanguageVersion = "0.1";
inline constexpr std::string_view kCompilerVersion = "0.1.0-dev55";
inline constexpr std::string_view kCompilerStage = "57";
}
