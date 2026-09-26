#pragma once

#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/Linker/CoffArchiveVerifier.h"
#include "nativesharp/Linker/WindowsRuntimeImports.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
class LlvmModule;
}

namespace nativesharp::linker
{
class WindowsLibraryBuilder final
{
public:
    bool buildStatic(
        llvm_backend::LlvmModule& programModule,
        const std::filesystem::path& outputPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        std::string& error);

    bool buildDynamic(
        llvm_backend::LlvmModule& programModule,
        std::span<const std::string> exportSymbols,
        const std::filesystem::path& outputPath,
        const std::filesystem::path& importLibraryPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        llvm_backend::DebugInformationKind debugInformation,
        std::string& error);

    bool buildStaticObjects(
        std::span<const std::filesystem::path> programObjects,
        const std::filesystem::path& outputPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        std::string& error);

    bool buildDynamicObjects(
        std::span<const std::filesystem::path> programObjects,
        std::string_view programLlvmText,
        std::span<const std::string> exportSymbols,
        const std::filesystem::path& outputPath,
        const std::filesystem::path& importLibraryPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        llvm_backend::DebugInformationKind debugInformation,
        std::string& error);

    const CoffArchiveInfo& archiveInfo() const noexcept;
    const PeExecutableInfo& dynamicLibraryInfo() const noexcept;

    // Interop Foundation -- see WindowsExecutableBuilder: the project's `extern C from` imports
    // and caller-supplied extra .lib inputs for the dynamic-library link. For a static product,
    // referenced archives are flattened into the resulting archive so its consumer does not have
    // to rediscover private static dependencies.
    void setExternalImports(std::vector<ExternalImportModule> externalImports);
    void setExtraLinkLibraries(std::vector<std::filesystem::path> extraLibraries);
    void setRuntimeLibrary(std::filesystem::path runtimeLibrary);
    void setProgramObjectsVerified(bool verified) noexcept;

private:
    CoffArchiveInfo archiveInfo_;
    PeExecutableInfo dynamicLibraryInfo_;
    std::vector<ExternalImportModule> externalImports_;
    std::vector<std::filesystem::path> extraLinkLibraries_;
    std::filesystem::path runtimeLibrary_;
    bool programObjectsVerified_ = false;
};
}
