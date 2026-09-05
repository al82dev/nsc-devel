#include <algorithm>
#include "nativesharp/Driver/Driver.h"

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
#include "nativesharp/Frontend/ProjectGenericModelDumper.h"
#include "nativesharp/Frontend/ProjectNameLookupDumper.h"
#include "nativesharp/Frontend/ProjectMemberModelDumper.h"
#include "nativesharp/Frontend/ProjectMethodBodyModelDumper.h"
#include "nativesharp/Frontend/ProjectInheritanceModelDumper.h"
#include "nativesharp/Frontend/ProjectInterfaceModelDumper.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModelDumper.h"
#include "nativesharp/Frontend/ProjectDestructionModelDumper.h"
#include "nativesharp/Frontend/ProjectConstructionModelDumper.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModelDumper.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/HIR/HirDumper.h"
#include "nativesharp/HIR/HirVerifier.h"
#include "nativesharp/Lexer/TokenDumper.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmLowerer.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/WindowsExecutableBuilder.h"
#include "nativesharp/Linker/WindowsLibraryBuilder.h"
#include "nativesharp/Interop/CHeaderGenerator.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/MIR/MirDumper.h"
#include "nativesharp/MIR/MirVerifier.h"
#include "nativesharp/Passes/OptimizationDumper.h"
#include "nativesharp/Passes/OptimizationVerifier.h"
#include "nativesharp/Passes/PassPipelineDumper.h"
#include "nativesharp/Sema/SemanticDumper.h"
#include "nativesharp/Sema/SymbolDumper.h"
#include "nativesharp/SSA/SsaDumper.h"
#include "nativesharp/SSA/SsaVerifier.h"
#include "nativesharp/Source/SourceFile.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"
#include "nativesharp/Target/TargetDataLayout.h"
#include "nativesharp/Target/TargetInfo.h"

