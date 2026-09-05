#pragma once

#include <string_view>

namespace nativesharp
{
// CC1-000 "build identity" -- GENERATED. The source of this file is
// include/nativesharp/Basic/BuildId.h.in; CMake regenerates it on every configure. Do not edit the
// generated copy, and do not replace this with a hand-maintained constant: a constant somebody has
// to remember to bump is exactly the failure being fixed here.
//
// WHY IT EXISTS. `kCompilerVersion` is the RELEASE identity and deliberately does not move between
// snapshots -- NativeSharp 0.1 is still 0.1. But a consumer holding two different nsc.exe builds
// needs to tell them apart, and until this existed it could not: every build of every stage
// reported `0.1.0-dev55`. On 2026-08-31 a binding generator that had just been handed a fresh
// compiler read the previous one's capabilities and reported them as current, and its workaround
// was to hash the binary itself.
//
// The value is the newest source mtime in the tree plus the configure timestamp, hashed. Two
// builds of different source are guaranteed to differ. Two builds of the SAME source from separate
// configures also differ -- that is the conservative direction on purpose: claiming two binaries
// are identical when they are not is the failure this prevents, and claiming they differ when they
// do not costs nothing.
inline constexpr std::string_view kCompilerBuildId = "1cfbe2e87a09bf67";
}
