#pragma once

#include "nativesharp/Linker/PeImportedSymbol.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::linker
{
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
    // EN: T08-D04: RVA of the TLS directory (data directory 9); zero when the image has no static TLS.
    // ES: T08-D04: RVA del directorio TLS (directorio de datos 9); cero si la imagen no tiene TLS estatico.
    std::uint32_t tlsDirectoryRva = 0;
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
}
