#pragma once

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/DebugPrefixMap.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/Linker/LldLinker.h"

#include <filesystem>
#include <iosfwd>
#include <span>
#include <string_view>
#include <vector>

namespace nativesharp::driver
{
class Driver final
{
public:
    ExitCode run(
        std::span<const std::string_view> arguments,
        std::ostream& output,
        std::ostream& error) const;

private:
    static void printHelp(std::ostream& output);
    static void printVersion(std::ostream& output);
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
        std::ostream& output,
        std::ostream& error);
    static ExitCode runProjectOopDynamicLibraryGeneration(
        std::string_view inputPath,
        std::string_view outputPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        llvm_backend::DebugInformationKind debugInformation,
        const std::vector<llvm_backend::DebugPrefixMap>& debugPrefixMaps,
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
        std::ostream& error);
    static ExitCode runFrontend(
        std::string_view action,
        std::string_view path,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        std::ostream& output,
        std::ostream& error);
};
}