#include <array>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace nativesharp::driver
{
namespace
{
using target::BuiltinLayoutKind;

struct LayoutRow
{
    std::string_view name;
    BuiltinLayoutKind kind;
};

constexpr std::array<LayoutRow, 15> kLayoutRows = {{
    {"bool", BuiltinLayoutKind::Bool},
    {"byte", BuiltinLayoutKind::Byte},
    {"char", BuiltinLayoutKind::Char},
    {"short", BuiltinLayoutKind::Short},
    {"ushort", BuiltinLayoutKind::UShort},
    {"int", BuiltinLayoutKind::Int},
    {"uint", BuiltinLayoutKind::UInt},
    {"long", BuiltinLayoutKind::Long},
    {"ulong", BuiltinLayoutKind::ULong},
    {"nint", BuiltinLayoutKind::NInt},
    {"nuint", BuiltinLayoutKind::NUInt},
    {"float", BuiltinLayoutKind::Float},
    {"double", BuiltinLayoutKind::Double},
    {"enum", BuiltinLayoutKind::Enum},
    {"pointer", BuiltinLayoutKind::Pointer}
}};
}

ExitCode Driver::run(
    std::span<const std::string_view> arguments,
    std::ostream& output,
    std::ostream& error) const
{
    if (arguments.empty())
    {
        printHelp(output);
        return ExitCode::Success;
    }

    llvm_backend::LlvmOptimizationLevel optimizationLevel =
        llvm_backend::LlvmOptimizationLevel::O0;
    bool optimizationLevelSeen = false;
    std::vector<std::string_view> normalizedArguments;
    normalizedArguments.reserve(arguments.size());
    // Interop Foundation: external .lib inputs for the link. Extracted here (like -O) so every
    // positional action keeps its exact shape; consumed by the exe/dll emission paths.
    std::vector<std::filesystem::path> extraLinkLibraries;

    for (std::size_t argumentIndex = 0; argumentIndex < arguments.size(); ++argumentIndex)
    {
        const std::string_view argument = arguments[argumentIndex];
        if (argument == "--link-lib")
        {
            if (argumentIndex + 1 >= arguments.size())
            {
                error << "nsc: error: --link-lib requires a library file path\n";
                return ExitCode::CommandLineError;
            }
            extraLinkLibraries.emplace_back(std::string(arguments[++argumentIndex]));
            continue;
        }
        if (argument.size() == 3 && argument[0] == '-' && argument[1] == 'O')
        {
            if (optimizationLevelSeen)
            {
                error << "nsc: error: duplicate LLVM optimization level\n";
                return ExitCode::CommandLineError;
            }
            optimizationLevelSeen = true;
            switch (argument[2])
            {
                case '0':
                    optimizationLevel = llvm_backend::LlvmOptimizationLevel::O0;
                    break;
                case '1':
                    optimizationLevel = llvm_backend::LlvmOptimizationLevel::O1;
                    break;
                case '2':
                    optimizationLevel = llvm_backend::LlvmOptimizationLevel::O2;
                    break;
                case '3':
                    optimizationLevel = llvm_backend::LlvmOptimizationLevel::O3;
                    break;
                default:
                    error << "nsc: error: unsupported LLVM optimization level '" << argument << "'\n";
                    return ExitCode::CommandLineError;
            }
            continue;
        }
        if (argument.size() >= 2 && argument[0] == '-' && argument[1] == 'O')
        {
            error << "nsc: error: unsupported LLVM optimization level '" << argument << "'\n";
            return ExitCode::CommandLineError;
        }
        normalizedArguments.push_back(argument);
    }

    if (normalizedArguments.empty())
    {
        error << "nsc: error: optimization level requires a compilation action or input file\n";
        return ExitCode::CommandLineError;
    }

    const std::span<const std::string_view> args(normalizedArguments);

    if (args.size() == 1)
    {
        const std::string_view argument = args.front();
        if (argument == "--help" || argument == "-h")
        {
            printHelp(output);
            return ExitCode::Success;
        }
        if (argument == "--version")
        {
            printVersion(output);
            return ExitCode::Success;
        }
        if (argument == "--print-target")
        {
            printTarget(output);
            return ExitCode::Success;
        }
        if (argument == "--print-data-layout")
        {
            printDataLayout(output);
            return ExitCode::Success;
        }
        if (argument == "--print-codegen")
        {
            return printCodeGeneration(optimizationLevel, output, error);
        }
        if (argument == "--print-llvm-pipeline")
        {
            printLlvmPipeline(optimizationLevel, output);
            return ExitCode::Success;
        }
        if (argument == "--print-linker")
        {
            return printLinker(output, error);
        }
        if (argument == "--dump-runtime-llvm")
        {
            return dumpRuntimeLlvm(output, error);
        }
        if (!argument.empty() && argument.front() == '-')
        {
            error << "nsc: error: unknown option '" << argument << "'\n";
            return ExitCode::CommandLineError;
        }
        return runFrontend("--program", argument, optimizationLevel, output, error);
    }

    if (args.size() == 2)
    {
        const std::string_view action = args[0];
        if (action == "--verify-library-metadata")
        {
            return verifyLibraryMetadata(args[1], output, error);
        }
        if (action == "--check-project" || action == "--dump-project" ||
            action == "--check-project-graph" || action == "--dump-project-graph" ||
            action == "--check-project-syntax" || action == "--dump-project-compilation" ||
            action == "--check-project-generics" || action == "--dump-project-generics" ||
            action == "--check-project-names" || action == "--dump-project-names" ||
            action == "--check-project-members" || action == "--dump-project-members" ||
            action == "--check-project-inheritance" || action == "--dump-project-inheritance" ||
            action == "--check-project-interfaces" || action == "--dump-project-interfaces" ||
            action == "--check-project-generic-constraints" ||
            action == "--check-project-method-bodies" || action == "--dump-project-method-bodies" ||
            action == "--check-project-virtual-dispatch" || action == "--dump-project-virtual-dispatch" ||
            action == "--check-project-destruction" || action == "--dump-project-destruction" ||
            action == "--check-project-constructions" || action == "--dump-project-constructions" ||
            action == "--check-project-layouts" || action == "--dump-project-layouts" ||
            action == "--check-project-oop-hir" || action == "--dump-project-oop-hir" ||
            action == "--check-project-oop-mir" || action == "--dump-project-oop-mir" ||
            action == "--verify-project-oop-llvm" || action == "--dump-project-oop-llvm")
        {
            return runProject(action, args[1], output, error);
        }
        if (action == "--check" || action == "--dump-source" || action == "--dump-tokens" ||
            action == "--dump-ast" || action == "--dump-symbols" || action == "--dump-sema" ||
            action == "--dump-hir" || action == "--verify-hir" || action == "--dump-mir" ||
            action == "--verify-mir" || action == "--dump-cfg" || action == "--verify-cfg" ||
            action == "--dump-dominance" || action == "--verify-dominance" ||
            action == "--dump-ssa" || action == "--verify-ssa" ||
            action == "--dump-opt" || action == "--verify-opt" ||
            action == "--dump-pass-pipeline" || action == "--dump-llvm" ||
            action == "--verify-llvm" || action == "--dump-llvm-opt" ||
            action == "--verify-llvm-opt")
        {
            return runFrontend(action, args[1], optimizationLevel, output, error);
        }
    }

    if (args.size() == 3 && args[1] == "-o" &&
        !args[0].empty() && args[0].front() != '-')
    {
        return runExecutableGeneration(args[0], args[2], optimizationLevel, output, error);
    }

    if (args.size() == 4 && args[2] == "-o")
    {
        const std::string_view action = args[0];
        if (action == "--emit-obj" || action == "--emit-asm")
        {
            return runCodeGeneration(action, args[1], args[3], optimizationLevel, output, error);
        }
        if (action == "--emit-project-oop-obj")
        {
            return runProjectOopCodeGeneration(args[1], args[3], optimizationLevel, output, error);
        }
        if (action == "--emit-project-oop-exe")
        {
            return runProjectOopExecutableGeneration(args[1], args[3], optimizationLevel, extraLinkLibraries, output, error);
        }
        if (action == "--emit-project-oop-lib")
        {
            return runProjectOopStaticLibraryGeneration(args[1], args[3], optimizationLevel, output, error);
        }
        if (action == "--emit-project-oop-dll")
        {
            return runProjectOopDynamicLibraryGeneration(args[1], args[3], optimizationLevel, extraLinkLibraries, output, error);
        }
        if (action == "--emit-project-host-header")
        {
            return runProjectHostHeaderGeneration(args[1], args[3], output, error);
        }
        if (action == "--emit-project-library-metadata")
        {
            return runProjectLibraryMetadataGeneration(args[1], args[3], output, error);
        }
        if (action == "--emit-exe")
        {
            return runExecutableGeneration(args[1], args[3], optimizationLevel, output, error);
        }
    }

    error << "nsc: error: invalid command line; use --help for supported options (stage "
          << kCompilerStage << ")\n";
    return ExitCode::CommandLineError;
}

void Driver::printHelp(std::ostream& output)
{
    output
        << "NativeSharp compiler (NativeSharp " << kLanguageVersion << ", stage " << kCompilerStage << ")\n\n"
        << "Usage:\n"
        << "  nsc --check-project <file.nsproj>\n"
        << "  nsc --dump-project <file.nsproj>\n"
        << "  nsc --check-project-graph <file.nsproj>\n"
        << "  nsc --dump-project-graph <file.nsproj>\n"
        << "  nsc --check-project-syntax <file.nsproj>\n"
        << "  nsc --dump-project-compilation <file.nsproj>\n"
        << "  nsc --check-project-generics <file.nsproj>\n"
        << "  nsc --dump-project-generics <file.nsproj>\n"
        << "  nsc --check-project-names <file.nsproj>\n"
        << "  nsc --dump-project-names <file.nsproj>\n"
        << "  nsc --check-project-members <file.nsproj>\n"
        << "  nsc --dump-project-members <file.nsproj>\n"
        << "  nsc --check-project-inheritance <file.nsproj>\n"
        << "  nsc --dump-project-inheritance <file.nsproj>\n"
        << "  nsc --check-project-interfaces <file.nsproj>\n"
        << "  nsc --check-project-generic-constraints <file.nsproj>\n"
        << "  nsc --dump-project-interfaces <file.nsproj>\n"
        << "  nsc --check-project-method-bodies <file.nsproj>\n"
        << "  nsc --dump-project-method-bodies <file.nsproj>\n"
        << "  nsc --check-project-virtual-dispatch <file.nsproj>\n"
        << "  nsc --dump-project-virtual-dispatch <file.nsproj>\n"
        << "  nsc --check-project-destruction <file.nsproj>\n"
        << "  nsc --dump-project-destruction <file.nsproj>\n"
        << "  nsc --check-project-constructions <file.nsproj>\n"
        << "  nsc --dump-project-constructions <file.nsproj>\n"
        << "  nsc --check-project-layouts <file.nsproj>\n"
        << "  nsc --dump-project-layouts <file.nsproj>\n"
        << "  nsc --check-project-oop-hir <file.nsproj>\n"
        << "  nsc --dump-project-oop-hir <file.nsproj>\n"
        << "  nsc --check-project-oop-mir <file.nsproj>\n"
        << "  nsc --dump-project-oop-mir <file.nsproj>\n"
        << "  nsc --verify-project-oop-llvm <file.nsproj>\n"
        << "  nsc --dump-project-oop-llvm <file.nsproj>\n"
        << "  nsc --emit-project-oop-obj <file.nsproj> [-O0|-O1|-O2|-O3] -o <file.obj>\n"
        << "  nsc --emit-project-oop-exe <file.nsproj> [-O0|-O1|-O2|-O3] -o <file.exe>\n"
        << "  nsc --emit-project-oop-lib <file.nsproj> [-O0|-O1|-O2|-O3] -o <file.lib>\n"
        << "  nsc --emit-project-oop-dll <file.nsproj> [-O0|-O1|-O2|-O3] -o <file.dll>\n"
        << "  nsc --emit-project-host-header <file.nsproj> -o <file.h>\n"
        << "  nsc --emit-project-library-metadata <file.nsproj> -o <file.nsmeta>\n"
        << "  nsc --verify-library-metadata <file.nsmeta>\n"
        << "  nsc <file.ns>\n"
        << "  nsc --check <file.ns>\n"
        << "  nsc --dump-source <file.ns>\n"
        << "  nsc --dump-tokens <file.ns>\n"
        << "  nsc --dump-ast <file.ns>\n"
        << "  nsc --dump-symbols <file.ns>\n"
        << "  nsc --dump-sema <file.ns>\n"
        << "  nsc --dump-hir <file.ns>\n"
        << "  nsc --verify-hir <file.ns>\n"
        << "  nsc --dump-mir <file.ns>\n"
        << "  nsc --verify-mir <file.ns>\n"
        << "  nsc --dump-cfg <file.ns>\n"
        << "  nsc --verify-cfg <file.ns>\n"
        << "  nsc --dump-dominance <file.ns>\n"
        << "  nsc --verify-dominance <file.ns>\n"
        << "  nsc --dump-ssa <file.ns>\n"
        << "  nsc --verify-ssa <file.ns>\n"
        << "  nsc --dump-opt <file.ns>\n"
        << "  nsc --verify-opt <file.ns>\n"
        << "  nsc --dump-pass-pipeline <file.ns>\n"
        << "  nsc --dump-llvm <file.ns>\n"
        << "  nsc --verify-llvm <file.ns>\n"
        << "  nsc --dump-llvm-opt <file.ns> [-O0|-O1|-O2|-O3]\n"
        << "  nsc --verify-llvm-opt <file.ns> [-O0|-O1|-O2|-O3]\n"
        << "  nsc --emit-obj <file.ns> [-O0|-O1|-O2|-O3] -o <file.obj>\n"
        << "  nsc --emit-asm <file.ns> [-O0|-O1|-O2|-O3] -o <file.asm>\n"
        << "  nsc --emit-exe <file.ns> [-O0|-O1|-O2|-O3] -o <file.exe>\n"
        << "  nsc <file.ns> [-O0|-O1|-O2|-O3] -o <file.exe>\n"
        << "  nsc --print-codegen [-O0|-O1|-O2|-O3]\n"
        << "  nsc --print-llvm-pipeline [-O0|-O1|-O2|-O3]\n"
        << "  nsc --print-linker\n"
        << "  nsc --dump-runtime-llvm\n"
        << "  nsc --print-target\n"
        << "  nsc --print-data-layout\n"
        << "  nsc --version\n\n"
        << "Optimization flags -O0, -O1, -O2 and -O3 may appear anywhere in a compilation command.\n"
        << "Stage 37 semantics remain preserved: static members, deterministic initialization, const and readonly.\n"
        << "Recovered Stage 35 adds abstract/sealed classes and methods plus descriptor-driven native destruction.\n"
        << "Stage 36 interfaces, Stage 38 properties/indexers/operators, and Stage 39 native RTTI remain preserved. "
        << "Stage 40 adds native struct value ABI; Stage 41 arrays/strings/Core; Stage 42 delegates/events; "
        << "Stage 43 lambdas/closures; and Stage 44 AOT generics by monomorphization. "
        << "Stage 45 adds native Win64 exceptions, cleanup/finally/using, RTTI catches, and PE EH closure. "
        << "Stage 46 adds static COFF .lib and PE32+ DLL + import-library products; "
        << "Stage 47 adds stable Windows x64 export extern C host ABI thunks; "
        << "Stage 48.1 adds deterministic generated C/C++ host headers from the semantic ABI model; "
        << "Stage 48.2 adds library metadata (.nsmeta) with self-verification. "
        << "Stage 49 adds `raii` locals with scope-bound native destruction; "
        << "Stage 50 adds the ownership type system (owned/borrow parameters, returns and lifetimes). "
        << "Stage 51 completes the native string type; Stage 52 adds generic collections, `var` and "
        << "`foreach`. "
        << "Stage 53 adds the platform library: time, environment and environment variables, "
        << "filesystem paths and contents, command-line arguments, and directory enumeration. "
        << "Stage 54 adds threading: threads, atomics, mutexes, monitors, semaphores and events. "
        << "Stage 55 adds process, hardware and diagnostics support. "
        << "Stage 56 completes interop and the type system: C aggregates, FFI aliases, "
        << "ref/out/in, struct members, definite assignment and the Object type root. "
        << "Stage 57 completes the language surface: the conditional operator, do-while and "
        << "switch/case.\n";
}

ExitCode Driver::printCodeGeneration(
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::ostream& output,
    std::ostream& error)
{
    std::string targetError;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
        llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
    if (targetMachine == nullptr)
    {
        error << "nsc: internal error: target machine creation failed: " << targetError << '\n';
        return ExitCode::CompilationError;
    }

    output
        << "target-triple: " << targetMachine->triple() << '\n'
        << "data-layout:   " << targetMachine->dataLayout() << '\n'
        << "object-format: COFF\n"
        << "machine:       AMD64 (0x8664)\n"
        << "optimization:  " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n';
    return ExitCode::Success;
}

void Driver::printLlvmPipeline(
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::ostream& output)
{
    output
        << "optimization-level: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
        << "pass-pipeline: " << llvm_backend::llvmOptimizationPipelineName(optimizationLevel) << '\n'
        << "target-aware: yes\n"
        << "verify-each: yes\n";
}

ExitCode Driver::runCodeGeneration(
    std::string_view action,
    std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::ostream& output,
    std::ostream& error)
{
    if (outputPath.empty())
    {
        error << "nsc: error: code generation output path is empty\n";
        return ExitCode::CommandLineError;
    }

    frontend::FrontendCompilation compilation;
    std::string loadError;
    const bool success = compilation.compileFile(std::filesystem::path(inputPath), loadError, false);
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
        error << "nsc: error: code generation requires a successful optimized compilation\n";
        return ExitCode::CompilationError;
    }

    llvm_backend::LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
    std::unique_ptr<llvm_backend::LlvmModule> llvmModule = lowerer.lower(compilation.optimizationModule());
    if (llvmModule == nullptr)
    {
        error << "nsc: internal error: LLVM lowering failed: " << lowerer.error() << '\n';
        return ExitCode::CompilationError;
    }

    std::string verificationError;
    if (!llvmModule->verify(verificationError))
    {
        error << "nsc: internal error: LLVM verification failed: " << verificationError << '\n';
        return ExitCode::CompilationError;
    }

    std::string targetError;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
        llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
    if (targetMachine == nullptr)
    {
        error << "nsc: internal error: target machine creation failed: " << targetError << '\n';
        return ExitCode::CompilationError;
    }

    llvm_backend::LlvmOptimizer optimizer;
    std::string optimizationError;
    if (!optimizer.optimize(*llvmModule, *targetMachine, optimizationLevel, optimizationError))
    {
        error << "nsc: internal error: LLVM optimization failed: " << optimizationError << '\n';
        return ExitCode::CompilationError;
    }

    const llvm_backend::LlvmCodeGenFileType fileType =
        action == "--emit-obj"
            ? llvm_backend::LlvmCodeGenFileType::Object
            : llvm_backend::LlvmCodeGenFileType::Assembly;
    const std::filesystem::path destination(outputPath);
    std::string emissionError;
    if (!targetMachine->emit(*llvmModule, destination, fileType, emissionError))
    {
        error << "nsc: error: target emission failed: " << emissionError << '\n';
        return ExitCode::CompilationError;
    }

    if (fileType == llvm_backend::LlvmCodeGenFileType::Object)
    {
        llvm_backend::CoffObjectVerifier verifier;
        if (!verifier.verify(destination))
        {
            error << "nsc: internal error: emitted COFF verification failed: " << verifier.error() << '\n';
            std::error_code removeError;
            std::filesystem::remove(destination, removeError);
            return ExitCode::CompilationError;
        }
        output
            << "COFF object emission: ok\n"
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
            error << "nsc: internal error: emitted assembly file is empty or unreadable\n";
            return ExitCode::CompilationError;
        }
        output
            << "assembly emission: ok\n"
            << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
            << "bytes: " << size << '\n';
    }
    return ExitCode::Success;
}

