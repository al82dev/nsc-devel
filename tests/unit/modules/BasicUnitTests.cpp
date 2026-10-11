#include "nsc_test/Modules.h"

#include "nsc_test/TestContext.h"
#include "nsc_test/TestRegistry.h"
#include "nativesharp/Basic/Version.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace nsc::test
{
namespace
{
void verifyBuildIdentity(TestContext& context)
{
    const std::string_view buildId = nativesharp::kCompilerBuildId;
    context.expect(!buildId.empty(), "build id is not empty");
    context.expect(
        buildId != "@NSC_BUILD_ID@",
        "build id was substituted by CMake, not left as the template placeholder");
    context.expect(
        buildId.size() == 16,
        "build id is 16 characters (" + std::to_string(buildId.size()) + ")");
    context.expect(
        std::all_of(
            buildId.begin(),
            buildId.end(),
            [](char character) {
                return (character >= '0' && character <= '9') ||
                       (character >= 'a' && character <= 'f');
            }),
        "build id is lowercase hexadecimal");
    context.expect(
        nativesharp::kCompilerReleaseVersion == "3.0.0",
        "compiler compatibility identity is the 3.0.0 development line");
    context.expect(
        nativesharp::kCompilerVersion.starts_with("3.0.0+build."),
        "visible compiler version identifies the development line and build metadata");
    context.expect(
        nativesharp::kCompilerVersion.ends_with(buildId),
        "visible compiler version contains the exact build id");
    context.expect(nativesharp::kLanguageVersion == "3.0.0", "language version is 3.0.0");
    context.expect(
        nativesharp::kCompilerStage == "70",
        "compiler stage remains traceable during 3.0 development");
}
}

void registerBasicUnitTests(TestRegistry& registry)
{
    registry.add(TestCase{
        "nsc.version.identity",
        "basic",
        "compiler build identity",
        {"identity", "version"},
        verifyBuildIdentity,
    });
}
}
