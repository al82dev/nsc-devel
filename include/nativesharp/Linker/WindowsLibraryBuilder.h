#pragma once

#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/Linker/CoffArchiveVerifier.h"
#include "nativesharp/Linker/WindowsRuntimeImports.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"

#include <filesystem>
#include <span>
#include <string>

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
        std::string& error);

    const CoffArchiveInfo& archiveInfo() const noexcept;
    const PeExecutableInfo& dynamicLibraryInfo() const noexcept;

    // Interop Foundation -- see WindowsExecutableBuilder: the project's `extern C from` imports
    // and caller-supplied extra .lib inputs for the dynamic-library link. The STATIC archive path
    // ignores both by nature (an archive is not linked); its consumer's final link resolves them.
    void setExternalImports(std::vector<ExternalImportModule> externalImports);
    void setExtraLinkLibraries(std::vector<std::filesystem::path> extraLibraries);

private:
    CoffArchiveInfo archiveInfo_;
    PeExecutableInfo dynamicLibraryInfo_;
    std::vector<ExternalImportModule> externalImports_;
    std::vector<std::filesystem::path> extraLinkLibraries_;
};
}
