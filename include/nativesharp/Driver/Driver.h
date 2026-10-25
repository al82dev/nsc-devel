#pragma once

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"
#include "nativesharp/Linker/LldLinker.h"

#include <filesystem>
#include <iosfwd>
#include <span>
#include <string_view>
#include <vector>

namespace nativesharp::compilation
{
class CompilationInputManifest;
}

namespace nativesharp::driver
{
class CompilerActionDispatcher;

class Driver final
{
public:
    ExitCode run(
        std::span<const std::string_view> arguments,
        std::ostream& output,
        std::ostream& error) const;

private:
    friend class CompilerActionDispatcher;

    ExitCode runWithManifest(
        std::span<const std::string_view> arguments,
        std::ostream& output,
        std::ostream& error,
        compilation::CompilationInputManifest* preparedInputManifest) const;
    static void printHelp(std::ostream& output);
    static void printVersion(std::ostream& output);
    static void printCompilerIdentity(std::ostream& output);
    static void printTarget(std::ostream& output);
    static void printDataLayout(std::ostream& output);
    static void printLlvmPipeline(
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        std::ostream& output);
    static ExitCode runCodeGeneration(
        std::string_view action,
        std::string_view inputPath,
        std::string_view outputPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        std::ostream& output,
        std::ostream& error);
    static ExitCode runProjectOopCodeGeneration(
        std::string_view inputPath,
        std::string_view outputPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        llvm_backend::DebugInformationKind debugInformation,
        const std::vector<llvm_backend::DebugPrefixMap>& debugPrefixMaps,
        std::ostream& output,
        std::ostream& error);
    static ExitCode runProjectOopExecutableGeneration(
        std::string_view inputPath,
        std::string_view outputPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        llvm_backend::DebugInformationKind debugInformation,
        const std::vector<llvm_backend::DebugPrefixMap>& debugPrefixMaps,
        llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
        bool incrementalLink,
        linker::WindowsExecutableSubsystem subsystem,
        const std::vector<std::filesystem::path>& extraLinkLibraries,
        std::ostream& output,
        std::ostream& error);
    static ExitCode runProjectOopStaticLibraryGeneration(
        std::string_view inputPath,
        std::string_view outputPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        llvm_backend::DebugInformationKind debugInformation,
        const std::vector<llvm_backend::DebugPrefixMap>& debugPrefixMaps,
        llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
        std::ostream& output,
        std::ostream& error);
    static ExitCode runProjectOopDynamicLibraryGeneration(
        std::string_view inputPath,
        std::string_view outputPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        llvm_backend::DebugInformationKind debugInformation,
        const std::vector<llvm_backend::DebugPrefixMap>& debugPrefixMaps,
        llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
        const std::vector<std::filesystem::path>& extraLinkLibraries,
        std::ostream& output,
        std::ostream& error);
    static ExitCode runProjectHostHeaderGeneration(
        std::string_view inputPath,
        std::string_view outputPath,
        std::ostream& output,
        std::ostream& error);
    static ExitCode runProjectLibraryMetadataGeneration(
        std::string_view inputPath,
        std::string_view outputPath,
        std::ostream& output,
        std::ostream& error);
    static ExitCode verifyLibraryMetadata(
        std::string_view inputPath,
        std::ostream& output,
        std::ostream& error);
    static ExitCode diffLibraryApi(
        std::string_view oldMetadataPath,
        std::string_view newMetadataPath,
        std::ostream& output,
        std::ostream& error);
    static ExitCode dumpLibraryApiSurface(
        std::string_view metadataPath,
        std::ostream& output,
        std::ostream& error);
    static ExitCode dumpLibraryAbi(
        std::string_view metadataPath,
        std::ostream& output,
        std::ostream& error);
    static ExitCode diffLibraryAbi(
        std::string_view oldMetadataPath,
        std::string_view newMetadataPath,
        std::ostream& output,
        std::ostream& error);
    static ExitCode verifyLibraryArtifacts(std::string_view metadataPath, std::string_view libraryPath,
                                           std::string_view runtimePath, std::string_view pdbPath,
                                           std::ostream& output, std::ostream& error);
    static ExitCode printCodeGeneration(
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        std::ostream& output,
        std::ostream& error);
    static ExitCode printLinker(std::ostream& output, std::ostream& error);
    static ExitCode dumpRuntimeLlvm(std::ostream& output, std::ostream& error);
    static ExitCode runExecutableGeneration(
        std::string_view inputPath,
        std::string_view outputPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        linker::WindowsExecutableSubsystem subsystem,
        std::ostream& output,
        std::ostream& error);
    static ExitCode runProject(
        std::string_view action,
        std::string_view path,
        std::ostream& output,
        std::ostream& error,
        llvm_backend::LlvmOptimizationLevel optimizationLevel = llvm_backend::LlvmOptimizationLevel::O0);
    // EN: Freeze 3.0 C-05: a file action (`nsc <file.ns>`, `--check`, the file dumps) runs on the
    //     implicit project of the file: the file plus a reference to NativeSharp.Core.
    // ES: Congelamiento 3.0 C-05: una accion de archivo (`nsc <file.ns>`, `--check`, los dumps de
    //     archivo) corre sobre el proyecto implicito del archivo: el archivo y una referencia a
    //     NativeSharp.Core.
    static ExitCode runSourceFile(
        std::string_view action,
        std::string_view path,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        std::ostream& output,
        std::ostream& error);
};
}
