#include "CompilerActionDispatcher.h"

#include "nativesharp/Driver/BreakingChangeLedger.h"
#include "nativesharp/Driver/CompatibilityPolicy.h"
#include "nativesharp/Driver/CompilerConsole.h"
#include "nativesharp/Driver/DeprecationPolicy.h"
#include "nativesharp/Driver/DiagnosticBundle.h"
#include "nativesharp/Driver/ExecutableMigrationGuide.h"
#include "nativesharp/Driver/FinalBaselineContract.h"
#include "nativesharp/Driver/InstallationManifest.h"
#include "nativesharp/Driver/InstallationVerifier.h"
#include "nativesharp/Driver/MigrationAnalyzer.h"
#include "nativesharp/Driver/MigrationBaseline.h"
#include "nativesharp/Driver/MigrationCompatibilityMatrix.h"
#include "nativesharp/Driver/MigrationGuide.h"
#include "nativesharp/Driver/NumericContract.h"
#include "nativesharp/Driver/PackageLifecycle.h"
#include "nativesharp/Driver/PromotionAct.h"
#include "nativesharp/Driver/ReleaseCandidateFreeze.h"
#include "nativesharp/Driver/ToolchainContract.h"
#include "nativesharp/Interop/LibraryApiDiff.h"
#include "nativesharp/Interop/LibraryCapabilityAudit.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/Language3Surface.h"
#include "nativesharp/Language/SmartPointerSemantics.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"
#include "nativesharp/Linker/WindowsRuntimeLibrary.h"
#include "nativesharp/LLVM/FloatingPointMode.h"
#include "nativesharp/LLVM/LlvmOptimizationObservation.h"
#include "nativesharp/LLVM/LlvmVersion.h"
#include "nativesharp/Runtime/RuntimeAbiContract.h"
#include "nativesharp/Runtime/RuntimeCapabilityReport.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>

