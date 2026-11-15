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
    void setLinkerSelection(std::string selection);
    void setSubsystem(WindowsExecutableSubsystem subsystem) noexcept;
    void setDebugInformation(llvm_backend::DebugInformationKind debugInformation) noexcept;
    void setIncrementalLink(bool enabled) noexcept;
    void setProgramObjectsVerified(bool verified) noexcept;
    // EN: Links with /opt:ref (ThinLTO objects of a final link, optimization part 2 item 50).
    // ES: Enlaza con /opt:ref (objetos ThinLTO de un enlace final, parte 2 punto 50).
    void setRemoveUnreferencedSections(bool remove) noexcept;
    // EN: T15. Starts the executable through the C runtime startup the link supplies (mainCRTStartup of the
    //     static MSVC runtime, given with --link-lib): it initializes the C and C++ runtime, runs the global
    //     constructors, calls the `main` the entry module defines and runs the exit handlers.
    // ES: T15. Arranca el ejecutable por el arranque del runtime de C que aporta el enlace (mainCRTStartup del
    //     runtime estatico de MSVC, dado con --link-lib): inicializa el runtime de C y C++, ejecuta los
    //     constructores globales, llama al `main` que define el modulo de entrada y ejecuta los manejadores
    //     de salida.
    void setCrtStartup(bool crtStartup) noexcept;
    bool reusedIncrementalLink() const noexcept;

private:
    PeExecutableInfo info_;
    std::vector<ExternalImportModule> externalImports_;
    std::vector<std::filesystem::path> extraLinkLibraries_;
    std::filesystem::path runtimeLibrary_;
    std::string linkerSelection_;
    WindowsExecutableSubsystem subsystem_ = WindowsExecutableSubsystem::Console;
    llvm_backend::DebugInformationKind debugInformation_ = llvm_backend::DebugInformationKind::None;
    bool incrementalLink_ = false;
    bool programObjectsVerified_ = false;
    bool removeUnreferencedSections_ = false;
    bool crtStartup_ = false;
    bool reusedIncrementalLink_ = false;
};
}
