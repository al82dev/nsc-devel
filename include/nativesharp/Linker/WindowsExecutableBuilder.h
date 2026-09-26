#pragma once

#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"
#include "nativesharp/Linker/WindowsRuntimeImports.h"

#include <vector>

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
class WindowsExecutableBuilder final
{
public:
    bool build(
        llvm_backend::LlvmModule& programModule,
        const std::filesystem::path& outputPath,
        std::string& error);

    bool build(
        llvm_backend::LlvmModule& programModule,
        const std::filesystem::path& outputPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        std::string& error);

    bool build(
        llvm_backend::LlvmModule& programModule,
        const std::filesystem::path& outputPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        std::string_view mainSymbol,
        std::string& error);

    bool buildObjects(
        std::span<const std::filesystem::path> programObjects,
        std::string_view programLlvmText,
        const std::filesystem::path& outputPath,
        llvm_backend::LlvmOptimizationLevel optimizationLevel,
        std::string_view mainSymbol,
        std::string& error);

    const PeExecutableInfo& info() const noexcept;

    // Interop Foundation: external inputs for the link. `externalImports` are the project's
    // `extern C from "<dll>"` declarations (merged into the runtime import tables -- see
    // WindowsRuntimeImports.h); `extraLibraries` are caller-supplied .lib files (--link-lib)
    // appended to the link line as-is.
    void setExternalImports(std::vector<ExternalImportModule> externalImports);
    void setExtraLinkLibraries(std::vector<std::filesystem::path> extraLibraries);
    void setRuntimeLibrary(std::filesystem::path runtimeLibrary);
    void setSubsystem(WindowsExecutableSubsystem subsystem) noexcept;
    void setDebugInformation(llvm_backend::DebugInformationKind debugInformation) noexcept;
    void setIncrementalLink(bool enabled) noexcept;
    void setProgramObjectsVerified(bool verified) noexcept;
    bool reusedIncrementalLink() const noexcept;

private:
    PeExecutableInfo info_;
    std::vector<ExternalImportModule> externalImports_;
    std::vector<std::filesystem::path> extraLinkLibraries_;
    std::filesystem::path runtimeLibrary_;
    WindowsExecutableSubsystem subsystem_ = WindowsExecutableSubsystem::Console;
    llvm_backend::DebugInformationKind debugInformation_ = llvm_backend::DebugInformationKind::None;
    bool incrementalLink_ = false;
    bool programObjectsVerified_ = false;
    bool reusedIncrementalLink_ = false;
};
}
