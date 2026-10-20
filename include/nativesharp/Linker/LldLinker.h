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
};
}