ExitCode Driver::printLinker(std::ostream& output, std::ostream& error)
{
    std::string linkerError;
    std::unique_ptr<linker::LldLinker> linker = linker::LldLinker::discover(linkerError);
    if (linker == nullptr)
    {
        error << "nsc: error: " << linkerError << '\n';
        return ExitCode::CompilationError;
    }

    output
        << "linker: " << linker->executablePath().string() << '\n'
        << "flavor: lld-link / COFF\n"
        << "target: x86_64-pc-windows-msvc\n"
        << "subsystem: console\n";
    return ExitCode::Success;
}

ExitCode Driver::dumpRuntimeLlvm(std::ostream& output, std::ostream& error)
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

ExitCode Driver::runExecutableGeneration(
    std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::ostream& output,
    std::ostream& error)
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
        error << "nsc: error: executable generation requires a valid int Main() program\n";
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
    std::string buildError;
    const std::filesystem::path destination(outputPath);
    if (!builder.build(*module, destination, optimizationLevel, buildError))
    {
        error << "nsc: error: Windows executable generation failed: " << buildError << '\n';
        return ExitCode::CompilationError;
    }

    output
        << "PE executable emission: ok\n"
        << "machine: AMD64 (0x8664)\n"
        << "format: PE32+\n"
        << "subsystem: Windows CUI\n"
        << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
        << "sections: " << builder.info().sectionCount << '\n'
        << "entry-rva: " << builder.info().entryPointRva << '\n'
        << "imports:";
    for (const std::string& library : builder.info().importedLibraries)
    {
        output << ' ' << library;
    }
    output << '\n';
    return ExitCode::Success;
}

void Driver::printVersion(std::ostream& output)
{
    // CC1-000: the unique build id appears both in the SemVer build metadata of
    // `kCompilerVersion` and on its own machine-friendly line. A consumer can compare either one
    // without hashing nsc.exe.
    output
        << "nsc " << kCompilerVersion
        << " (NativeSharp " << kLanguageVersion
        << ", stage " << kCompilerStage << ")\n"
        << "build " << kCompilerBuildId << '\n';
}

void Driver::printTarget(std::ostream& output)
{
    const target::TargetInfo targetInfo = target::TargetInfo::WindowsX64();
    output
        << "architecture: " << targetInfo.architectureName() << '\n'
        << "os:           " << targetInfo.operatingSystemName() << '\n'
        << "object:       " << targetInfo.objectFormatName() << '\n'
        << "abi:          " << targetInfo.abiName() << '\n'
        << "llvm-triple:  " << targetInfo.llvmTriple() << '\n';
}

void Driver::printDataLayout(std::ostream& output)
{
    const target::TargetDataLayout dataLayout = target::TargetDataLayout::WindowsX64();
    output
        << "endianness: little\n"
        << "max-natural-alignment: " << dataLayout.maximumNaturalAlignment() << "\n"
        << "type size align\n";

    for (const LayoutRow& row : kLayoutRows)
    {
        const target::TypeLayout layout = dataLayout.layoutOf(row.kind);
        output << row.name << ' ' << layout.size << ' ' << layout.alignment << '\n';
    }
}

