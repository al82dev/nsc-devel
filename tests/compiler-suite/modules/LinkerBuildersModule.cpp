#include "LinkerTestSupport.h"

#include "nativesharp/Linker/WindowsLibraryBuilder.h"
#include "nativesharp/Linker/WindowsRuntimeLibrary.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace linker_support;
using namespace nativesharp::linker;

/** Confirms that a fresh library builder publishes neutral, non-success state. */
void expectNeutralLibraryState(TestContext& context, const WindowsLibraryBuilder& builder)
{
    context.expect(builder.archiveInfo().memberCount == 0, "fresh builder must not publish archive members");
    context.expect(builder.archiveInfo().symbolNames.empty(), "fresh builder must not publish archive symbols");
    context.expect(builder.dynamicLibraryInfo().sectionCount == 0,
        "fresh builder must not publish a dynamic-library image");
    context.expect(!builder.staticArchiveCacheAttempted(), "fresh builder must not report an archive cache attempt");
    context.expect(!builder.reusedStaticArchive(), "fresh builder must not report archive reuse");
    context.expect(!builder.runtimeObjectCacheAttempted(), "fresh builder must not report a runtime cache attempt");
    context.expect(!builder.reusedRuntimeObject(), "fresh builder must not report runtime-object reuse");
}

/** Exercises only CF-LINKER-034 variants with complete public, process-free oracles. */
void libraryConfigurationContract(TestContext& context, std::size_t variant)
{
    WindowsLibraryBuilder builder;
    if (variant == 1)
    {
        expectNeutralLibraryState(context, builder);
        return;
    }

    std::string error;
    const std::span<const std::filesystem::path> noObjects;
    if (variant == 3 || variant == 5)
    {
        builder.setExternalImports({});
        builder.setExtraLinkLibraries({});
        builder.setRuntimeLibrary({});
        builder.setProgramObjectsVerified(variant == 5);
        const bool built = builder.buildStaticObjects(
            noObjects, {}, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, error);
        context.expect(!built, "empty static-library inputs must be rejected before any external producer");
        context.expect(error == "partitioned static library has no program objects",
            "empty static-library inputs must retain the exact producer-boundary diagnostic");
        expectNeutralLibraryState(context, builder);
        return;
    }
    if (variant == 4)
    {
        const std::span<const std::string> noExports;
        const bool built = builder.buildDynamicObjects(
            noObjects,
            {},
            noExports,
            {},
            {},
            nativesharp::llvm_backend::LlvmOptimizationLevel::O0,
            nativesharp::llvm_backend::DebugInformationKind::None,
            error);
        context.expect(!built, "empty DLL inputs must be rejected before any external producer");
        context.expect(error == "partitioned DLL has no program objects",
            "empty DLL inputs must retain the exact producer-boundary diagnostic");
        expectNeutralLibraryState(context, builder);
        return;
    }
    if (variant == 7)
    {
        const bool first = builder.buildStaticObjects(
            noObjects, {}, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, error);
        const std::string firstError = error;
        const bool second = builder.buildStaticObjects(
            noObjects, {}, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, error);
        context.expect(!first && !second, "repeated missing-input builds must reject deterministically");
        context.expect(firstError == error, "repeated missing-input builds must retain the same diagnostic");
        expectNeutralLibraryState(context, builder);
        return;
    }

    for (std::size_t index = 0; index < 4096; ++index)
    {
        WindowsLibraryBuilder sample;
        context.expect(!sample.staticArchiveCacheAttempted() && !sample.reusedStaticArchive() &&
                !sample.runtimeObjectCacheAttempted() && !sample.reusedRuntimeObject(),
            "large fresh-builder populations must retain neutral telemetry");
    }
}

/** Creates a collision-resistant test root without recursively searching the filesystem. */
std::filesystem::path temporaryRoot(std::size_t variant)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
        ("nsc-host-linker-runtime-" + std::to_string(stamp) + "-" + std::to_string(variant));
}

/** Exercises the pure and filesystem-local parts of CF-LINKER-037. */
void runtimeResolutionContract(TestContext& context, std::size_t variant)
{
    const std::filesystem::path root = temporaryRoot(variant);
    const std::filesystem::path executable = root / "bin" / "nsc.exe";
    const std::filesystem::path adjacent = root / "lib" / "nativesharp" / "runtime" /
        std::string(kWindowsRuntimeLibraryFileName);
    const std::filesystem::path configured = root / "configured" / "runtime.lib";

    if (variant == 1)
    {
        context.expect(resolveWindowsRuntimeLibrary({}, configured) == configured,
            "an absent executable path must preserve the configured default");
        return;
    }
    if (variant == 2)
    {
        std::error_code error;
        std::filesystem::create_directories(adjacent.parent_path(), error);
        context.expect(!error, "representative runtime fixture directory must be creatable");
        if (!error)
        {
            std::ofstream output(adjacent, std::ios::binary | std::ios::trunc);
            output.put('\0');
            output.close();
            context.expect(resolveWindowsRuntimeLibrary(executable, configured) == adjacent,
                "an existing adjacent runtime must take precedence over the configured default");
        }
        std::filesystem::remove_all(root, error);
        return;
    }
    if (variant == 3)
    {
        context.expect(resolveWindowsRuntimeLibrary(executable, {}) == adjacent,
            "without a configured default the canonical adjacent candidate must remain observable");
        return;
    }
    if (variant == 4)
    {
        const std::filesystem::path quotedExecutable = root / "bad\"name" / "nsc.exe";
        context.expect(resolveWindowsRuntimeLibrary(quotedExecutable, configured) == configured,
            "the pure resolver must fall back instead of inventing path-validation behavior");
        return;
    }
    if (variant == 5)
    {
        context.expect(resolveWindowsRuntimeLibrary({}, {}).empty(),
            "empty executable and default paths must yield the documented empty result");
        return;
    }
    if (variant == 7)
    {
        const auto first = resolveWindowsRuntimeLibrary(executable, configured);
        const auto second = resolveWindowsRuntimeLibrary(executable, configured);
        context.expect(first == configured && second == first,
            "runtime resolution must be deterministic for unchanged filesystem state");
        return;
    }

    bool stable = true;
    for (std::size_t index = 0; index < 4096; ++index)
    {
        const std::filesystem::path candidate = root / std::to_string(index) / "nsc.exe";
        stable = stable && resolveWindowsRuntimeLibrary(candidate, configured) == configured;
    }
    context.expect(stable, "large independent resolution sets must preserve the configured fallback");
}
}

/** Materializes builder, runtime and publication contracts with explicit case dispositions. */
void registerLinkerBuildersModule(TestRegistry& registry)
{
    using namespace linker_support;
    addMixedContract(
        registry,
        34,
        {1, 3, 4, 5, 7, 8},
        libraryConfigurationContract);
    addMixedContract(
        registry,
        37,
        {1, 2, 3, 4, 5, 7, 8},
        runtimeResolutionContract);
}
}
