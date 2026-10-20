#include "nativesharp/Driver/Driver.h"
#include "ArtifactPublisher.h"
#include "CompilerActionDispatcher.h"
#include "CompilerInvocation.h"
#include "Internal/DriverExecutionScope.h"
#include "Internal/DriverInternal.h"
#include "Internal/DriverPhaseTimer.h"
#include "Internal/LayoutRow.h"
#include "Internal/LoadedPrecompiledMetadata.h"
#include "Internal/PrecompiledExportContract.h"
#include "PublicSurfaceEmitter.h"
#include "PublicSurfaceEmitterSupport.h"
#include "nativesharp/Driver/ProjectCommands.h"
#include "nativesharp/Driver/BreakingChangeLedger.h"
#include "nativesharp/Driver/CompatibilityPolicy.h"
#include "nativesharp/Driver/CompilationProfile.h"
#include "nativesharp/Driver/CompilationJobPolicy.h"
#include "nativesharp/Driver/CompilationTelemetry.h"
#include "nativesharp/Driver/CompilerConsole.h"
#include "nativesharp/Driver/DiagnosticBundle.h"
#include "nativesharp/Driver/DeprecationPolicy.h"
#include "nativesharp/Driver/FinalBaselineContract.h"
#include "nativesharp/Driver/ExecutableMigrationGuide.h"
#include "nativesharp/Driver/InstallationManifest.h"
#include "nativesharp/Driver/InstallationVerifier.h"
#include "nativesharp/Driver/MigrationBaseline.h"
#include "nativesharp/Driver/MigrationAnalyzer.h"
#include "nativesharp/Driver/MigrationCompatibilityMatrix.h"
#include "nativesharp/Driver/MigrationGuide.h"
#include "nativesharp/Driver/PackageLifecycle.h"
#include "nativesharp/Driver/PromotionAct.h"
#include "nativesharp/Driver/ReleaseCandidateFreeze.h"
#include "nativesharp/Driver/ToolchainContract.h"
#include "nativesharp/Driver/NumericContract.h"
#include "nativesharp/Linker/WindowsRuntimeLibrary.h"
#include "nativesharp/Runtime/RuntimeAbiContract.h"
#include "nativesharp/Runtime/RuntimeCapabilityReport.h"
#include <algorithm>
#include <cstdlib>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "nativesharp/AST/AstDumper.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/CFG/CfgDumper.h"
#include "nativesharp/CFG/CfgVerifier.h"
#include "nativesharp/Compilation/ProjectDumper.h"
#include "nativesharp/Compilation/ProjectGraphDumper.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Diagnostics/DiagnosticRenderer.h"
#include "nativesharp/Dominance/DominanceDumper.h"
#include "nativesharp/Dominance/DominanceVerifier.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectCompilationDumper.h"
#include "nativesharp/Frontend/ProjectConstructionModelDumper.h"
#include "nativesharp/Frontend/ProjectDestructionModelDumper.h"
#include "nativesharp/Frontend/ProjectGenericModelDumper.h"
#include "nativesharp/Frontend/ProjectInheritanceModelDumper.h"
#include "nativesharp/Frontend/ProjectInterfaceModelDumper.h"
#include "nativesharp/Frontend/ProjectMemberModelDumper.h"
#include "nativesharp/Frontend/ProjectMethodBodyModelDumper.h"
#include "nativesharp/Frontend/ProjectNameLookupDumper.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModelDumper.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/Frontend/ProjectCostExplorer.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModelDumper.h"
#include "nativesharp/HIR/HirDumper.h"
#include "nativesharp/HIR/HirVerifier.h"
#include "nativesharp/Interop/CHeaderGenerator.h"
#include "nativesharp/Interop/LibraryAbiReport.h"
#include "nativesharp/Interop/LibraryApiDiff.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Interop/LibraryCapabilityAudit.h"
#include "nativesharp/Language/SmartPointerSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/Language3Surface.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/CodegenIdentity.h"
#include "nativesharp/LLVM/FloatingPointMode.h"
#include "nativesharp/LLVM/LlvmLowerer.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizationObservation.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmProcessTrace.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/LlvmVersion.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"
#include "nativesharp/Lexer/TokenDumper.h"
#include "nativesharp/Linker/CoffArchiveVerifier.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"
#include "nativesharp/Linker/PdbVerifier.h"
#include "nativesharp/Linker/WindowsExecutableBuilder.h"
#include "nativesharp/Linker/WindowsLibraryBuilder.h"
#include "nativesharp/MIR/MirDumper.h"
#include "nativesharp/MIR/MirVerifier.h"
#include "nativesharp/Passes/OptimizationDumper.h"
#include "nativesharp/Passes/OptimizationVerifier.h"
#include "nativesharp/Passes/PassPipelineDumper.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"
#include "nativesharp/SSA/SsaDumper.h"
#include "nativesharp/SSA/SsaVerifier.h"
#include "nativesharp/Sema/SemanticDumper.h"
#include "nativesharp/Sema/SymbolDumper.h"
#include "nativesharp/Source/SourceFile.h"
#include "nativesharp/Target/TargetDataLayout.h"
#include "nativesharp/Target/TargetInfo.h"

