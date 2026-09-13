#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/CompilationProfile.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
nativesharp::ExitCode run(
    const std::vector<std::string_view>& arguments,
    std::ostringstream& output,
    std::ostringstream& error)
{
    return nativesharp::driver::Driver{}.run(arguments, output, error);
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::driver::CompilationProfile;
    using nativesharp::driver::resolveCompilationProfile;
    using nativesharp::llvm_backend::DebugInformationKind;
    using nativesharp::llvm_backend::LlvmOptimizationLevel;
    nativesharp::tests::TestContext tests;

    tests.expect(nativesharp::driver::parseCompilationProfile("debug") == CompilationProfile::Debug &&
            nativesharp::driver::parseCompilationProfile("release") == CompilationProfile::Release &&
            !nativesharp::driver::parseCompilationProfile("fast").has_value(),
        "only the canonical debug and release profile names are accepted");

    const auto debug = resolveCompilationProfile(CompilationProfile::Debug, std::nullopt, std::nullopt);
    tests.expect(debug.optimizationLevel == LlvmOptimizationLevel::O0 &&
            debug.debugInformation == DebugInformationKind::Full,
        "debug profile resolves to O0 with full debug information");
    const auto release = resolveCompilationProfile(CompilationProfile::Release, std::nullopt, std::nullopt);
    tests.expect(release.optimizationLevel == LlvmOptimizationLevel::O2 &&
            release.debugInformation == DebugInformationKind::LineTablesOnly,
        "release profile resolves to O2 with line tables");
    const auto overridden = resolveCompilationProfile(
        CompilationProfile::Debug, LlvmOptimizationLevel::O3, DebugInformationKind::None);
    tests.expect(overridden.optimizationLevel == LlvmOptimizationLevel::O3 &&
            overridden.debugInformation == DebugInformationKind::None,
        "explicit optimization and debug selections override profile defaults");

    {
        std::ostringstream output;
        std::ostringstream error;
        tests.expect(run({}, output, error) == ExitCode::Success &&
                output.str().find("--profile=debug|release") != std::string::npos,
            "help documents the two compilation profiles");
    }
    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {
            "--emit-project-oop-obj", "input.nsproj", "-o", "output.obj", "--profile=fast"};
        tests.expect(run(arguments, output, error) == ExitCode::CommandLineError &&
                error.str().find("expected --profile=debug or --profile=release") != std::string::npos,
            "unknown profile is rejected before compilation");
    }
    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {
            "--emit-project-oop-obj", "input.nsproj", "-o", "output.obj",
            "--profile=debug", "--profile=release"};
        tests.expect(run(arguments, output, error) == ExitCode::CommandLineError &&
                error.str().find("duplicate compilation profile") != std::string::npos,
            "duplicate profile is rejected deterministically");
    }
    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {"--version", "--profile=debug"};
        tests.expect(run(arguments, output, error) == ExitCode::CommandLineError &&
                error.str().find("supported when emitting a project OOP") != std::string::npos,
            "profile cannot silently attach to an unsupported action");
    }

    const std::filesystem::path project =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage51/Stage51String.nsproj";
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp-compilation-profile-tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const auto compileProfile = [&](std::string_view profile, std::string_view expectedOptimization,
                                    std::string_view expectedDebug, const std::filesystem::path& object) {
        const std::string projectText = project.string();
        const std::string objectText = object.string();
        const std::string profileOption = "--profile=" + std::string(profile);
        const std::vector<std::string_view> arguments = {
            "--emit-project-oop-obj", projectText, "-o", objectText, profileOption};
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode result = run(arguments, output, error);
        tests.expect(result == ExitCode::Success && std::filesystem::is_regular_file(object) &&
                output.str().find("compilation-profile: " + std::string(profile)) != std::string::npos &&
                output.str().find("optimization: " + std::string(expectedOptimization)) != std::string::npos &&
                output.str().find("debug-information: " + std::string(expectedDebug)) != std::string::npos,
            std::string(profile) + " profile reaches real OOP object emission with its resolved policy (" +
                error.str() + ")");
    };
    compileProfile("debug", "O0", "full", root / "debug.obj");
    compileProfile("release", "O2", "line-tables-only", root / "release.obj");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