namespace nativesharp::driver
{
namespace
{
using PathOperationFunction = bool (*)(const std::filesystem::path&, std::string&);
using PathOperation =
    std::tuple<std::string_view, std::string_view, PathOperationFunction>;

constexpr PathOperation kPathOperations[] = {
    {"--write-toolchain-contract", "toolchain contract written: ", writeToolchainContract},
    {"--write-runtime-abi-contract", "runtime ABI contract written: ",
     runtime::writeRuntimeAbiContract},
    {"--verify-runtime-abi-contract", "runtime ABI contract verified: ",
     runtime::verifyRuntimeAbiContractFile},
    {"--write-smart-pointer-contract", "smart pointer contract written: ",
     language::writeSmartPointerContract},
    {"--verify-smart-pointer-contract", "smart pointer contract verified: ",
     language::verifySmartPointerContractFile},
    {"--write-concurrency-contract", "concurrency contract written: ",
     language::writeConcurrencyContract},
    {"--verify-concurrency-contract", "concurrency contract verified: ",
     language::verifyConcurrencyContractFile},
    {"--write-windows-runtime-library", "Windows runtime library written: ",
     linker::writeWindowsRuntimeLibrary},
    {"--verify-windows-runtime-library", "Windows runtime library verified: ",
     linker::verifyWindowsRuntimeLibrary},
    {"--verify-toolchain-contract", "toolchain contract verified: ",
     verifyToolchainContract},
    {"--write-compatibility-policy", "compatibility policy written: ",
     writeCompatibilityPolicy},
    {"--verify-compatibility-policy", "compatibility policy verified: ",
     verifyCompatibilityPolicy},
    {"--write-compatibility-matrix", "compatibility matrix written: ",
     writeCompatibilityMatrix},
    {"--verify-compatibility-matrix", "compatibility matrix verified: ",
     verifyCompatibilityMatrix},
    {"--write-breaking-change-ledger", "breaking-change ledger written: ",
     writeBreakingChangeLedger},
    {"--verify-breaking-change-ledger", "breaking-change ledger verified: ",
     verifyBreakingChangeLedger},
    {"--write-migration-baseline", "migration baseline written: ",
     writeMigrationBaseline},
    {"--verify-migration-baseline", "migration baseline verified: ",
     verifyMigrationBaseline},
    {"--write-migration-guide", "migration guide written: ", writeMigrationGuide},
    {"--verify-migration-guide", "migration guide verified: ", verifyMigrationGuide},
    {"--write-release-candidate-freeze", "release-candidate freeze written: ",
     writeReleaseCandidateFreeze},
    {"--verify-release-candidate-freeze", "release-candidate freeze verified: ",
     verifyReleaseCandidateFreeze},
    {"--write-final-baseline-contract", "final baseline contract written: ",
     writeFinalBaselineContract},
    {"--verify-final-baseline-contract", "final baseline contract verified: ",
     verifyFinalBaselineContract},
    {"--write-promotion-act", "promotion act written: ", writePromotionAct},
    {"--verify-promotion-act", "promotion act verified: ", verifyPromotionAct},
    {"--verify-installation", "NativeSharp installation verified: ", verifyInstallation}};

constexpr std::string_view kProjectActions[] = {
    "--check-project", "--dump-project", "--check-project-graph",
    "--dump-project-graph", "--check-project-syntax", "--dump-project-compilation",
    "--dump-project-comptime", "--check-project-generics", "--dump-project-generics",
    "--check-project-names", "--dump-project-names", "--check-project-members",
    "--dump-project-members", "--check-project-inheritance", "--dump-project-inheritance",
    "--check-project-interfaces", "--dump-project-interfaces",
    "--check-project-generic-constraints", "--check-project-method-bodies",
    "--dump-project-method-bodies", "--check-project-virtual-dispatch",
    "--dump-project-virtual-dispatch", "--check-project-destruction",
    "--dump-project-destruction", "--check-project-constructions",
    "--dump-project-constructions", "--check-project-layouts", "--dump-project-layouts",
    "--check-project-oop-hir", "--dump-project-oop-hir", "--check-project-oop-mir",
    "--dump-project-oop-mir", "--explain-project-costs", "--explain-project-costs-json",
    "--verify-project-oop-llvm", "--dump-project-oop-llvm"};

constexpr std::string_view kFrontendActions[] = {
    "--check", "--dump-source", "--dump-tokens", "--dump-ast", "--dump-symbols",
    "--dump-sema", "--dump-hir", "--verify-hir", "--dump-mir", "--verify-mir",
    "--dump-cfg", "--verify-cfg", "--dump-dominance", "--verify-dominance",
    "--dump-ssa", "--verify-ssa", "--dump-opt", "--verify-opt",
    "--dump-pass-pipeline", "--dump-llvm", "--verify-llvm", "--dump-llvm-opt",
    "--verify-llvm-opt"};

bool containsAction(
    std::span<const std::string_view> actions, std::string_view action) noexcept
{
    return std::find(actions.begin(), actions.end(), action) != actions.end();
}
} // namespace

CompilerActionDispatcher::CompilerActionDispatcher(
    const Driver&,
    std::span<const std::string_view> arguments,
    const CompilerInvocation& invocation,
    const CompilationJobPolicy& jobPolicy,
    const compilation::ProjectLoadOptions& projectLoadOptions,
    std::string_view compilerStage,
    std::ostream& output,
    std::ostream& error) noexcept
    : arguments_(arguments),
      invocation_(invocation),
      jobPolicy_(jobPolicy),
      projectLoadOptions_(projectLoadOptions),
      compilerStage_(compilerStage),
      output_(output),
      error_(error)
{
}

void CompilerActionDispatcher::showHelp(std::ostream& output)
{
    Driver::printHelp(output);
}

ExitCode CompilerActionDispatcher::dispatch() const
{
    Result result;
    if (arguments_.size() == 1U) result = dispatchSingleArgument();
    else if (arguments_.size() == 2U) result = dispatchTwoArguments();
    else if (arguments_.size() == 3U) result = dispatchThreeArguments();
    else if (arguments_.size() == 4U)
    {
        result = dispatchDeprecationSimulation();
        if (!result.has_value()) result = dispatchFourArguments();
    }
    else if (arguments_.size() == 5U) result = dispatchFiveArguments();
    if (result.has_value()) return *result;
    result = dispatchEmission();
    return result.has_value() ? *result : invalidCommandLine();
}

CompilerActionDispatcher::Result
CompilerActionDispatcher::dispatchCompilerQuery(std::string_view action) const
{
    if (action == "--help" || action == "-h") Driver::printHelp(output_);
    else if (action == "--version") Driver::printVersion(output_);
    else if (action == "--print-compiler-identity") Driver::printCompilerIdentity(output_);
    else if (action == "--print-target") Driver::printTarget(output_);
    else if (action == "--print-data-layout") Driver::printDataLayout(output_);
    else if (action == "--print-codegen")
        return Driver::printCodeGeneration(
            invocation_.optimizationLevel, output_, error_);
    else if (action == "--print-llvm-pipeline")
        Driver::printLlvmPipeline(invocation_.optimizationLevel, output_);
    else if (action == "--print-optimization-contract")
        output_ << llvm_backend::llvmOptimizationContractManifest();
    else if (action == "--print-llvm-version")
    {
        const llvm_backend::LlvmVersion version = llvm_backend::loadedLlvmVersion();
        output_ << "LLVM " << llvm_backend::llvmVersionString(version)
                << " (supported: " << llvm_backend::kSupportedLlvmMajor << ".x)\n";
        std::string versionError;
        if (!llvm_backend::verifyLoadedLlvmVersion(versionError))
        {
            error_ << "nsc: internal error: " << versionError << '\n';
            return ExitCode::InternalError;
        }
    }
    else if (action == "--print-toolchain-contract") output_ << currentToolchainContract();
    else return {};
    return ExitCode::Success;
}

CompilerActionDispatcher::Result
CompilerActionDispatcher::dispatchLanguageQuery(std::string_view action) const
{
    if (action == "--print-runtime-abi-contract")
        output_ << runtime::runtimeAbiContractManifest();
    else if (action == "--print-smart-pointer-contract")
        output_ << language::smartPointerContractManifest();
    else if (action == "--print-concurrency-contract")
        output_ << language::concurrencyContractManifest();
    else if (action == "--print-language-3-surface")
        output_ << language::language3SurfaceManifest();
    else if (action == "--print-runtime-capabilities")
        output_ << runtime::runtimeCapabilityReport();
    else if (action == "--print-numeric-contract")
        output_ << numericContractManifest(llvm_backend::captureFloatingPointState());
    else return {};
    return ExitCode::Success;
}

CompilerActionDispatcher::Result
CompilerActionDispatcher::dispatchPolicyQuery(std::string_view action) const
{
    if (action == "--print-compatibility-policy") output_ << currentCompatibilityPolicy();
    else if (action == "--print-compatibility-matrix")
        output_ << currentCompatibilityMatrix();
    else if (action == "--print-breaking-change-ledger")
        output_ << currentBreakingChangeLedger();
    else if (action == "--print-migration-baseline") output_ << currentMigrationBaseline();
    else if (action == "--print-migration-guide") output_ << currentMigrationGuide();
    else if (action == "--print-deprecation-policy") output_ << currentDeprecationPolicy();
    else if (action == "--print-release-candidate-freeze")
        output_ << currentReleaseCandidateFreeze();
    else if (action == "--print-final-baseline-contract")
        output_ << currentFinalBaselineContract();
    else if (action == "--print-promotion-act") output_ << currentPromotionAct();
    else return {};
    return ExitCode::Success;
}

CompilerActionDispatcher::Result
CompilerActionDispatcher::dispatchSingleUtility(std::string_view action) const
{
    if (action == "--print-job-policy")
    {
        output_ << "hardware-threads: " << jobPolicy_.hardwareThreads() << '\n'
                << "requested-jobs: ";
        if (jobPolicy_.usesAutomaticJobs()) output_ << "auto";
        else output_ << jobPolicy_.requestedJobs();
        output_ << '\n' << "reserved-cores: " << jobPolicy_.reservedCores() << '\n'
                << "effective-jobs: " << jobPolicy_.effectiveJobs() << '\n'
                << "scheduler: project-references\n";
        return ExitCode::Success;
    }
    if (action == "--print-linker")
        return Driver::printLinker(output_, error_);
    if (action == "--dump-runtime-llvm")
        return Driver::dumpRuntimeLlvm(output_, error_);
    return {};
}

CompilerActionDispatcher::Result
CompilerActionDispatcher::dispatchSingleArgument() const
{
    const std::string_view action = arguments_.front();
    Result result = dispatchCompilerQuery(action);
    if (result.has_value()) return result;
    result = dispatchLanguageQuery(action);
    if (result.has_value()) return result;
    result = dispatchPolicyQuery(action);
    if (result.has_value()) return result;
    result = dispatchSingleUtility(action);
    if (result.has_value()) return result;
    if (!action.empty() && action.front() == '-')
    {
        error_ << "nsc: error: " << compilerText("unknown option '", "opción desconocida '")
               << action << "'\n";
        return ExitCode::CommandLineError;
    }
    return Driver::runFrontend(
        "--program", action, invocation_.optimizationLevel, output_, error_);
}

CompilerActionDispatcher::Result
CompilerActionDispatcher::dispatchMigrationAnalysis(std::string_view action) const
{
    if (action != "--analyze-migration" && action != "--analyze-migration-strict")
        return {};
    MigrationAnalysis analysis;
    std::string migrationError;
    if (!analyzeMigrationFile(
            std::filesystem::path(arguments_[1]), analysis, migrationError))
    {
        error_ << "nsc: error: " << migrationError << '\n';
        return ExitCode::CompilationError;
    }
    output_ << renderMigrationAnalysis(analysis);
    if (action == "--analyze-migration-strict" && !analysis.findings.empty())
    {
        error_ << "nsc: error: migration warnings are errors; "
               << analysis.findings.size() << " finding(s)\n";
        return ExitCode::CompilationError;
    }
    return ExitCode::Success;
}

CompilerActionDispatcher::Result
CompilerActionDispatcher::dispatchRuntimeAndNumeric(std::string_view action) const
{
    if (action == "--check-runtime-capability")
    {
        std::string detail;
        if (!runtime::runtimeCapabilityReady(arguments_[1], detail))
        {
            error_ << "nsc: error: " << detail << '\n';
            return ExitCode::CompilationError;
        }
        output_ << detail << '\n';
        return ExitCode::Success;
    }
    if (action != "--write-numeric-contract" && action != "--verify-numeric-contract")
        return {};
    std::string contractError;
    const llvm_backend::FloatingPointState state =
        llvm_backend::captureFloatingPointState();
    const bool success = action == "--write-numeric-contract"
        ? writeNumericContract(std::filesystem::path(arguments_[1]), state, contractError)
        : verifyNumericContractFile(
              std::filesystem::path(arguments_[1]), state, contractError);
    if (!success)
    {
        error_ << "nsc: error: " << contractError << '\n';
        return ExitCode::CompilationError;
    }
    output_ << (action == "--write-numeric-contract"
        ? "numeric contract written: " : "numeric contract verified: ")
            << arguments_[1] << '\n';
    return ExitCode::Success;
}

CompilerActionDispatcher::Result
CompilerActionDispatcher::dispatchPathOperation(std::string_view action) const
{
    const auto operation = std::find_if(
        std::begin(kPathOperations), std::end(kPathOperations),
        [action](const PathOperation& candidate) {
            return std::get<0>(candidate) == action;
        });
    if (operation == std::end(kPathOperations)) return {};
    std::string operationError;
    const std::filesystem::path path{std::string(arguments_[1])};
    if (!std::get<2>(*operation)(path, operationError))
    {
        error_ << "nsc: error: " << operationError << '\n';
        return ExitCode::CompilationError;
    }
    output_ << std::get<1>(*operation) << path.string() << '\n';
    return ExitCode::Success;
}

CompilerActionDispatcher::Result
CompilerActionDispatcher::dispatchPeAndCompatibility(std::string_view action) const
{
    if (action == "--verify-pe")
    {
        const std::filesystem::path source{std::string(arguments_[1])};
        linker::PeExecutableVerifier verifier;
        if (!verifier.verify(source))
        {
            error_ << "nsc: error: PE verification failed: " << verifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        const linker::PeExecutableInfo& info = verifier.info();
        output_ << "PE executable verified: " << source.string()
                << " (machine=0x" << std::hex << info.machine << std::dec
                << ", sections=" << info.sectionCount
                << ", imports=" << info.importedSymbols.size() << ")\n";
        return ExitCode::Success;
    }
    if (action != "--check-compiler-compatibility") return {};
    std::string explanation;
    std::string compatibilityError;
    if (!checkCompilerCompatibility(arguments_[1], explanation, compatibilityError))
    {
        error_ << "nsc: error: "
               << (compatibilityError.empty() ? explanation : compatibilityError) << '\n';
        return ExitCode::CompilationError;
    }
    output_ << explanation << '\n';
    return ExitCode::Success;
}

CompilerActionDispatcher::Result
CompilerActionDispatcher::dispatchLibraryAndFrontend(std::string_view action) const
{
    if (action == "--verify-library-metadata")
        return Driver::verifyLibraryMetadata(arguments_[1], output_, error_);
    if (action == "--audit-library-capabilities")
    {
        std::string report;
        std::string auditError;
        if (!interop::auditLibraryCapabilityDirectory(
                std::filesystem::path(arguments_[1]), report, auditError))
        {
            error_ << "nsc: error: " << auditError << '\n';
            return ExitCode::CompilationError;
        }
        output_ << report;
        return ExitCode::Success;
    }
    if (action == "--dump-library-abi")
        return Driver::dumpLibraryAbi(arguments_[1], output_, error_);
    if (action == "--dump-api-surface")
        return Driver::dumpLibraryApiSurface(arguments_[1], output_, error_);
    if (containsAction(kProjectActions, action))
        return Driver::runProject(action, arguments_[1], output_, error_);
    if (containsAction(kFrontendActions, action))
        return Driver::runFrontend(
            action, arguments_[1], invocation_.optimizationLevel, output_, error_);
    return {};
}

CompilerActionDispatcher::Result CompilerActionDispatcher::dispatchTwoArguments() const
{
    const std::string_view action = arguments_[0];
    Result result = dispatchMigrationAnalysis(action);
    if (result.has_value()) return result;
    result = dispatchRuntimeAndNumeric(action);
    if (result.has_value()) return result;
    result = dispatchPathOperation(action);
    if (result.has_value()) return result;
    result = dispatchPeAndCompatibility(action);
    if (result.has_value()) return result;
    return dispatchLibraryAndFrontend(action);
}

CompilerActionDispatcher::Result
CompilerActionDispatcher::dispatchDeprecationSimulation() const
{
    if ((arguments_.size() != 3U && arguments_.size() != 4U) ||
        arguments_[0] != "--simulate-deprecation-removal")
        return {};
    const DeprecatedFeature* feature = deprecatedFeature(arguments_[1]);
    const std::optional<DeprecationRelease> release =
        parseDeprecationRelease(arguments_[2]);
    const bool securityException =
        arguments_.size() == 4U && arguments_[3] == "--security-ub";
    if (feature == nullptr || !release.has_value() ||
        (arguments_.size() == 4U && !securityException))
    {
        error_ << "nsc: error: invalid deprecation-removal simulation\n";
        return ExitCode::CommandLineError;
    }
    const bool allowed = deprecationRemovalAllowed(
        *feature, *release, true, securityException);
    output_ << "deprecation removal feature=" << feature->identity
            << " release=" << release->major << '.' << release->minor
            << " security-ub=" << (securityException ? "yes" : "no")
            << " allowed=" << (allowed ? "yes" : "no") << '\n';
    return allowed ? ExitCode::Success : ExitCode::CompilationError;
}

CompilerActionDispatcher::Result
CompilerActionDispatcher::dispatchMigrationCompatibilityMatrix() const
{
    if (arguments_[0] != "--migration-compatibility-matrix") return {};
    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata oldMetadata;
    interop::LibraryMetadata newMetadata;
    std::string metadataError;
    if (!codec.read(std::filesystem::path(arguments_[1]), oldMetadata, metadataError) ||
        !codec.verifyCurrentCompiler(oldMetadata, metadataError))
    {
        error_ << "nsc: error: invalid old NativeSharp library metadata: "
               << metadataError << '\n';
        return ExitCode::CompilationError;
    }
    if (!codec.read(std::filesystem::path(arguments_[2]), newMetadata, metadataError) ||
        !codec.verifyCurrentCompiler(newMetadata, metadataError))
    {
        error_ << "nsc: error: invalid new NativeSharp library metadata: "
               << metadataError << '\n';
        return ExitCode::CompilationError;
    }
    const interop::LibraryApiDiffReport apiDiff =
        interop::LibraryApiDiffer::compare(oldMetadata, newMetadata);
    const MigrationCompatibilityMatrix matrix = buildMigrationCompatibilityMatrix(
        apiDiff,
        interop::LibraryMetadataCodec::fingerprintHex(oldMetadata.abiFingerprint),
        interop::LibraryMetadataCodec::fingerprintHex(newMetadata.abiFingerprint));
    output_ << renderMigrationCompatibilityMatrix(matrix);
    return ExitCode::Success;
}

CompilerActionDispatcher::Result
CompilerActionDispatcher::dispatchInstallationComparison() const
{
    if (arguments_[0] != "--compare-installation-manifests") return {};
    ReproducibilityComparison comparison;
    std::string comparisonError;
    if (!compareInstallationManifests(
            std::filesystem::path(std::string(arguments_[1])),
            std::filesystem::path(std::string(arguments_[2])),
            comparison,
            comparisonError))
    {
        error_ << "nsc: error: " << comparisonError << '\n';
        return ExitCode::CompilationError;
    }
    output_ << comparison.report;
    if (!comparison.reproducible)
    {
        error_ << "nsc: error: installation builds differ in "
               << comparison.differences << " deterministic entr"
               << (comparison.differences == 1 ? "y" : "ies") << '\n';
        return ExitCode::CompilationError;
    }
    return ExitCode::Success;
}

CompilerActionDispatcher::Result CompilerActionDispatcher::dispatchDiagnosticBundle() const
{
    if (arguments_[0] != "--write-diagnostic-bundle") return {};
    std::ostringstream capturedOutput;
    std::ostringstream capturedError;
    const ExitCode result = Driver::runProject(
        "--verify-project-oop-llvm", arguments_[1], capturedOutput, capturedError);
    const std::array<std::string_view, 2> bundleArguments = {
        "--verify-project-oop-llvm", "-O0"};
    std::string bundleError;
    if (!writeDiagnosticBundle(
            std::filesystem::path(std::string(arguments_[1])),
            std::filesystem::path(std::string(arguments_[2])),
            result,
            capturedOutput.str(),
            capturedError.str(),
            projectLoadOptions_,
            bundleArguments,
            bundleError))
    {
        error_ << "nsc: error: " << bundleError << '\n';
        return ExitCode::CompilationError;
    }
    output_ << "diagnostic bundle written: " << arguments_[2] << '\n';
    return result;
}

CompilerActionDispatcher::Result CompilerActionDispatcher::dispatchThreeArguments() const
{
    if (arguments_[0] == "--verify-library-artifacts")
        return Driver::verifyLibraryArtifacts(
            arguments_[1], arguments_[2], {}, {}, output_, error_);
    if (arguments_[0] == "--api-diff")
        return Driver::diffLibraryApi(
            arguments_[1], arguments_[2], output_, error_);
    Result result = dispatchDeprecationSimulation();
    if (result.has_value()) return result;
    result = dispatchMigrationCompatibilityMatrix();
    if (result.has_value()) return result;
    if (arguments_[0] == "--abi-diff")
        return Driver::diffLibraryAbi(
            arguments_[1], arguments_[2], output_, error_);
    if (arguments_[0] == "--verify-installation-manifest")
    {
        std::string manifestError;
        const std::filesystem::path prefix{std::string(arguments_[1])};
        const std::filesystem::path manifest{std::string(arguments_[2])};
        if (!verifyInstallationManifest(prefix, manifest, manifestError))
        {
            error_ << "nsc: error: " << manifestError << '\n';
            return ExitCode::CompilationError;
        }
        output_ << "installation manifest verified: " << manifest.string() << '\n';
        return ExitCode::Success;
    }
    result = dispatchInstallationComparison();
    if (result.has_value()) return result;
    if (arguments_[0] == "--write-installation-removal-plan")
    {
        std::string planError;
        const std::filesystem::path destination{std::string(arguments_[2])};
        if (!writeInstallationRemovalPlan(
                std::filesystem::path(std::string(arguments_[1])),
                destination,
                planError))
        {
            error_ << "nsc: error: " << planError << '\n';
            return ExitCode::CompilationError;
        }
        output_ << "installation removal plan written: " << destination.string() << '\n';
        return ExitCode::Success;
    }
    return dispatchDiagnosticBundle();
}

CompilerActionDispatcher::Result CompilerActionDispatcher::dispatchFourArguments() const
{
    if (arguments_[0] == "--verify-library-artifacts")
        return Driver::verifyLibraryArtifacts(
            arguments_[1], arguments_[2], arguments_[3], {}, output_, error_);
    if (arguments_[0] == "--apply-migration-fixes" && arguments_[2] == "-o")
    {
        MigrationAnalysis analysis;
        std::size_t appliedFixCount = 0;
        std::string migrationError;
        if (!applySafeMigrationFixesToFile(
                std::filesystem::path(arguments_[1]),
                std::filesystem::path(arguments_[3]),
                analysis,
                appliedFixCount,
                migrationError))
        {
            error_ << "nsc: error: " << migrationError << '\n';
            return ExitCode::CompilationError;
        }
        output_ << "safe migration fixes applied: " << appliedFixCount << '\n'
                << "migration output written: " << arguments_[3] << '\n';
        return ExitCode::Success;
    }
    if (arguments_[0] == "--write-source-migration-guide" && arguments_[2] == "-o")
    {
        ExecutableMigrationGuide guide;
        std::string migrationError;
        if (!writeExecutableMigrationGuide(
                std::filesystem::path(arguments_[1]),
                std::filesystem::path(arguments_[3]), guide, migrationError))
        {
            error_ << "nsc: error: " << migrationError << '\n';
            return ExitCode::CompilationError;
        }
        output_ << "migration guide chapters: " << guide.chapters.size() << '\n'
                << "migration guide written: " << arguments_[3] << '\n';
        return ExitCode::Success;
    }
    if (arguments_[0] == "--write-installation-manifest")
    {
        std::string manifestError;
        const std::filesystem::path prefix{std::string(arguments_[1])};
        const std::filesystem::path expectedFiles{std::string(arguments_[2])};
        const std::filesystem::path destination{std::string(arguments_[3])};
        if (!writeInstallationManifest(prefix, expectedFiles, destination, manifestError))
        {
            error_ << "nsc: error: " << manifestError << '\n';
            return ExitCode::CompilationError;
        }
        output_ << "installation manifest written: " << destination.string() << '\n';
        return ExitCode::Success;
    }
    if (arguments_[0] != "--write-installation-update-plan") return {};
    std::string planError;
    const std::filesystem::path destination{std::string(arguments_[3])};
    if (!writeInstallationUpdatePlan(
            std::filesystem::path(std::string(arguments_[1])),
            std::filesystem::path(std::string(arguments_[2])), destination, planError))
    {
        error_ << "nsc: error: " << planError << '\n';
        return ExitCode::CompilationError;
    }
    output_ << "installation update plan written: " << destination.string() << '\n';
    return ExitCode::Success;
}

CompilerActionDispatcher::Result CompilerActionDispatcher::dispatchFiveArguments() const
{
    if (arguments_[0] == "--verify-library-artifacts")
        return Driver::verifyLibraryArtifacts(
            arguments_[1], arguments_[2], arguments_[3], arguments_[4], output_, error_);
    if (arguments_[0] != "--apply-installation-removal-plan") return {};
    std::string planError;
    const std::filesystem::path nextManifest = arguments_[3] == "none"
        ? std::filesystem::path{}
        : std::filesystem::path(std::string(arguments_[3]));
    if (!applyInstallationRemovalPlan(
            std::filesystem::path(std::string(arguments_[1])),
            std::filesystem::path(std::string(arguments_[2])),
            nextManifest,
            std::filesystem::path(std::string(arguments_[4])),
            planError))
    {
        error_ << "nsc: error: " << planError << '\n';
        return ExitCode::CompilationError;
    }
    output_ << "installation removal plan applied: " << arguments_[4] << '\n';
    return ExitCode::Success;
}

CompilerActionDispatcher::Result CompilerActionDispatcher::dispatchEmission() const
{
    if (arguments_.size() == 3U && arguments_[1] == "-o" &&
        !arguments_[0].empty() && arguments_[0].front() != '-')
    {
        return Driver::runExecutableGeneration(
            arguments_[0], arguments_[2], invocation_.optimizationLevel,
            invocation_.subsystem, output_, error_);
    }
    if (arguments_.size() != 4U || arguments_[2] != "-o") return {};
    const std::string_view action = arguments_[0];
    if (action == "--emit-obj" || action == "--emit-asm")
        return Driver::runCodeGeneration(
            action, arguments_[1], arguments_[3], invocation_.optimizationLevel,
            output_, error_);
    if (action == "--emit-project-oop-obj")
        return Driver::runProjectOopCodeGeneration(
            arguments_[1], arguments_[3], invocation_.optimizationLevel,
            invocation_.debugInformation, invocation_.debugPrefixMaps, output_, error_);
    if (action == "--emit-project-oop-exe")
        return Driver::runProjectOopExecutableGeneration(
            arguments_[1], arguments_[3], invocation_.optimizationLevel,
            invocation_.debugInformation, invocation_.debugPrefixMaps,
            invocation_.linkTimeOptimization, invocation_.incrementalLink,
            invocation_.subsystem, invocation_.extraLinkLibraries, output_, error_);
    if (action == "--emit-project-oop-lib")
        return Driver::runProjectOopStaticLibraryGeneration(
            arguments_[1], arguments_[3], invocation_.optimizationLevel,
            invocation_.debugInformation, invocation_.debugPrefixMaps,
            invocation_.linkTimeOptimization, output_, error_);
    if (action == "--emit-project-oop-dll")
        return Driver::runProjectOopDynamicLibraryGeneration(
            arguments_[1], arguments_[3], invocation_.optimizationLevel,
            invocation_.debugInformation, invocation_.debugPrefixMaps,
            invocation_.linkTimeOptimization, invocation_.extraLinkLibraries,
            output_, error_);
    if (action == "--emit-project-host-header")
        return Driver::runProjectHostHeaderGeneration(
            arguments_[1], arguments_[3], output_, error_);
    if (action == "--emit-project-library-metadata")
        return Driver::runProjectLibraryMetadataGeneration(
            arguments_[1], arguments_[3], output_, error_);
    if (action == "--emit-exe")
        return Driver::runExecutableGeneration(
            arguments_[1], arguments_[3], invocation_.optimizationLevel,
            invocation_.subsystem, output_, error_);
    return {};
}

ExitCode CompilerActionDispatcher::invalidCommandLine() const
{
    error_ << "nsc: error: " << compilerText(
                  "invalid command line; use --help for supported options ",
                  "línea de comandos no válida; use --help para ver las opciones compatibles ")
           << "(stage " << compilerStage_ << ")\n";
    return ExitCode::CommandLineError;
}
}
