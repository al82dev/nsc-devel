#include "CompilerInvocation.h"
#include "Internal/InvocationCapabilities.h"

#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Driver/CompilerConsole.h"
#include "nativesharp/LLVM/CompilationArtifactCache.h"

#include <cstdlib>
#include <ostream>

namespace nativesharp::driver
{
namespace
{
bool isProjectBuildAction(std::span<const std::string_view> arguments) noexcept
{
    return arguments.size() >= 2U &&
        (arguments[0] == "--emit-project-oop-obj" ||
         arguments[0] == "--emit-project-oop-exe" ||
         arguments[0] == "--emit-project-oop-lib" ||
         arguments[0] == "--emit-project-oop-dll");
}

bool isProjectSettingsAction(std::span<const std::string_view> arguments)
{
    return arguments.size() >= 2U &&
        std::filesystem::path(std::string(arguments[1])).extension() == ".nsproj";
}

void applyProjectOptimization(
    const compilation::ProjectBuildSettings& settings,
    CompilerInvocation& invocation)
{
    if (invocation.optimizationLevelSeen || invocation.compilationProfileSeen ||
        !settings.optimizationLevel.has_value())
        return;
    switch (*settings.optimizationLevel)
    {
        case 0U:
            invocation.optimizationLevel = llvm_backend::LlvmOptimizationLevel::O0;
            break;
        case 1U:
            invocation.optimizationLevel = llvm_backend::LlvmOptimizationLevel::O1;
            break;
        case 2U:
            invocation.optimizationLevel = llvm_backend::LlvmOptimizationLevel::O2;
            break;
        case 3U:
            invocation.optimizationLevel = llvm_backend::LlvmOptimizationLevel::O3;
            break;
        default: break;
    }
    invocation.optimizationLevelSeen = true;
}

void applyEnvironmentLibraryRoot(CompilerInvocation& invocation)
{
    if (invocation.libraryReferenceRootSeen ||
        invocation.projectLoadOptions.libraryReferenceRoot.has_value())
        return;
    char* environmentRoot = nullptr;
    std::size_t environmentRootLength = 0;
    if (_dupenv_s(&environmentRoot, &environmentRootLength, "NS_LIB_ROOT") == 0 &&
        environmentRoot != nullptr && *environmentRoot != '\0')
    {
        const std::filesystem::path root{environmentRoot};
        if (root.is_absolute())
            invocation.projectLoadOptions.libraryReferenceFallbackRoot =
                root.lexically_normal();
    }
    std::free(environmentRoot);
}

void applyProjectSettings(
    const compilation::ProjectBuildSettings& settings,
    CompilerInvocation& invocation)
{
    applyProjectOptimization(settings, invocation);
    if (!invocation.jobCountSeen && settings.compilerJobs.has_value())
        invocation.requestedJobs = settings.compilerJobs;
    if (!invocation.libraryReferenceRootSeen && settings.libraryReferenceRoot.has_value())
    {
        invocation.projectLoadOptions.libraryReferenceRoot =
            settings.libraryReferenceRoot->lexically_normal();
    }
    else
        applyEnvironmentLibraryRoot(invocation);
    if (!invocation.genericInstantiationDepthSeen &&
        settings.genericInstantiationDepth.has_value())
    {
        invocation.projectLoadOptions.genericInstantiationDepth =
            settings.genericInstantiationDepth;
    }
    if (!invocation.genericInstantiationCountSeen &&
        settings.genericInstantiationCount.has_value())
    {
        invocation.projectLoadOptions.genericInstantiationCount =
            settings.genericInstantiationCount;
    }
}

ExitCode applyConfiguredOutput(
    const compilation::ProjectBuildSettings& settings,
    CompilerInvocation& invocation,
    std::ostream& error)
{
    if (!isProjectBuildAction(invocation.arguments) ||
        invocation.arguments.size() != 2U || !settings.outputPath.has_value())
        return ExitCode::Success;
    std::error_code directoryError;
    const std::filesystem::path parent = settings.outputPath->parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, directoryError);
    if (directoryError)
    {
        error << "nsc: error: cannot create project OutputPath directory '"
              << parent.string() << "': " << directoryError.message() << '\n';
        return ExitCode::CompilationError;
    }
    invocation.projectConfiguredOutput = settings.outputPath->string();
    invocation.arguments.push_back("-o");
    invocation.arguments.push_back(invocation.projectConfiguredOutput);
    return ExitCode::Success;
}

ExitCode loadProjectDefaults(
    compilation::ProjectLoader& projectLoader,
    CompilerInvocation& invocation,
    std::ostream& error)
{
    if (!isProjectSettingsAction(invocation.arguments)) return ExitCode::Success;
    std::string settingsError;
    const auto settings = projectLoader.loadBuildSettings(
        std::filesystem::path(std::string(invocation.arguments[1])), settingsError);
    if (!settings.has_value())
    {
        error << "nsc: error: " << settingsError << '\n';
        return ExitCode::CompilationError;
    }
    applyProjectSettings(*settings, invocation);
    return applyConfiguredOutput(*settings, invocation, error);
}

void applyProfileDefaults(CompilerInvocation& invocation)
{
    const CompilationProfileSettings settings = resolveCompilationProfile(
        invocation.compilationProfile,
        invocation.optimizationLevelSeen
            ? std::optional<llvm_backend::LlvmOptimizationLevel>{
                  invocation.optimizationLevel}
            : std::nullopt,
        invocation.debugInformationSeen
            ? std::optional<llvm_backend::DebugInformationKind>{
                  invocation.debugInformation}
            : std::nullopt);
    invocation.optimizationLevel = settings.optimizationLevel;
    invocation.debugInformation = settings.debugInformation;
    if (!invocation.fmaContractPolicySeen &&
        invocation.floatingPointMode == llvm_backend::FloatingPointMode::Fast)
        invocation.fmaContractPolicy = llvm_backend::FmaContractPolicy::Fast;
    if (!invocation.floatingPointRoundingModeSeen &&
        (invocation.floatingPointMode == llvm_backend::FloatingPointMode::Deterministic ||
         invocation.floatingPointMode == llvm_backend::FloatingPointMode::Strict))
    {
        invocation.floatingPointRoundingMode =
            llvm_backend::FloatingPointRoundingMode::NearestEven;
    }
}

InvocationCapabilities classifyInvocation(
    std::span<const std::string_view> arguments) noexcept
{
    const bool projectOutput = arguments.size() == 4U && arguments[2] == "-o";
    const bool executable =
        (arguments.size() == 3U && arguments[1] == "-o" &&
         !arguments[0].empty() && arguments[0].front() != '-') ||
        (projectOutput && (arguments[0] == "--emit-exe" ||
                           arguments[0] == "--emit-project-oop-exe"));
    const bool debug = projectOutput &&
        (arguments[0] == "--emit-project-oop-obj" ||
         arguments[0] == "--emit-project-oop-exe" ||
         arguments[0] == "--emit-project-oop-lib" ||
         arguments[0] == "--emit-project-oop-dll");
    const bool artifactCache = projectOutput &&
        (arguments[0] == "--emit-project-oop-exe" ||
         arguments[0] == "--emit-project-oop-lib" ||
         arguments[0] == "--emit-project-oop-dll");
    return {executable, debug, artifactCache};
}

ExitCode validateActionOptions(
    const CompilerInvocation& invocation,
    const InvocationCapabilities capabilities,
    std::ostream& error)
{
    if (invocation.subsystemSeen && !capabilities.executable)
    {
        error << "nsc: error: /subsystem is only valid when emitting an executable\n";
        return ExitCode::CommandLineError;
    }
    if (invocation.debugInformationSeen && !capabilities.debug)
    {
        error << "nsc: error: debug information is currently supported when emitting "
                 "a project OOP object, executable, static library, or DLL\n";
        return ExitCode::CommandLineError;
    }
    if (!invocation.debugPrefixMaps.empty() && !capabilities.debug)
    {
        error << "nsc: error: debug prefix maps are currently supported when emitting "
                 "a project OOP object, executable, static library, or DLL\n";
        return ExitCode::CommandLineError;
    }
    if (invocation.compilationProfileSeen && !capabilities.debug)
    {
        error << "nsc: error: compilation profiles are supported when emitting a project "
                 "OOP object, executable, static library, or DLL\n";
        return ExitCode::CommandLineError;
    }
    const bool explicitCache = invocation.artifactCacheSeen ||
        invocation.artifactCacheDisabled || invocation.artifactCacheMaximumSeen;
    if (explicitCache && !capabilities.artifactCache)
    {
        error << "nsc: error: artifact-cache options are supported when emitting a project "
                 "OOP executable, static library, or DLL\n";
        return ExitCode::CommandLineError;
    }
    return ExitCode::Success;
}

ExitCode applyDefaultArtifactCache(
    CompilerInvocation& invocation,
    bool artifactCacheCapable,
    std::ostream& error)
{
    if (!artifactCacheCapable || invocation.artifactCacheSeen ||
        invocation.artifactCacheDisabled)
        return ExitCode::Success;
    std::error_code pathError;
    const std::filesystem::path projectFile = std::filesystem::absolute(
        std::filesystem::path{invocation.arguments[1]}, pathError);
    if (pathError)
    {
        error << "nsc: error: cannot resolve project path for artifact cache: "
              << pathError.message() << '\n';
        return ExitCode::CommandLineError;
    }
    invocation.artifactCacheRoot =
        llvm_backend::defaultProjectArtifactCacheRoot(projectFile);
    invocation.defaultArtifactCache = true;
    if (!invocation.artifactCacheMaximumSeen)
        invocation.artifactCacheMaximumBytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;
    return ExitCode::Success;
}
} // namespace

