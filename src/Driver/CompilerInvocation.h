#pragma once

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Driver/CompilationProfile.h"
#include "nativesharp/Driver/CompilationTelemetry.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"
#include "nativesharp/LLVM/FloatingPointMode.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/Linker/LldLinker.h"

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::driver
{
// Normalized, validated state for one compiler-driver invocation. The object owns only values
// derived from the command line and root project settings; dispatch and execution remain in Driver.
struct CompilerInvocation final
{
    std::vector<std::string_view> arguments;
    std::string projectConfiguredOutput;

    llvm_backend::LlvmOptimizationLevel optimizationLevel =
        llvm_backend::LlvmOptimizationLevel::O0;
    std::optional<CompilationProfile> compilationProfile;
    bool optimizationLevelSeen = false;
    bool compilationProfileSeen = false;

    llvm_backend::DebugInformationKind debugInformation =
        llvm_backend::DebugInformationKind::None;
    std::vector<llvm_backend::DebugPrefixMap> debugPrefixMaps;
    bool debugInformationSeen = false;

    llvm_backend::LinkTimeOptimizationMode linkTimeOptimization =
        llvm_backend::LinkTimeOptimizationMode::None;
    bool linkTimeOptimizationSeen = false;
    bool incrementalLink = false;
    // EN: Freeze 3.0 DEC-13: opt-in check of `extern C from "<dll>"` symbols against host DLLs.
    // ES: Congelamiento 3.0 DEC-13: comprobacion opcional de simbolos `extern C from "<dll>"` contra
    //     las DLL del host.
    bool validateSystemImports = false;
    linker::WindowsExecutableSubsystem subsystem =
        linker::WindowsExecutableSubsystem::Console;
    bool subsystemSeen = false;
    std::vector<std::filesystem::path> extraLinkLibraries;

    llvm_backend::FloatingPointMode floatingPointMode =
        llvm_backend::FloatingPointMode::Precise;
    llvm_backend::FmaContractPolicy fmaContractPolicy =
        llvm_backend::FmaContractPolicy::Off;
    llvm_backend::FloatingPointRoundingMode floatingPointRoundingMode =
        llvm_backend::FloatingPointRoundingMode::Dynamic;
    llvm_backend::FloatingPointDenormalMode floatingPointDenormalMode =
        llvm_backend::FloatingPointDenormalMode::Preserve;
    bool floatingPointModeSeen = false;
    bool fmaContractPolicySeen = false;
    bool floatingPointRoundingModeSeen = false;
    bool floatingPointDenormalModeSeen = false;

    compilation::ProjectLoadOptions projectLoadOptions;
    std::optional<std::size_t> requestedJobs;
    std::optional<std::size_t> reservedCores;
    std::optional<std::uint64_t> timeoutMilliseconds;
    std::optional<std::uint64_t> optimizationBudgetMilliseconds;
    bool optimizationBudgetSeen = false;
    bool jobCountSeen = false;
    bool reservedCoresSeen = false;
    bool libraryReferenceRootSeen = false;
    bool genericInstantiationDepthSeen = false;
    bool genericInstantiationCountSeen = false;
#if 0 // INSTRUMENTATION CODE: parser state for parked generics breadcrumbs.
    bool genericsTraceSeen = false;
#endif // INSTRUMENTATION CODE

    bool reportTiming = false;
    bool reportProgress = false;
    ProgressFormat progressFormat = ProgressFormat::Text;

    bool artifactCacheSeen = false;
    bool artifactCacheDisabled = false;
    bool artifactCacheMaximumSeen = false;
    bool defaultArtifactCache = false;
    std::optional<std::filesystem::path> artifactCacheRoot;
    std::uintmax_t artifactCacheMaximumBytes = 0;
};

#if 0 // INSTRUMENTATION CODE: exact trace-opening hook, intentionally disabled.
ExitCode openCompilerGenericsTrace(
    const std::filesystem::path& path,
    std::string_view displayPath,
    bool automatic,
    std::ostream& error);
#endif // INSTRUMENTATION CODE

// Parses options and applies root-project defaults without dispatching an action. Diagnostics and
// exit codes deliberately retain the existing Driver::run contract.
ExitCode parseCompilerInvocation(
    std::span<const std::string_view> arguments,
    compilation::ProjectLoader& projectLoader,
    CompilerInvocation& invocation,
    std::ostream& error);

// Internal linear argv pass. Project/profile defaults remain in parseCompilerInvocation so this
// operation owns only option recognition and normalized positional arguments.
ExitCode parseCompilerOptions(
    std::span<const std::string_view> arguments,
    CompilerInvocation& invocation,
    std::ostream& error);

// Validates option/action combinations after Driver installs and reports the floating-point
// contract, preserving the historical ordering of observable output.
ExitCode validateCompilerInvocation(
    CompilerInvocation& invocation,
    std::ostream& error);

// Link-only validation deliberately remains a second step: the established driver contract
// prunes/reports the artifact cache before rejecting an unsupported LTO or incremental-link mode.
ExitCode validateCompilerLinkOptions(
    const CompilerInvocation& invocation,
    std::ostream& error);
}
