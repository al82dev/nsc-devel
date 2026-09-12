#pragma once

#include <cstdint>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::linker
{
struct PeImportedSymbol final
{
    std::string library;
    std::string name;
};

struct PeExecutableInfo final
{
    std::uint16_t machine = 0;
    std::uint16_t sectionCount = 0;
    std::uint16_t optionalHeaderMagic = 0;
    std::uint16_t characteristics = 0;
    std::uint32_t entryPointRva = 0;
    std::uint16_t subsystem = 0;
    std::uint32_t exportDirectoryRva = 0;
    std::uint32_t exportDirectorySize = 0;
    std::uint32_t importDirectoryRva = 0;
    std::uint32_t exceptionDirectoryRva = 0;
    std::uint32_t exceptionDirectorySize = 0;
    std::uint32_t clrDirectoryRva = 0;
    std::uint32_t debugDirectoryRva = 0;
    std::uint32_t debugDirectorySize = 0;
    bool hasCodeViewRsds = false;
    std::array<std::uint8_t, 16> codeViewGuid{};
    std::uint32_t codeViewAge = 0;
    std::string codeViewPdbFileName;
    std::vector<std::string> exportedSymbols;
    std::vector<std::string> importedLibraries;
    std::vector<PeImportedSymbol> importedSymbols;
};

class PeExecutableVerifier final
{
public:
    bool verify(const std::filesystem::path& path);
    bool verifyDynamicLibrary(const std::filesystem::path& path);
    const PeExecutableInfo& info() const noexcept;
    const std::string& error() const noexcept;

private:
    bool verifyImage(const std::filesystem::path& path, bool dynamicLibrary);

    PeExecutableInfo info_;
    std::string error_;
};
}