ExitCode parseCompilerInvocation(
    std::span<const std::string_view> arguments,
    compilation::ProjectLoader& projectLoader,
    CompilerInvocation& invocation,
    std::ostream& error)
{
    const ExitCode optionsResult = parseCompilerOptions(arguments, invocation, error);
    if (optionsResult != ExitCode::Success) return optionsResult;
    if (invocation.artifactCacheDisabled && invocation.artifactCacheMaximumSeen)
    {
        error << "nsc: error: " << compilerText(
            "conflicting artifact-cache policy\n",
            "la política de caché de artefactos tiene opciones incompatibles\n");
        return ExitCode::CommandLineError;
    }
#if 0 // INSTRUMENTATION CODE: preserve automatic opening before project settings load.
    if (isProjectSettingsAction(invocation.arguments) && !invocation.genericsTraceSeen)
    {
        const ExitCode traceResult = openCompilerGenericsTrace({}, {}, true, error);
        if (traceResult != ExitCode::Success) return traceResult;
    }
#endif // INSTRUMENTATION CODE
    const ExitCode defaultsResult = loadProjectDefaults(projectLoader, invocation, error);
    if (defaultsResult != ExitCode::Success) return defaultsResult;
    applyProfileDefaults(invocation);
    return ExitCode::Success;
}

