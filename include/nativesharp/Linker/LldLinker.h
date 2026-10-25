#pragma once

#include "nativesharp/Linker/WindowsExecutableSubsystem.h"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::linker
{
class LldLinker final
{
public:
    LldLinker(const LldLinker&) = delete;
    LldLinker& operator=(const LldLinker&) = delete;

    static std::unique_ptr<LldLinker> discover(std::string& error);

    const std::filesystem::path& executablePath() const noexcept;
    std::filesystem::path librarianExecutablePath() const;

    // EN: Executable and DLL links pass /opt:ref, so the linker drops every COMDAT section nothing
    //     reachable references (optimization part 2, item 50: ThinLTO objects of a final link).
    // ES: Los enlaces de ejecutable y DLL pasan /opt:ref, asi que el enlazador tira toda seccion
    //     COMDAT que nada alcanzable referencia (parte 2, punto 50: objetos ThinLTO de enlace final).
    void setRemoveUnreferencedSections(bool remove) noexcept;

    bool createImportLibrary(
        std::string_view dllName,
        std::span<const std::string_view> symbols,
        const std::filesystem::path& outputPath,
        std::string& error) const;

    bool createStaticLibrary(
        std::span<const std::filesystem::path> objectFiles,
        const std::filesystem::path& outputPath,
        std::string& error) const;

    bool linkDynamicLibrary(
        std::span<const std::filesystem::path> objectFiles,
        std::span<const std::filesystem::path> importLibraries,
        std::span<const std::string> exportSymbols,
        const std::filesystem::path& outputPath,
        const std::filesystem::path& importLibraryPath,
        const std::filesystem::path& pdbPath,
        std::string& error) const;

    bool linkExecutable(
        std::span<const std::filesystem::path> objectFiles,
        std::span<const std::filesystem::path> importLibraries,
        const std::filesystem::path& outputPath,
        WindowsExecutableSubsystem subsystem,
        std::string& error) const;

    bool linkExecutable(
        std::span<const std::filesystem::path> objectFiles,
        std::span<const std::filesystem::path> importLibraries,
        const std::filesystem::path& outputPath,
        WindowsExecutableSubsystem subsystem,
        const std::filesystem::path& pdbPath,
        std::string& error) const;

private:
    explicit LldLinker(std::filesystem::path executablePath) noexcept;

    bool runResponseFile(
        const std::filesystem::path& responsePath,
        const std::filesystem::path& logPath,
        std::string& error,
        bool libraryMode = false,
        const std::filesystem::path& workingDirectory = {}) const;

    std::filesystem::path executablePath_;
    bool removeUnreferencedSections_ = false;
};
}