#include <array>
#include <bit>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <ostream>
#include <set>
#include <sstream>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::driver
{
ExitCode Driver::run(
    std::span<const std::string_view> arguments,
    std::ostream& output,
    std::ostream& error) const
{
    CompilerConsoleArguments consoleArguments =
        parseCompilerConsoleArguments(arguments, activeCompilerLanguage());
    CompilerLanguageScope languageScope(consoleArguments.language);
    if (!consoleArguments.error.empty())
    {
        error << "nsc: " << compilerText("error: ", "error: ")
              << consoleArguments.error << '\n';
        return ExitCode::CommandLineError;
    }
    arguments = std::span<const std::string_view>(consoleArguments.arguments);
    if (!arguments.empty() && isProjectCommand(arguments.front()))
    {
        return runProjectCommand(
            arguments,
            output,
            error,
            [this](std::span<const std::string_view> nested,
                   std::ostream& nestedOutput,
                   std::ostream& nestedError) {
                return run(nested, nestedOutput, nestedError);
            });
    }
    return runCompilerSession(*this, arguments, output, error);
}

ExitCode Driver::printCodeGeneration(
    llvm_backend::LlvmOptimizationLevel optimizationLevel, std::ostream &output, std::ostream &error)
{
    std::string targetError;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
        llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
    if (targetMachine == nullptr)
    {
        error << "nsc: internal error: target machine creation failed: " << targetError << '\n';
        return ExitCode::CompilationError;
    }

    output << "target-triple: " << targetMachine->triple() << '\n'
           << "data-layout:   " << targetMachine->dataLayout() << '\n'
           << "object-format: COFF\n"
           << "machine:       AMD64 (0x8664)\n"
           << "optimization:  " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n';
    return ExitCode::Success;
}

void Driver::printLlvmPipeline(llvm_backend::LlvmOptimizationLevel optimizationLevel, std::ostream &output)
{
    output << "optimization-level: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "pass-pipeline: " << llvm_backend::llvmOptimizationPipelineName(optimizationLevel) << '\n'
           << "target-aware: yes\n"
           << "verify-each: "
           << (llvm_backend::llvmVerifyEachPassRequired(
                   optimizationLevel, llvm_backend::llvmProcessTraceActive())
                   ? "yes"
                   : "no")
           << '\n';
}

bool prepareCodeGenerationModule(
    std::string_view inputPath,
    std::unique_ptr<llvm_backend::LlvmModule>& llvmModule,
    std::ostream& error)
{
    frontend::FrontendCompilation compilation;
    std::string loadError;
    const bool success = compilation.compileFile(std::filesystem::path(inputPath), loadError, false);
    if (!loadError.empty())
    {
        error << "nsc: error: " << loadError << '\n';
        return false;
    }
    if (!success || compilation.optimizationModule() == nullptr || compilation.semanticAnalyzer() == nullptr)
    {
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        error << "nsc: error: code generation requires a successful optimized "
                 "compilation\n";
        return false;
    }

    llvm_backend::LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
    llvmModule = lowerer.lower(compilation.optimizationModule());
    if (llvmModule == nullptr)
    {
        error << "nsc: internal error: LLVM lowering failed: " << lowerer.error() << '\n';
        return false;
    }

    std::string verificationError;
    if (!llvmModule->verify(verificationError))
    {
        error << "nsc: internal error: LLVM verification failed: " << verificationError << '\n';
        return false;
    }
    return true;
}

bool optimizeCodeGenerationModule(
    llvm_backend::LlvmModule& llvmModule,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::unique_ptr<llvm_backend::LlvmTargetMachine>& targetMachine,
    std::ostream& error)
{
    std::string targetError;
    targetMachine = llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
    if (targetMachine == nullptr)
    {
        error << "nsc: internal error: target machine creation failed: " << targetError << '\n';
        return false;
    }

    // NSC-LIMIT-005. La observacion va SIEMPRE, no bajo bandera. Un fallo del
    // optimizador que no dice contra que LLVM, con que pipeline ni por donde iba
    // obliga a REPRODUCIRLO para poder diagnosticarlo, y eso es exactamente lo
    // que no se pudo hacer con `c38_enum_ancho`: cuando el sintoma es un codigo
    // de salida distinto a -O2, la corrida que fallo ya termino. Ver
    // LlvmOptimizer.h y LlvmOptimizationObservation.h.
    //
    // COSTE MEDIDO: ~3% del tiempo de compilacion completo a -O2 y 0% a -O0, y el
    // IR resultante es byte a byte el mismo con y sin observar --hay una prueba
    // que lo compara en los tres niveles--. Se paga en cada construccion a cambio
    // de que el PRIMER fallo sea legible.
    llvm_backend::LlvmOptimizer optimizer;
    llvm_backend::LlvmOptimizationObservation observation;
    std::string optimizationError;
    if (!optimizer.optimize(llvmModule, *targetMachine, optimizationLevel, &observation, optimizationError))
    {
        error << "nsc: internal error: LLVM optimization failed: " << optimizationError << '\n';
        return false;
    }
    return true;
}

bool emitCodeGenerationArtifact(
    std::string_view action,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    llvm_backend::LlvmModule& llvmModule,
    llvm_backend::LlvmTargetMachine& targetMachine,
    std::ostream& output,
    std::ostream& error)
{
    const llvm_backend::LlvmCodeGenFileType fileType = action == "--emit-obj"
                                                           ? llvm_backend::LlvmCodeGenFileType::Object
                                                           : llvm_backend::LlvmCodeGenFileType::Assembly;
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    std::string emissionError;
    if (!targetMachine.emit(llvmModule, destination, fileType, emissionError))
    {
        error << "nsc: error: target emission failed: " << emissionError << '\n';
        return false;
    }

    if (fileType == llvm_backend::LlvmCodeGenFileType::Object)
    {
        llvm_backend::CoffObjectVerifier verifier;
        if (!verifier.verify(destination))
        {
            error << "nsc: internal error: emitted COFF verification failed: " << verifier.error() << '\n';
            std::error_code removeError;
            std::filesystem::remove(destination, removeError);
            return false;
        }
        output << "COFF object emission: ok\n"
               << "machine: AMD64 (0x8664)\n"
               << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
               << "sections: " << verifier.info().sectionCount << '\n';
    }
    else
    {
        std::error_code sizeError;
        const std::uintmax_t size = std::filesystem::file_size(destination, sizeError);
        if (sizeError || size == 0)
        {
            error << "nsc: internal error: emitted assembly file is empty or "
                     "unreadable\n";
            return false;
        }
        output << "assembly emission: ok\n"
               << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
               << "bytes: " << size << '\n';
    }
    return true;
}

ExitCode Driver::runCodeGeneration(std::string_view action,
    std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::ostream &output,
    std::ostream &error)
{
    if (outputPath.empty())
    {
        error << "nsc: error: code generation output path is empty\n";
        return ExitCode::CommandLineError;
    }

    std::unique_ptr<llvm_backend::LlvmModule> llvmModule;
    if (!prepareCodeGenerationModule(inputPath, llvmModule, error))
        return ExitCode::CompilationError;

    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine;
    if (!optimizeCodeGenerationModule(*llvmModule, optimizationLevel, targetMachine, error))
        return ExitCode::CompilationError;

    if (!emitCodeGenerationArtifact(
            action,
            outputPath,
            optimizationLevel,
            *llvmModule,
            *targetMachine,
            output,
            error))
        return ExitCode::CompilationError;
    return ExitCode::Success;
}

ExitCode Driver::printLinker(std::ostream &output, std::ostream &error)
{
    std::string linkerError;
    std::unique_ptr<linker::LldLinker> linker = linker::LldLinker::discover(linkerError);
    if (linker == nullptr)
    {
        error << "nsc: error: " << linkerError << '\n';
        return ExitCode::CompilationError;
    }

    output << "linker: " << linker->executablePath().string() << '\n'
           << "librarian: "
           << (linker->librarianExecutablePath().empty() ? std::string("unavailable (set NSC_LLVM_LIB)")
                                                         : linker->librarianExecutablePath().string())
           << '\n'
           << "flavor: Windows COFF/PE\n"
           << "target: x86_64-pc-windows-msvc\n"
           << "subsystem: console\n";
    return ExitCode::Success;
}

ExitCode Driver::dumpRuntimeLlvm(std::ostream &output, std::ostream &error)
{
    runtime::WindowsRuntimeBuilder builder;
    std::string runtimeError;
    std::unique_ptr<llvm_backend::LlvmModule> module = builder.build(runtimeError);
    if (module == nullptr)
    {
        error << "nsc: internal error: runtime construction failed: " << runtimeError << '\n';
        return ExitCode::CompilationError;
    }

    output << module->text();
    return ExitCode::Success;
}

ExitCode Driver::runExecutableGeneration(std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    linker::WindowsExecutableSubsystem subsystem,
    std::ostream &output,
    std::ostream &error)
{
    if (outputPath.empty())
    {
        error << "nsc: error: executable output path is empty\n";
        return ExitCode::CommandLineError;
    }

    frontend::FrontendCompilation compilation;
    std::string loadError;
    const bool success = compilation.compileFile(std::filesystem::path(inputPath), loadError, true);
    if (!loadError.empty())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    if (!success || compilation.optimizationModule() == nullptr || compilation.semanticAnalyzer() == nullptr)
    {
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        error << "nsc: error: executable generation requires a valid int Main() "
                 "program\n";
        return ExitCode::CompilationError;
    }

    llvm_backend::LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
    std::unique_ptr<llvm_backend::LlvmModule> module = lowerer.lower(compilation.optimizationModule());
    if (module == nullptr)
    {
        error << "nsc: internal error: LLVM lowering failed: " << lowerer.error() << '\n';
        return ExitCode::CompilationError;
    }

    std::string verificationError;
    if (!module->verify(verificationError))
    {
        error << "nsc: internal error: LLVM verification failed: " << verificationError << '\n';
        return ExitCode::CompilationError;
    }

    linker::WindowsExecutableBuilder builder;
    builder.setRuntimeLibrary(linker::configuredWindowsRuntimeLibrary());
    builder.setSubsystem(subsystem);
    std::string buildError;
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    if (!builder.build(*module, destination, optimizationLevel, buildError))
    {
        error << "nsc: error: Windows executable generation failed: " << buildError << '\n';
        return ExitCode::CompilationError;
    }

    output << "PE executable emission: ok\n"
           << "machine: AMD64 (0x8664)\n"
           << "format: PE32+\n"
           << "subsystem: " << subsystemDisplayName(subsystem) << '\n'
           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "sections: " << builder.info().sectionCount << '\n'
           << "entry-rva: " << builder.info().entryPointRva << '\n'
           << "imports:";
    for (const std::string &library : builder.info().importedLibraries)
    {
        output << ' ' << library;
    }
    output << '\n';
    return ExitCode::Success;
}

void Driver::printVersion(std::ostream &output)
{
    printCompilerBanner(output);
}

void Driver::printCompilerIdentity(std::ostream &output)
{
    output << "NSCOMPILER 1\n"
           << "compiler_version=" << kCompilerVersion << '\n'
           << "compiler_release=" << kCompilerReleaseVersion << '\n'
           << "language=" << kLanguageVersion << '\n'
           << "stage=" << kCompilerStage << '\n'
           << "build=" << kCompilerBuildId << '\n';
}

void Driver::printTarget(std::ostream &output)
{
    const target::TargetInfo targetInfo = target::TargetInfo::WindowsX64();
    output << "architecture: " << targetInfo.architectureName() << '\n'
           << "os:           " << targetInfo.operatingSystemName() << '\n'
           << "object:       " << targetInfo.objectFormatName() << '\n'
           << "abi:          " << targetInfo.abiName() << '\n'
           << "llvm-triple:  " << targetInfo.llvmTriple() << '\n';
}

} // namespace nativesharp::driver