ExitCode validateCompilerInvocation(
    CompilerInvocation& invocation,
    std::ostream& error)
{
    const InvocationCapabilities capabilities = classifyInvocation(invocation.arguments);
    const ExitCode validation = validateActionOptions(invocation, capabilities, error);
    if (validation != ExitCode::Success) return validation;
    return applyDefaultArtifactCache(invocation, capabilities.artifactCache, error);
}

ExitCode validateCompilerLinkOptions(
    const CompilerInvocation& invocation,
    std::ostream& error)
{
    const std::span<const std::string_view> arguments(invocation.arguments);
    const bool linkTimeOptimizationCapableInvocation =
        arguments.size() == 4U && arguments[2] == "-o" &&
        (arguments[0] == "--emit-project-oop-exe" ||
         arguments[0] == "--emit-project-oop-lib" ||
         arguments[0] == "--emit-project-oop-dll");
    const bool incrementalLinkCapableInvocation =
        arguments.size() == 4U && arguments[2] == "-o" &&
        arguments[0] == "--emit-project-oop-exe";
    if (invocation.linkTimeOptimizationSeen &&
        !linkTimeOptimizationCapableInvocation)
    {
        error << "nsc: error: -flto is supported when emitting a project OOP executable, "
                 "static library, or DLL\n";
        return ExitCode::CommandLineError;
    }
    if (invocation.incrementalLink && !incrementalLinkCapableInvocation)
    {
        error << "nsc: error: --incremental-link is supported when emitting a project "
                 "OOP executable\n";
        return ExitCode::CommandLineError;
    }
    // EN: Freeze 3.0 DEC-13: only a linked executable or DLL imports from DLLs at load time.
    // ES: Congelamiento 3.0 DEC-13: solo un ejecutable o DLL enlazado importa de DLL al cargar.
    const bool fileExecutableInvocation = arguments.size() == 3U && arguments[1] == "-o" &&
        !arguments[0].empty() && arguments[0].front() != '-';
    const bool systemImportCapableInvocation = fileExecutableInvocation ||
        (arguments.size() == 4U && arguments[2] == "-o" &&
         (arguments[0] == "--emit-project-oop-exe" || arguments[0] == "--emit-project-oop-dll" ||
          arguments[0] == "--emit-exe"));
    if (invocation.validateSystemImports && !systemImportCapableInvocation)
    {
        error << "nsc: error: --validate-system-imports is supported when emitting an executable "
                 "or a project OOP DLL\n";
        return ExitCode::CommandLineError;
    }
    return ExitCode::Success;
}
}
