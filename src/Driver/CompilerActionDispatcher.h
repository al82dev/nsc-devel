#pragma once

#include "CompilerInvocation.h"

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Driver/CompilationJobPolicy.h"
#include "nativesharp/Driver/Driver.h"

#include <iosfwd>
#include <optional>
#include <span>
#include <string_view>

namespace nativesharp::driver
{
class CompilerActionDispatcher final
{
public:
    CompilerActionDispatcher(
        const Driver& driver,
        std::span<const std::string_view> arguments,
        const CompilerInvocation& invocation,
        const CompilationJobPolicy& jobPolicy,
        const compilation::ProjectLoadOptions& projectLoadOptions,
        std::string_view compilerStage,
        std::ostream& output,
        std::ostream& error) noexcept;

    ExitCode dispatch() const;
    static void showHelp(std::ostream& output);

private:
    using Result = std::optional<ExitCode>;

    Result dispatchSingleArgument() const;
    Result dispatchCompilerQuery(std::string_view action) const;
    Result dispatchLanguageQuery(std::string_view action) const;
    Result dispatchPolicyQuery(std::string_view action) const;
    Result dispatchSingleUtility(std::string_view action) const;
    Result dispatchTwoArguments() const;
    Result dispatchMigrationAnalysis(std::string_view action) const;
    Result dispatchRuntimeAndNumeric(std::string_view action) const;
    Result dispatchPathOperation(std::string_view action) const;
    Result dispatchPeAndCompatibility(std::string_view action) const;
    Result dispatchLibraryAndFrontend(std::string_view action) const;
    Result dispatchThreeArguments() const;
    Result dispatchDeprecationSimulation() const;
    Result dispatchMigrationCompatibilityMatrix() const;
    Result dispatchInstallationComparison() const;
    Result dispatchDiagnosticBundle() const;
    Result dispatchFourArguments() const;
    Result dispatchFiveArguments() const;
    Result dispatchEmission() const;
    ExitCode invalidCommandLine() const;

    const Driver& driver_;
    std::span<const std::string_view> arguments_;
    const CompilerInvocation& invocation_;
    const CompilationJobPolicy& jobPolicy_;
    const compilation::ProjectLoadOptions& projectLoadOptions_;
    std::string_view compilerStage_;
    std::ostream& output_;
    std::ostream& error_;
};
}