ExitCode Driver::runProject(
    std::string_view action,
    std::string_view path,
    std::ostream& output,
    std::ostream& error)
{
    if (action == "--check-project" || action == "--dump-project")
    {
        compilation::ProjectLoader loader;
        std::string loadError;
        const std::optional<compilation::Project> project =
            loader.load(std::filesystem::path(path), loadError);
        if (!project.has_value())
        {
            error << "nsc: error: " << loadError << '\n';
            return ExitCode::CompilationError;
        }

        if (action == "--dump-project")
        {
            compilation::ProjectDumper dumper;
            dumper.dump(*project, output);
            return ExitCode::Success;
        }

        const std::size_t sourceCount = project->sourceSet().sources().size();
        const std::size_t referenceCount = project->projectReferences().size();
        output
            << "project check: ok (" << sourceCount << " source"
            << (sourceCount == 1 ? "" : "s") << ", "
            << referenceCount << " project reference"
            << (referenceCount == 1 ? "" : "s") << ")\n";
        return ExitCode::Success;
    }

    if (action == "--check-project-graph" || action == "--dump-project-graph")
    {
        compilation::ProjectGraphLoader loader;
        std::string loadError;
        const std::optional<compilation::ProjectGraph> graph =
            loader.load(std::filesystem::path(path), loadError);
        if (!graph.has_value())
        {
            error << "nsc: error: " << loadError << '\n';
            return ExitCode::CompilationError;
        }

        if (action == "--dump-project-graph")
        {
            compilation::ProjectGraphDumper dumper;
            dumper.dump(*graph, output);
            return ExitCode::Success;
        }

        output << "project graph check: ok (" << graph->projects().size() << " project"
               << (graph->projects().size() == 1 ? "" : "s") << ")\n";
        return ExitCode::Success;
    }

    frontend::ProjectCompilation projectCompilation;
    std::string loadError;
    const bool oopLlvmAction =
        action == "--verify-project-oop-llvm" || action == "--dump-project-oop-llvm";
    const bool oopMirAction = oopLlvmAction ||
        action == "--check-project-oop-mir" || action == "--dump-project-oop-mir";
    const bool oopHirAction = oopMirAction ||
        action == "--check-project-oop-hir" || action == "--dump-project-oop-hir";
    const bool layoutAction = oopHirAction ||
        action == "--check-project-layouts" || action == "--dump-project-layouts";
    const bool constructionAction = layoutAction ||
        action == "--check-project-constructions" || action == "--dump-project-constructions";
    const bool destructionAction = constructionAction ||
        action == "--check-project-destruction" || action == "--dump-project-destruction";
    const bool virtualDispatchAction = destructionAction ||
        action == "--check-project-virtual-dispatch" || action == "--dump-project-virtual-dispatch";
    const bool methodBodyAction = virtualDispatchAction ||
        action == "--check-project-method-bodies" || action == "--dump-project-method-bodies";
    // NSC-LIMIT-007. FASE PROPIA, entre las interfaces y los cuerpos. Va DEBAJO de methodBodyAction
    // --toda fase posterior la arrastra-- y ENCIMA de interfaceAction, que es lo ultimo que
    // necesita. Antes se sumaba a `methodBodyAction`, y entonces pedir la comprobacion de
    // constraints analizaba tambien todos los cuerpos: un rojo podia venir de un error que no
    // tenia nada que ver con un `where`.
    const bool genericConstraintAction = methodBodyAction ||
        action == "--check-project-generic-constraints";
    const bool interfaceAction = genericConstraintAction ||
        action == "--check-project-interfaces" || action == "--dump-project-interfaces";
    const bool inheritanceAction = interfaceAction ||
        action == "--check-project-inheritance" || action == "--dump-project-inheritance";
    const bool memberBindingAction = inheritanceAction ||
        action == "--check-project-members" || action == "--dump-project-members";
    const bool nameBindingAction = memberBindingAction ||
        action == "--check-project-names" || action == "--dump-project-names";
    const bool genericAction = nameBindingAction ||
        action == "--check-project-generics" || action == "--dump-project-generics";
    const bool success = oopMirAction
        ? projectCompilation.compileProjectOopMir(std::filesystem::path(path), loadError)
        : oopHirAction
            ? projectCompilation.compileProjectOopHir(std::filesystem::path(path), loadError)
        : layoutAction
            ? projectCompilation.compileProjectObjectLayouts(std::filesystem::path(path), loadError)
        : constructionAction
            ? projectCompilation.compileProjectConstructions(std::filesystem::path(path), loadError)
        : destructionAction
            ? projectCompilation.compileProjectDestruction(std::filesystem::path(path), loadError)
        : virtualDispatchAction
            ? projectCompilation.compileProjectVirtualDispatch(std::filesystem::path(path), loadError)
        : methodBodyAction
            ? projectCompilation.compileProjectMethodBodies(std::filesystem::path(path), loadError)
        : genericConstraintAction
            ? projectCompilation.compileProjectGenericConstraints(
                  std::filesystem::path(path), loadError)
        : interfaceAction
            ? projectCompilation.compileProjectInterfaces(std::filesystem::path(path), loadError)
        : inheritanceAction
            ? projectCompilation.compileProjectInheritance(std::filesystem::path(path), loadError)
        : memberBindingAction
            ? projectCompilation.compileProjectMembers(std::filesystem::path(path), loadError)
            : nameBindingAction
                ? projectCompilation.compileProjectNames(std::filesystem::path(path), loadError)
                : genericAction
                    ? projectCompilation.compileProjectGenerics(std::filesystem::path(path), loadError)
                    : projectCompilation.compileProjectSyntax(std::filesystem::path(path), loadError);
    if (!loadError.empty())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }

    if (action == "--dump-project-compilation")
    {
        frontend::ProjectCompilationDumper dumper;
        dumper.dump(projectCompilation, output);
    }

    if (action == "--dump-project-generics" && projectCompilation.genericModel() != nullptr)
    {
        output << frontend::ProjectGenericModelDumper::dump(*projectCompilation.genericModel());
    }

    if (action == "--dump-project-names" && projectCompilation.nameLookup() != nullptr)
    {
        frontend::ProjectNameLookupDumper dumper;
        dumper.dump(*projectCompilation.nameLookup(), output);
    }

    if (action == "--dump-project-members" &&
        projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr)
    {
        frontend::ProjectMemberModelDumper dumper;
        dumper.dump(
            *projectCompilation.memberModel(),
            *projectCompilation.nameLookup(),
            output);
    }

    if (action == "--dump-project-inheritance" &&
        projectCompilation.nameLookup() != nullptr &&
        projectCompilation.inheritanceModel() != nullptr)
    {
        frontend::ProjectInheritanceModelDumper dumper;
        dumper.dump(*projectCompilation.inheritanceModel(), *projectCompilation.nameLookup(), output);
    }

    if (action == "--dump-project-interfaces" &&
        projectCompilation.interfaceModel() != nullptr)
    {
        frontend::ProjectInterfaceModelDumper::dump(
            *projectCompilation.interfaceModel(), output);
    }

    if (action == "--dump-project-virtual-dispatch" &&
        projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr &&
        projectCompilation.virtualDispatchModel() != nullptr)
    {
        frontend::ProjectVirtualDispatchModelDumper dumper;
        dumper.dump(
            *projectCompilation.virtualDispatchModel(),
            *projectCompilation.memberModel(),
            *projectCompilation.nameLookup(),
            output);
    }

    if (action == "--dump-project-method-bodies" &&
        projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr &&
        projectCompilation.methodBodyModel() != nullptr)
    {
        frontend::ProjectMethodBodyModelDumper dumper;
        dumper.dump(
            *projectCompilation.methodBodyModel(),
            *projectCompilation.memberModel(),
            *projectCompilation.nameLookup(),
            output);
    }

    if (action == "--dump-project-destruction" &&
        projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr &&
        projectCompilation.destructionModel() != nullptr)
    {
        frontend::ProjectDestructionModelDumper::dump(
            *projectCompilation.destructionModel(),
            *projectCompilation.nameLookup(),
            *projectCompilation.memberModel(),
            output);
    }

    if (action == "--dump-project-constructions" &&
        projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr &&
        projectCompilation.constructionModel() != nullptr)
    {
        frontend::ProjectConstructionModelDumper dumper;
        dumper.dump(
            *projectCompilation.constructionModel(),
            *projectCompilation.memberModel(),
            *projectCompilation.nameLookup(),
            output);
    }

    if (action == "--dump-project-layouts" &&
        projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr &&
        projectCompilation.objectLayoutModel() != nullptr)
    {
        frontend::ProjectObjectLayoutModelDumper dumper;
        dumper.dump(
            *projectCompilation.objectLayoutModel(),
            *projectCompilation.memberModel(),
            *projectCompilation.nameLookup(),
            output);
    }

    if (action == "--dump-project-oop-hir" && projectCompilation.oopHirModel() != nullptr)
    {
        output << frontend::ProjectOopHirModelDumper::dump(*projectCompilation.oopHirModel());
    }

    if (action == "--dump-project-oop-mir" && projectCompilation.oopMirModel() != nullptr)
    {
        output << frontend::ProjectOopMirModelDumper::dump(*projectCompilation.oopMirModel());
    }

    std::unique_ptr<llvm_backend::LlvmModule> oopLlvmModule;
    if (oopLlvmAction && success &&
        projectCompilation.nameLookup() != nullptr &&
        projectCompilation.memberModel() != nullptr &&
        projectCompilation.objectLayoutModel() != nullptr &&
        projectCompilation.oopMirModel() != nullptr)
    {
        llvm_backend::ProjectOopLlvmLowerer lowerer;
        oopLlvmModule = lowerer.lower(
            *projectCompilation.nameLookup(),
            *projectCompilation.oopMirModel());
        if (oopLlvmModule == nullptr)
        {
            error << "nsc: error: " << lowerer.error() << '\n';
            return ExitCode::CompilationError;
        }
        if (action == "--dump-project-oop-llvm")
        {
            output << oopLlvmModule->text();
        }
    }

    if (projectCompilation.diagnostics().hasErrors())
    {
        diagnostics::DiagnosticRenderer renderer(projectCompilation.sourceManager());
        renderer.renderAll(projectCompilation.diagnostics(), error);
    }
    if (!success)
    {
        return ExitCode::CompilationError;
    }

    if (action == "--check-project-syntax")
    {
        output << "project syntax check: ok (" << projectCompilation.units().size() << " syntax unit"
               << (projectCompilation.units().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-generics" && projectCompilation.genericModel() != nullptr)
    {
        output << "project generic monomorphization check: ok ("
               << projectCompilation.genericModel()->templates().size() << " template"
               << (projectCompilation.genericModel()->templates().size() == 1 ? "" : "s")
               << ", " << projectCompilation.genericModel()->specializations().size() << " specialization"
               << (projectCompilation.genericModel()->specializations().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-names" && projectCompilation.nameLookup() != nullptr)
    {
        output << "project name lookup check: ok ("
               << projectCompilation.nameLookup()->namedTypes().size() << " named type"
               << (projectCompilation.nameLookup()->namedTypes().size() == 1 ? "" : "s")
               << ", " << projectCompilation.nameLookup()->bindings().size() << " type binding"
               << (projectCompilation.nameLookup()->bindings().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-members" &&
             projectCompilation.memberModel() != nullptr)
    {
        output << "project member binding check: ok ("
               << projectCompilation.memberModel()->members().size() << " member"
               << (projectCompilation.memberModel()->members().size() == 1 ? "" : "s")
               << ", " << projectCompilation.memberModel()->bodyBindings().size()
               << " body binding"
               << (projectCompilation.memberModel()->bodyBindings().size() == 1 ? "" : "s")
               << ")\n";
    }
    else if (action == "--check-project-inheritance" &&
             projectCompilation.inheritanceModel() != nullptr)
    {
        std::size_t edgeCount = 0;
        for (const frontend::ClassInheritanceInfo& info : projectCompilation.inheritanceModel()->classes())
        {
            edgeCount += info.baseTypeOrdinal.has_value() ? 1u : 0u;
        }
        output << "project inheritance check: ok ("
               << projectCompilation.inheritanceModel()->classes().size() << " class"
               << (projectCompilation.inheritanceModel()->classes().size() == 1 ? "" : "es")
               << ", " << edgeCount << " base edge" << (edgeCount == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-interfaces" &&
             projectCompilation.interfaceModel() != nullptr)
    {
        output << "project interface semantic check: ok ("
               << projectCompilation.interfaceModel()->interfaces().size() << " interface"
               << (projectCompilation.interfaceModel()->interfaces().size() == 1 ? "" : "s")
               << ", " << projectCompilation.interfaceModel()->tablePlans().size() << " class-interface table"
               << (projectCompilation.interfaceModel()->tablePlans().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-generic-constraints" &&
             projectCompilation.genericConstraintModel() != nullptr)
    {
        const std::size_t violations =
            projectCompilation.genericConstraintModel()->violations().size();
        output << "project generic constraint check: ok (" << violations << " violation"
               << (violations == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-virtual-dispatch" &&
             projectCompilation.virtualDispatchModel() != nullptr)
    {
        output << "project virtual dispatch check: ok ("
               << projectCompilation.virtualDispatchModel()->virtualMethods().size() << " virtual method"
               << (projectCompilation.virtualDispatchModel()->virtualMethods().size() == 1 ? "" : "s")
               << ", " << projectCompilation.virtualDispatchModel()->classTables().size() << " class table"
               << (projectCompilation.virtualDispatchModel()->classTables().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-method-bodies" &&
             projectCompilation.methodBodyModel() != nullptr)
    {
        output << "project method-body semantic check: ok ("
               << projectCompilation.methodBodyModel()->scopes().size() << " scope"
               << (projectCompilation.methodBodyModel()->scopes().size() == 1 ? "" : "s")
               << ", " << projectCompilation.methodBodyModel()->locals().size() << " local"
               << (projectCompilation.methodBodyModel()->locals().size() == 1 ? "" : "s")
               << ", " << projectCompilation.methodBodyModel()->nameBindings().size() << " name binding"
               << (projectCompilation.methodBodyModel()->nameBindings().size() == 1 ? "" : "s")
               << ", " << projectCompilation.methodBodyModel()->calls().size() << " call"
               << (projectCompilation.methodBodyModel()->calls().size() == 1 ? "" : "s")
               << ")\n";
    }
    else if (action == "--check-project-destruction" &&
             projectCompilation.destructionModel() != nullptr)
    {
        std::size_t explicitDestructorCount = 0;
        for (const frontend::ClassDestructionPlan& plan : projectCompilation.destructionModel()->plans())
        {
            explicitDestructorCount += plan.explicitDestructorMemberOrdinal.has_value() ? 1u : 0u;
        }
        output << "project destruction semantic check: ok ("
               << projectCompilation.destructionModel()->plans().size() << " class plan"
               << (projectCompilation.destructionModel()->plans().size() == 1 ? "" : "s")
               << ", " << explicitDestructorCount << " explicit destructor"
               << (explicitDestructorCount == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-constructions" &&
             projectCompilation.constructionModel() != nullptr)
    {
        std::size_t explicitCount = 0;
        std::size_t implicitCount = 0;
        for (const frontend::ObjectConstructionBinding& construction :
             projectCompilation.constructionModel()->constructions())
        {
            explicitCount += construction.resolution == frontend::ConstructionResolutionKind::ResolvedExplicit ? 1u : 0u;
            implicitCount += construction.resolution == frontend::ConstructionResolutionKind::ResolvedImplicitDefault ? 1u : 0u;
        }
        output << "project construction semantic check: ok ("
               << projectCompilation.constructionModel()->constructions().size() << " construction"
               << (projectCompilation.constructionModel()->constructions().size() == 1 ? "" : "s")
               << ", " << explicitCount << " explicit, " << implicitCount << " implicit-default)\n";
    }
    else if (action == "--check-project-layouts" &&
             projectCompilation.objectLayoutModel() != nullptr)
    {
        std::size_t fieldCount = 0;
        for (const frontend::ClassObjectLayout& layout : projectCompilation.objectLayoutModel()->classLayouts())
        {
            fieldCount += layout.fields.size();
        }
        output << "project object layout check: ok ("
               << projectCompilation.objectLayoutModel()->classLayouts().size() << " class layout"
               << (projectCompilation.objectLayoutModel()->classLayouts().size() == 1 ? "" : "s")
               << ", " << fieldCount << " field layout"
               << (fieldCount == 1 ? "" : "s")
               << ", " << projectCompilation.objectLayoutModel()->allocationPlans().size() << " allocation plan"
               << (projectCompilation.objectLayoutModel()->allocationPlans().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-oop-hir" && projectCompilation.oopHirModel() != nullptr)
    {
        output << "project OOP HIR check: ok (" << projectCompilation.oopHirModel()->functions().size()
               << " callable" << (projectCompilation.oopHirModel()->functions().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--check-project-oop-mir" && projectCompilation.oopMirModel() != nullptr)
    {
        output << "project OOP MIR check: ok (" << projectCompilation.oopMirModel()->functions().size()
               << " callable" << (projectCompilation.oopMirModel()->functions().size() == 1 ? "" : "s") << ")\n";
    }
    else if (action == "--verify-project-oop-llvm" && oopLlvmModule != nullptr)
    {
        output << "project OOP LLVM verification: ok (" << projectCompilation.oopMirModel()->functions().size()
               << " lowered callable" << (projectCompilation.oopMirModel()->functions().size() == 1 ? "" : "s") << ")\n";
    }
    return ExitCode::Success;
}

ExitCode Driver::runProjectOopCodeGeneration(
    std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::ostream& output,
    std::ostream& error)
{
    frontend::ProjectCompilation compilation;
    std::string loadError;
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty())
        {
            error << "nsc: error: " << loadError << '\n';
        }
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }
    if (compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr ||
        compilation.objectLayoutModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 44 OOP project compilation did not produce required lowering models\n";
        return ExitCode::CompilationError;
    }

    llvm_backend::ProjectOopLlvmLowerer lowerer;
    std::unique_ptr<llvm_backend::LlvmModule> module = lowerer.lower(
        *compilation.nameLookup(), *compilation.oopMirModel());
    if (module == nullptr)
    {
        error << "nsc: error: " << lowerer.error() << '\n';
        return ExitCode::CompilationError;
    }

    std::string targetError;
    std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
        llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
    if (targetMachine == nullptr || !targetMachine->configureModuleForTarget(*module, targetError))
    {
        error << "nsc: error: " << targetError << '\n';
        return ExitCode::CompilationError;
    }

    llvm_backend::LlvmOptimizer optimizer;
    if (!optimizer.optimize(*module, *targetMachine, optimizationLevel, targetError))
    {
        error << "nsc: error: " << targetError << '\n';
        return ExitCode::CompilationError;
    }

    const std::filesystem::path destination(outputPath);
    if (!targetMachine->emit(*module, destination, llvm_backend::LlvmCodeGenFileType::Object, targetError))
    {
        error << "nsc: error: " << targetError << '\n';
        return ExitCode::CompilationError;
    }
    llvm_backend::CoffObjectVerifier verifier;
    if (!verifier.verify(destination))
    {
        error << "nsc: error: Stage 44 emitted invalid COFF object: " << verifier.error() << '\n';
        return ExitCode::CompilationError;
    }
    output << "project OOP object emission: ok (machine=0x" << std::hex << verifier.info().machine
           << std::dec << ", sections=" << verifier.info().sectionCount << ")\n";
    return ExitCode::Success;
}

namespace
{
// Interop Foundation: gather the project's `extern C from "<dll>"` declarations as (dll -> its
// symbols), in first-appearance order, for the linker's import-library step. The compiler learned
// no API by name at any point -- this is purely the shape the frontend recorded.
std::vector<linker::ExternalImportModule> collectExternalImports(
    const frontend::ProjectOopMirModel& mirModel)
{
    std::vector<linker::ExternalImportModule> modules;
    bool anyRuntimeBoundImport = false;
    const auto addSymbol = [&modules](const std::string& dllName, const std::string& symbol)
    {
        linker::ExternalImportModule* found = nullptr;
        for (linker::ExternalImportModule& module : modules)
        {
            if (module.dllName == dllName) { found = &module; break; }
        }
        if (found == nullptr)
        {
            modules.push_back(linker::ExternalImportModule{dllName, {}});
            found = &modules.back();
        }
        if (std::find(found->symbols.begin(), found->symbols.end(), symbol) == found->symbols.end())
        {
            found->symbols.push_back(symbol);
        }
    };
    for (const frontend::ProjectOopMirFunction& function : mirModel.functions())
    {
        if (!function.isImportedC || function.importDllName.empty()) continue;
        // C1.2 "import identity": an import whose C symbol name is shared by two libraries has NO
        // import-library entry at all -- there is exactly one slot per symbol name in a PE import
        // table and the two would fight over it. The backend binds those at first call instead.
        if (function.importRequiresRuntimeBinding)
        {
            anyRuntimeBoundImport = true;
            continue;
        }
        // The symbol that goes into the import library is the EXTERNAL one (`cAbiName`), never the
        // internal mangled name -- they coincide for a statically resolved import, and this is the
        // line that keeps them from being confused if they ever stop coinciding.
        addSymbol(function.importDllName, function.cAbiName);
    }
    if (anyRuntimeBoundImport)
    {
        // The two entry points every runtime-bound import needs, requested through the SAME door
        // the project's own imports use, so the runtime's single import table merges and
        // deduplicates them (a project that never needs runtime binding never imports them).
        addSymbol("KERNEL32.dll", "LoadLibraryA");
        addSymbol("KERNEL32.dll", "GetProcAddress");
    }
    return modules;
}
}

ExitCode Driver::runProjectOopExecutableGeneration(
    std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    const std::vector<std::filesystem::path>& extraLinkLibraries,
    std::ostream& output,
    std::ostream& error)
{
    frontend::ProjectCompilation compilation;
    std::string loadError;
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty())
        {
            error << "nsc: error: " << loadError << '\n';
        }
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }
    if (compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr ||
        compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 45.12 OOP executable compilation did not produce required lowering models\n";
        return ExitCode::CompilationError;
    }

    const frontend::ProjectOopMirFunction* entryFunction = nullptr;
    const auto members = compilation.memberModel()->members();
    for (const frontend::ProjectOopMirFunction& function : compilation.oopMirModel()->functions())
    {
        if (function.isLambda || function.constructor || function.destructor ||
            function.memberOrdinal >= members.size())
        {
            continue;
        }
        const frontend::ClassMemberSymbol& member = members[function.memberOrdinal];
        if (member.kind != frontend::ClassMemberKind::Method || member.name != "Main" ||
            !member.isStatic || !member.parameters.empty() || member.declaredTypeKey != "builtin:int")
        {
            continue;
        }
        if (entryFunction != nullptr)
        {
            error << "nsc: error: OOP executable has multiple static int Main() entry points\n";
            return ExitCode::CompilationError;
        }
        entryFunction = &function;
    }
    if (entryFunction == nullptr)
    {
        error << "nsc: error: OOP executable requires exactly one static int Main() method with no parameters\n";
        return ExitCode::CompilationError;
    }

    llvm_backend::ProjectOopLlvmLowerer lowerer;
    std::unique_ptr<llvm_backend::LlvmModule> module = lowerer.lower(
        *compilation.nameLookup(), *compilation.oopMirModel());
    if (module == nullptr)
    {
        error << "nsc: error: " << lowerer.error() << '\n';
        return ExitCode::CompilationError;
    }

    std::string verificationError;
    if (!module->verify(verificationError))
    {
        error << "nsc: error: OOP LLVM verification failed: " << verificationError << '\n';
        return ExitCode::CompilationError;
    }

    linker::WindowsExecutableBuilder builder;
    builder.setExternalImports(collectExternalImports(*compilation.oopMirModel()));
    builder.setExtraLinkLibraries(extraLinkLibraries);
    std::string buildError;
    const std::filesystem::path destination(outputPath);
    if (!builder.build(
            *module,
            destination,
            optimizationLevel,
            entryFunction->mangledName,
            buildError))
    {
        error << "nsc: error: OOP Windows executable generation failed: " << buildError << '\n';
        return ExitCode::CompilationError;
    }

    output
        << "project OOP PE executable emission: ok\n"
        << "entry-symbol: " << entryFunction->mangledName << '\n'
        << "machine: AMD64 (0x8664)\n"
        << "format: PE32+\n"
        << "subsystem: Windows CUI\n"
        << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
        << "sections: " << builder.info().sectionCount << '\n'
        << "entry-rva: " << builder.info().entryPointRva << '\n'
        << "exception-directory-rva: " << builder.info().exceptionDirectoryRva << '\n'
        << "exception-directory-size: " << builder.info().exceptionDirectorySize << '\n'
        << "imports:";
    for (const std::string& library : builder.info().importedLibraries)
    {
        output << ' ' << library;
    }
    output << '\n';
    return ExitCode::Success;
}


ExitCode Driver::runProjectOopStaticLibraryGeneration(
    std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::ostream& output,
    std::ostream& error)
{
    frontend::ProjectCompilation compilation;
    std::string loadError;
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty())
        {
            error << "nsc: error: " << loadError << '\n';
        }
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }
    if (compilation.projectGraph() == nullptr || compilation.nameLookup() == nullptr ||
        compilation.memberModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 46 static-library compilation did not produce required models\n";
        return ExitCode::CompilationError;
    }
    const compilation::Project* rootProject = compilation.projectGraph()->rootProject();
    if (rootProject == nullptr || rootProject->outputKind() != compilation::ProjectOutputKind::Library)
    {
        error << "nsc: error: --emit-project-oop-lib requires root OutputType Library or StaticLibrary\n";
        return ExitCode::CompilationError;
    }

    llvm_backend::ProjectOopLlvmLowerer lowerer;
    std::unique_ptr<llvm_backend::LlvmModule> module = lowerer.lower(
        *compilation.nameLookup(), *compilation.oopMirModel());
    if (module == nullptr)
    {
        error << "nsc: error: " << lowerer.error() << '\n';
        return ExitCode::CompilationError;
    }
    std::string verificationError;
    if (!module->verify(verificationError))
    {
        error << "nsc: error: static-library LLVM verification failed: " << verificationError << '\n';
        return ExitCode::CompilationError;
    }

    linker::WindowsLibraryBuilder builder;
    std::string buildError;
    const std::filesystem::path destination(outputPath);
    if (!builder.buildStatic(*module, destination, optimizationLevel, buildError))
    {
        error << "nsc: error: NativeSharp static-library generation failed: " << buildError << '\n';
        return ExitCode::CompilationError;
    }

    std::filesystem::path headerDestination = destination;
    headerDestination.replace_extension(".h");
    interop::CHeaderGenerator headerGenerator;
    std::string headerError;
    if (!headerGenerator.writeHeader(compilation, headerDestination, headerError))
    {
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        error << "nsc: error: Stage 48.1 static-library C header generation failed: "
              << headerError << '\n';
        return ExitCode::CompilationError;
    }

    std::filesystem::path metadataDestination = destination;
    metadataDestination.replace_extension(".nsmeta");
    interop::LibraryMetadataCodec metadataCodec;
    std::string metadataError;
    if (!metadataCodec.write(compilation, metadataDestination, metadataError))
    {
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        std::filesystem::remove(headerDestination, removeError);
        error << "nsc: error: Stage 48.2 static-library metadata generation failed: "
              << metadataError << '\n';
        return ExitCode::CompilationError;
    }

    output
        << "project OOP static library emission: ok\n"
        << "format: COFF archive (.lib)\n"
        << "machine-members: AMD64 COFF\n"
        << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
        << "archive-members: " << builder.archiveInfo().memberCount << '\n'
        << "host-header: " << headerDestination.generic_string() << '\n'
        << "library-metadata: " << metadataDestination.generic_string() << '\n';
    return ExitCode::Success;
}

ExitCode Driver::runProjectOopDynamicLibraryGeneration(
    std::string_view inputPath,
    std::string_view outputPath,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    const std::vector<std::filesystem::path>& extraLinkLibraries,
    std::ostream& output,
    std::ostream& error)
{
    frontend::ProjectCompilation compilation;
    std::string loadError;
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty())
        {
            error << "nsc: error: " << loadError << '\n';
        }
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }
    if (compilation.projectGraph() == nullptr || compilation.nameLookup() == nullptr ||
        compilation.memberModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 47 DLL compilation did not produce required models\n";
        return ExitCode::CompilationError;
    }
    const compilation::Project* rootProject = compilation.projectGraph()->rootProject();
    if (rootProject == nullptr || rootProject->outputKind() != compilation::ProjectOutputKind::SharedLibrary)
    {
        error << "nsc: error: --emit-project-oop-dll requires root OutputType SharedLibrary\n";
        return ExitCode::CompilationError;
    }

    const auto types = compilation.nameLookup()->namedTypes();
    const auto members = compilation.memberModel()->members();
    const std::size_t rootProjectOrdinal = compilation.projectGraph()->projects().size() - 1;
    std::vector<std::string> exportSymbols;
    for (const frontend::ProjectOopMirFunction& function : compilation.oopMirModel()->functions())
    {
        if (function.isLambda || function.constructor || function.destructor ||
            function.memberOrdinal >= members.size() || function.declaringTypeOrdinal >= types.size())
        {
            continue;
        }
        const frontend::ClassMemberSymbol& member = members[function.memberOrdinal];
        const frontend::NamedTypeDeclaration& type = types[function.declaringTypeOrdinal];
        if (type.projectOrdinal != rootProjectOrdinal ||
            member.kind != frontend::ClassMemberKind::Method || !member.isExportedC)
        {
            continue;
        }
        exportSymbols.push_back(member.cAbiName);
    }
    if (exportSymbols.empty())
    {
        error << "nsc: error: Stage 47 DLL requires at least one 'public static export extern C' method in the root project\n";
        return ExitCode::CompilationError;
    }

    llvm_backend::ProjectOopLlvmLowerer lowerer;
    std::unique_ptr<llvm_backend::LlvmModule> module = lowerer.lower(
        *compilation.nameLookup(), *compilation.oopMirModel());
    if (module == nullptr)
    {
        error << "nsc: error: " << lowerer.error() << '\n';
        return ExitCode::CompilationError;
    }
    std::string verificationError;
    if (!module->verify(verificationError))
    {
        error << "nsc: error: DLL LLVM verification failed: " << verificationError << '\n';
        return ExitCode::CompilationError;
    }

    const std::filesystem::path destination(outputPath);
    std::filesystem::path importLibrary = destination;
    importLibrary.replace_extension(".lib");
    if (importLibrary == destination)
    {
        importLibrary += ".import.lib";
    }

    linker::WindowsLibraryBuilder builder;
    builder.setExternalImports(collectExternalImports(*compilation.oopMirModel()));
    builder.setExtraLinkLibraries(extraLinkLibraries);
    std::string buildError;
    if (!builder.buildDynamic(
            *module,
            exportSymbols,
            destination,
            importLibrary,
            optimizationLevel,
            buildError))
    {
        error << "nsc: error: NativeSharp DLL generation failed: " << buildError << '\n';
        return ExitCode::CompilationError;
    }

    std::filesystem::path headerDestination = destination;
    headerDestination.replace_extension(".h");
    interop::CHeaderGenerator headerGenerator;
    std::string headerError;
    if (!headerGenerator.writeHeader(compilation, headerDestination, headerError))
    {
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        std::filesystem::remove(importLibrary, removeError);
        error << "nsc: error: Stage 48.1 DLL C header generation failed: " << headerError << '\n';
        return ExitCode::CompilationError;
    }

    std::filesystem::path metadataDestination = destination;
    metadataDestination.replace_extension(".nsmeta");
    interop::LibraryMetadataCodec metadataCodec;
    std::string metadataError;
    if (!metadataCodec.write(compilation, metadataDestination, metadataError))
    {
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        std::filesystem::remove(importLibrary, removeError);
        std::filesystem::remove(headerDestination, removeError);
        error << "nsc: error: Stage 48.2 DLL metadata generation failed: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }

    output
        << "project OOP DLL emission: ok\n"
        << "format: PE32+ DLL\n"
        << "machine: AMD64 (0x8664)\n"
        << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
        << "import-library: " << importLibrary.generic_string() << '\n'
        << "host-header: " << headerDestination.generic_string() << '\n'
        << "library-metadata: " << metadataDestination.generic_string() << '\n'
        << "exports: " << builder.dynamicLibraryInfo().exportedSymbols.size() << '\n';
    for (const std::string& symbol : builder.dynamicLibraryInfo().exportedSymbols)
    {
        output << "  " << symbol << '\n';
    }
    output
        << "exception-directory-rva: " << builder.dynamicLibraryInfo().exceptionDirectoryRva << '\n'
        << "exception-directory-size: " << builder.dynamicLibraryInfo().exceptionDirectorySize << '\n'
        << "clr-header-rva: " << builder.dynamicLibraryInfo().clrDirectoryRva << '\n';
    return ExitCode::Success;
}

ExitCode Driver::runProjectHostHeaderGeneration(
    std::string_view inputPath,
    std::string_view outputPath,
    std::ostream& output,
    std::ostream& error)
{
    frontend::ProjectCompilation compilation;
    std::string loadError;
    if (!compilation.compileProjectObjectLayouts(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty()) error << "nsc: error: " << loadError << '\n';
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }

    if (compilation.projectGraph() == nullptr || compilation.projectGraph()->rootProject() == nullptr)
    {
        error << "nsc: error: Stage 48.1 C header generation requires a valid root project\n";
        return ExitCode::CompilationError;
    }
    const compilation::ProjectOutputKind outputKind = compilation.projectGraph()->rootProject()->outputKind();
    if (outputKind != compilation::ProjectOutputKind::Library &&
        outputKind != compilation::ProjectOutputKind::SharedLibrary)
    {
        error << "nsc: error: Stage 48.1 C headers require OutputType Library, StaticLibrary, or SharedLibrary\n";
        return ExitCode::CompilationError;
    }

    interop::CHeaderGenerator generator;
    std::string generationError;
    const std::filesystem::path destination(outputPath);
    if (!generator.writeHeader(compilation, destination, generationError))
    {
        error << "nsc: error: Stage 48.1 C header generation failed: " << generationError << '\n';
        return ExitCode::CompilationError;
    }

    output << "C host header emission: ok\n"
           << "output: " << destination.generic_string() << '\n';
    return ExitCode::Success;
}

ExitCode Driver::runProjectLibraryMetadataGeneration(
    std::string_view inputPath,
    std::string_view outputPath,
    std::ostream& output,
    std::ostream& error)
{
    frontend::ProjectCompilation compilation;
    std::string loadError;
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty()) error << "nsc: error: " << loadError << '\n';
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }
    if (compilation.projectGraph() == nullptr || compilation.projectGraph()->rootProject() == nullptr)
    {
        error << "nsc: error: Stage 48.2 metadata generation requires a valid root project\n";
        return ExitCode::CompilationError;
    }
    const compilation::ProjectOutputKind outputKind = compilation.projectGraph()->rootProject()->outputKind();
    if (outputKind != compilation::ProjectOutputKind::Library &&
        outputKind != compilation::ProjectOutputKind::SharedLibrary)
    {
        error << "nsc: error: Stage 48.2 metadata requires OutputType Library, StaticLibrary, or SharedLibrary\n";
        return ExitCode::CompilationError;
    }

    interop::LibraryMetadataCodec codec;
    std::string metadataError;
    const std::filesystem::path destination(outputPath);
    if (!codec.write(compilation, destination, metadataError))
    {
        error << "nsc: error: Stage 48.2 metadata generation failed: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }

    interop::LibraryMetadata metadata;
    if (!codec.read(destination, metadata, metadataError) || !codec.verifyCurrentCompiler(metadata, metadataError))
    {
        std::error_code removeError;
        std::filesystem::remove(destination, removeError);
        error << "nsc: error: Stage 48.2 generated metadata failed self-verification: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }

    output << "NativeSharp library metadata emission: ok\n"
           << "output: " << destination.generic_string() << '\n'
           << "format: NSMETA " << metadata.formatMajor << '.' << metadata.formatMinor << '\n'
           << "module: " << metadata.moduleName << '\n'
           << "target: " << metadata.targetTriple << '\n'
           << "types: " << metadata.types.size() << '\n'
           << "members: " << metadata.members.size() << '\n'
           << "exports: " << metadata.exports.size() << '\n'
           << "abi-fingerprint: " << interop::LibraryMetadataCodec::fingerprintHex(metadata.abiFingerprint) << '\n';
    return ExitCode::Success;
}

ExitCode Driver::verifyLibraryMetadata(
    std::string_view inputPath,
    std::ostream& output,
    std::ostream& error)
{
    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata metadata;
    std::string metadataError;
    if (!codec.read(std::filesystem::path(inputPath), metadata, metadataError))
    {
        error << "nsc: error: invalid NativeSharp library metadata: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }
    if (!codec.verifyCurrentCompiler(metadata, metadataError))
    {
        error << "nsc: error: incompatible NativeSharp library metadata: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }
    output << "NativeSharp library metadata: valid\n"
           << "producer: " << metadata.producerVersion << '\n'
           << "module: " << metadata.moduleName << '\n'
           << "target: " << metadata.targetTriple << '\n'
           << "types: " << metadata.types.size() << '\n'
           << "members: " << metadata.members.size() << '\n'
           << "generic-templates: " << metadata.genericTemplates.size() << '\n'
           << "generic-specializations: " << metadata.genericSpecializations.size() << '\n'
           << "dispatch-tables: " << metadata.dispatchTables.size() << '\n'
           << "exports: " << metadata.exports.size() << '\n'
           << "abi-fingerprint: " << interop::LibraryMetadataCodec::fingerprintHex(metadata.abiFingerprint) << '\n';
    return ExitCode::Success;
}

ExitCode Driver::runFrontend(
    std::string_view action,
    std::string_view path,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    std::ostream& output,
    std::ostream& error)
{
    frontend::FrontendCompilation compilation;
    std::string loadError;
    const bool requireMain = action == "--program";
    const bool success = compilation.compileFile(std::filesystem::path(path), loadError, requireMain);

    if (!loadError.empty())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }

    const source::SourceFile* sourceFile = compilation.sourceManager().getFile(compilation.primaryFileId());
    if (action == "--dump-source" && sourceFile != nullptr)
    {
        output
            << "file: " << sourceFile->displayPath() << '\n'
            << "bytes: " << sourceFile->size() << '\n'
            << "lines: " << sourceFile->lineCount() << '\n';
        for (std::uint32_t line = 1; line <= sourceFile->lineCount(); ++line)
        {
            output << line << ": " << sourceFile->lineText(line) << '\n';
        }
    }
    else if (action == "--dump-tokens")
    {
        lexer::TokenDumper dumper;
        dumper.dump(compilation.tokens(), output);
    }
    else if (action == "--dump-ast" && compilation.syntaxTree() != nullptr)
    {
        ast::AstDumper dumper;
        dumper.dump(compilation.syntaxTree(), output);
    }
    else if (action == "--dump-symbols" && compilation.semanticAnalyzer() != nullptr)
    {
        sema::SymbolDumper dumper;
        dumper.dump(compilation.semanticAnalyzer()->globalSymbols(), output);
    }
    else if (action == "--dump-sema" && compilation.semanticAnalyzer() != nullptr &&
             compilation.syntaxTree() != nullptr)
    {
        sema::SemanticDumper dumper;
        dumper.dump(
            compilation.syntaxTree(),
            compilation.semanticAnalyzer()->model(),
            output);
    }
    else if (action == "--dump-hir" && compilation.hirRoot() != nullptr)
    {
        hir::HirDumper dumper;
        dumper.dump(compilation.hirRoot(), output);
    }
    else if (action == "--verify-hir" && compilation.hirRoot() != nullptr)
    {
        hir::HirVerifier verifier;
        if (!verifier.verify(compilation.hirRoot()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        output << "HIR verification: ok\n";
    }
    else if (action == "--dump-mir" && compilation.mirModule() != nullptr)
    {
        mir::MirDumper dumper;
        dumper.dump(compilation.mirModule(), output);
    }
    else if (action == "--verify-mir" && compilation.mirModule() != nullptr)
    {
        mir::MirVerifier verifier;
        if (!verifier.verify(compilation.mirModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        output << "MIR verification: ok\n";
    }
    else if (action == "--dump-cfg" && compilation.cfgModule() != nullptr)
    {
        cfg::CfgDumper dumper;
        dumper.dump(compilation.cfgModule(), output);
    }
    else if (action == "--verify-cfg" && compilation.cfgModule() != nullptr)
    {
        cfg::CfgVerifier verifier;
        if (!verifier.verify(compilation.cfgModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        output << "CFG verification: ok\n";
    }
    else if (action == "--dump-dominance" && compilation.dominanceModule() != nullptr)
    {
        dominance::DominanceDumper dumper;
        dumper.dump(compilation.dominanceModule(), output);
    }
    else if (action == "--verify-dominance" && compilation.dominanceModule() != nullptr)
    {
        dominance::DominanceVerifier verifier;
        if (!verifier.verify(compilation.dominanceModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        output << "Dominance verification: ok\n";
    }
    else if (action == "--dump-ssa" && compilation.ssaModule() != nullptr)
    {
        ssa::SsaDumper dumper;
        dumper.dump(compilation.ssaModule(), output);
    }
    else if (action == "--verify-ssa" && compilation.ssaModule() != nullptr)
    {
        ssa::SsaVerifier verifier;
        if (!verifier.verify(compilation.ssaModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        output << "SSA verification: ok\n";
    }
    else if (action == "--dump-opt" && compilation.optimizationModule() != nullptr)
    {
        passes::OptimizationDumper dumper;
        dumper.dump(compilation.optimizationModule(), output);
    }
    else if (action == "--verify-opt" && compilation.optimizationModule() != nullptr)
    {
        passes::OptimizationVerifier verifier;
        if (!verifier.verify(compilation.optimizationModule()))
        {
            error << "nsc: internal error: " << verifier.error() << '\n';
            return ExitCode::CompilationError;
        }
        output << "Optimization verification: ok\n";
    }
    else if (action == "--dump-pass-pipeline" &&
             compilation.optimizationModule() != nullptr)
    {
        passes::PassPipelineDumper dumper;
        dumper.dump(compilation.optimizationModule(), output);
    }
    else if ((action == "--dump-llvm" || action == "--verify-llvm" ||
              action == "--dump-llvm-opt" || action == "--verify-llvm-opt" ||
              action == "--program" || action == "--check") &&
             success && compilation.optimizationModule() != nullptr &&
             compilation.semanticAnalyzer() != nullptr)
    {
        llvm_backend::LlvmLowerer lowerer(compilation.semanticAnalyzer()->globalSymbols());
        std::unique_ptr<llvm_backend::LlvmModule> llvmModule = lowerer.lower(compilation.optimizationModule());
        if (llvmModule == nullptr)
        {
            error << "nsc: internal error: LLVM lowering failed: " << lowerer.error() << '\n';
            return ExitCode::CompilationError;
        }

        if (action == "--dump-llvm")
        {
            output << llvmModule->text();
        }
        else
        {
            std::string verificationError;
            if (!llvmModule->verify(verificationError))
            {
                error << "nsc: internal error: LLVM verification failed: " << verificationError << '\n';
                return ExitCode::CompilationError;
            }

            if (action == "--verify-llvm")
            {
                output << "LLVM verification: ok\n";
            }
            else
            {
                std::string targetError;
                std::unique_ptr<llvm_backend::LlvmTargetMachine> targetMachine =
                    llvm_backend::LlvmTargetMachine::createWindowsX64(optimizationLevel, targetError);
                if (targetMachine == nullptr)
                {
                    error << "nsc: internal error: target machine creation failed: " << targetError << '\n';
                    return ExitCode::CompilationError;
                }

                llvm_backend::LlvmOptimizer optimizer;
                std::string optimizationError;
                if (!optimizer.optimize(*llvmModule, *targetMachine, optimizationLevel, optimizationError))
                {
                    error << "nsc: internal error: LLVM optimization failed: " << optimizationError << '\n';
                    return ExitCode::CompilationError;
                }

                if (action == "--dump-llvm-opt")
                {
                    output << llvmModule->text();
                }
                else if (action == "--verify-llvm-opt")
                {
                    output
                        << "LLVM optimization verification: ok\n"
                        << "optimization: "
                        << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n';
                }
            }
        }
    }

    if (compilation.diagnostics().hasErrors())
    {
        diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
        renderer.renderAll(compilation.diagnostics(), error);
    }
    if (!compilation.hirError().empty())
    {
        error << "nsc: internal error: " << compilation.hirError() << '\n';
    }
    if (!compilation.mirError().empty())
    {
        error << "nsc: internal error: " << compilation.mirError() << '\n';
    }
    if (!compilation.cfgError().empty())
    {
        error << "nsc: internal error: " << compilation.cfgError() << '\n';
    }
    if (!compilation.dominanceError().empty())
    {
        error << "nsc: internal error: " << compilation.dominanceError() << '\n';
    }
    if (!compilation.ssaError().empty())
    {
        error << "nsc: internal error: " << compilation.ssaError() << '\n';
    }
    if (!compilation.optimizationError().empty())
    {
        error << "nsc: internal error: " << compilation.optimizationError() << '\n';
    }

    return success ? ExitCode::Success : ExitCode::CompilationError;
}
}
