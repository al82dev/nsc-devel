#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::compilation::detail
{
// EN: T08-D04b. The Windows loader initializes the static TLS of a DLL (assigns `_tls_index` and copies the
//     template) only when the image has an import directory. For `RuntimeTls="ImportDirectory"` NSC derives a copy
//     of the DLL whose import directory is one null descriptor (20 zero bytes) placed in the zero padding that
//     already follows the contents of a read-only initialized data section. Only three fields change: that
//     section's VirtualSize, DataDirectory[1] and the CheckSum (recomputed when the original has one). Code,
//     exports, relocations, TLS, debug records and file size stay the same. The original file is never written.
//     A signed DLL is rejected: the change would invalidate its Authenticode signature.
// ES: T08-D04b. El cargador de Windows inicializa el TLS estatico de una DLL (asigna `_tls_index` y copia la
//     plantilla) solo si la imagen tiene directorio de importacion. Para `RuntimeTls="ImportDirectory"` NSC deriva
//     una copia de la DLL cuyo directorio de importacion es un descriptor nulo (20 bytes en cero) colocado en el
//     relleno de ceros que ya sigue al contenido de una seccion de datos inicializados de solo lectura. Solo
//     cambian tres campos: el VirtualSize de esa seccion, DataDirectory[1] y el CheckSum (recalculado si el
//     original lo tiene). Codigo, exportaciones, relocalizaciones, TLS, registros de depuracion y tamano del
//     archivo no cambian. El archivo original nunca se escribe. Una DLL firmada se rechaza: el cambio invalidaria
//     su firma Authenticode.
struct RuntimeTlsAdaptation
{
    std::uint32_t importDirectoryRva = 0;
    std::size_t sectionIndex = 0;
    std::uint32_t checkSum = 0;
};

inline std::uint32_t peRead32(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

inline std::uint16_t peRead16(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

inline void peWrite32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}

// EN: PE image checksum (the algorithm of CheckSumMappedFile): 16-bit folded sum of the file with the CheckSum
//     field read as zero, plus the file length.
// ES: Suma de verificacion de imagen PE (el algoritmo de CheckSumMappedFile): suma plegada de 16 bits del archivo
//     con el campo CheckSum leido como cero, mas la longitud del archivo.
inline std::uint32_t peImageCheckSum(std::span<const std::uint8_t> bytes, std::size_t checkSumOffset)
{
    std::uint64_t sum = 0;
    for (std::size_t offset = 0; offset < bytes.size(); offset += 2)
    {
        std::uint32_t word = bytes[offset];
        if (offset + 1 < bytes.size()) word |= static_cast<std::uint32_t>(bytes[offset + 1]) << 8;
        if (offset >= checkSumOffset && offset < checkSumOffset + 4) word = 0;
        sum += word;
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    sum = (sum & 0xFFFFu) + (sum >> 16);
    return static_cast<std::uint32_t>(sum + bytes.size());
}

// EN: Derives the adapted image in `output`; returns false with the exact cause when the DLL cannot or need not
//     be adapted.
// ES: Deriva la imagen adaptada en `output`; devuelve false con la causa exacta si la DLL no puede o no necesita
//     adaptarse.
inline bool deriveRuntimeWithEmptyImportDirectory(std::span<const std::uint8_t> input,
                                                  std::vector<std::uint8_t>& output,
                                                  RuntimeTlsAdaptation& adaptation, std::string& error)
{
    const auto fail = [&](const std::string& why) { error = why; return false; };
    if (input.size() < 0x40 || input[0] != 'M' || input[1] != 'Z') return fail("is not a PE image");
    const std::size_t nt = peRead32(input, 0x3C);
    if (nt > input.size() || input.size() - nt < 24 + 112 || peRead32(input, nt) != 0x00004550u)
        return fail("is not a PE image");
    const std::size_t fileHeader = nt + 4;
    const std::uint16_t machine = peRead16(input, fileHeader);
    const std::uint16_t sectionCount = peRead16(input, fileHeader + 2);
    const std::uint16_t optionalSize = peRead16(input, fileHeader + 16);
    const std::uint16_t characteristics = peRead16(input, fileHeader + 18);
    const std::size_t optional = fileHeader + 20;
    if (machine != 0x8664u || optionalSize < 112 + 13 * 8 || input.size() - optional < optionalSize ||
        peRead16(input, optional) != 0x20Bu)
        return fail("is not an AMD64 PE32+ image");
    if ((characteristics & 0x2000u) == 0) return fail("is not a DLL");
    if (peRead32(input, optional + 108) < 13) return fail("has too few data directories");
    const std::size_t directories = optional + 112;
    const std::size_t importDirectory = directories + 1 * 8;
    const std::size_t securityDirectory = directories + 4 * 8;
    const std::size_t tlsDirectory = directories + 9 * 8;
    const std::size_t checkSumOffset = optional + 64;
    if (peRead32(input, importDirectory) != 0 || peRead32(input, importDirectory + 4) != 0)
        return fail("already has an import directory: RuntimeTls=\"ImportDirectory\" is not needed");
    if (peRead32(input, tlsDirectory) == 0) return fail("has no static TLS directory: RuntimeTls is not needed");
    if (peRead32(input, securityDirectory) != 0 || peRead32(input, securityDirectory + 4) != 0)
        return fail("is signed (Authenticode): NSC does not adapt a signed DLL, whose signature the change would "
                    "invalidate");
    const std::uint32_t sizeOfImage = peRead32(input, optional + 56);
    const std::size_t sections = optional + optionalSize;
    if (sectionCount == 0 || input.size() - sections < static_cast<std::size_t>(sectionCount) * 40)
        return fail("has an invalid section table");
    for (std::size_t index = 0; index < sectionCount; ++index)
    {
        const std::size_t header = sections + index * 40;
        const std::uint32_t virtualSize = peRead32(input, header + 8);
        const std::uint32_t virtualAddress = peRead32(input, header + 12);
        const std::uint32_t rawSize = peRead32(input, header + 16);
        const std::uint32_t rawPointer = peRead32(input, header + 20);
        const std::uint32_t flags = peRead32(input, header + 36);
        const bool readOnlyData = (flags & 0x00000040u) != 0 && (flags & 0x40000000u) != 0 &&
            (flags & 0x80000000u) == 0 && (flags & 0x20000000u) == 0;
        if (!readOnlyData || rawPointer == 0) continue;
        const std::uint64_t offset = (static_cast<std::uint64_t>(virtualSize) + 3u) & ~std::uint64_t{3};
        if (offset + 20 > rawSize || static_cast<std::uint64_t>(rawPointer) + offset + 20 > input.size()) continue;
        // EN: the next section (by address) and SizeOfImage bound the extended virtual range.
        // ES: la siguiente seccion (por direccion) y SizeOfImage limitan el rango virtual ampliado.
        std::uint64_t limit = sizeOfImage;
        for (std::size_t other = 0; other < sectionCount; ++other)
        {
            const std::uint32_t otherAddress = peRead32(input, sections + other * 40 + 12);
            if (other != index && otherAddress > virtualAddress) limit = std::min<std::uint64_t>(limit, otherAddress);
        }
        if (static_cast<std::uint64_t>(virtualAddress) + offset + 20 > limit) continue;
        const auto padding = input.subspan(rawPointer + static_cast<std::size_t>(offset), 20);
        if (!std::all_of(padding.begin(), padding.end(), [](std::uint8_t value) { return value == 0; })) continue;
        output.assign(input.begin(), input.end());
        peWrite32(output, header + 8, static_cast<std::uint32_t>(offset + 20));
        adaptation.importDirectoryRva = virtualAddress + static_cast<std::uint32_t>(offset);
        peWrite32(output, importDirectory, adaptation.importDirectoryRva);
        peWrite32(output, importDirectory + 4, 20);
        adaptation.sectionIndex = index;
        adaptation.checkSum = peRead32(input, checkSumOffset) == 0 ? 0 : peImageCheckSum(output, checkSumOffset);
        peWrite32(output, checkSumOffset, adaptation.checkSum);
        // EN: the copy differs from the original only in the three fields.
        // ES: la copia difiere del original solo en los tres campos.
        for (std::size_t position = 0; position < input.size(); ++position)
        {
            const bool allowed = (position >= header + 8 && position < header + 12) ||
                (position >= importDirectory && position < importDirectory + 8) ||
                (position >= checkSumOffset && position < checkSumOffset + 4);
            if (!allowed && output[position] != input[position])
                return fail("internal error: the adapted copy changed an unexpected byte");
        }
        error.clear();
        return true;
    }
    return fail("has no read-only initialized data section with 20 zero bytes after its contents inside the "
                "section and before the next one: NSC does not add sections or move data");
}
}
