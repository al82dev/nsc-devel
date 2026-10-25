#pragma once

#include "CoffSection.h"
#include "CoffSymbol.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::llvm_backend
{
// ES: Un objeto COFF x64 cargado en memoria para leer sus secciones, simbolos y relocaciones y corregir campos
// de 32 bits en su lugar. Solo entiende el formato COFF normal; un objeto de otra maquina o en formato bigobj
// se carga como "no AMD64" y no se toca.
// EN: An x64 COFF object loaded in memory to read its sections, symbols and relocations and patch 32-bit
// fields in place. It only understands the regular COFF format; an object for another machine or in bigobj
// format loads as "not AMD64" and is left alone.
class CoffObjectFile final
{
public:
    bool load(const std::filesystem::path& path, std::string& error);
    bool save(const std::filesystem::path& path, std::string& error) const;

    bool isAmd64() const noexcept;
    const std::vector<CoffSection>& sections() const noexcept;
    const std::uint8_t* data(std::size_t offset, std::size_t size) const noexcept;

    bool readU16(std::size_t offset, std::uint16_t& value) const noexcept;
    bool readU32(std::size_t offset, std::uint32_t& value) const noexcept;
    bool writeU32(std::size_t offset, std::uint32_t value) noexcept;

    // ES: Lugar al que apunta la relocacion del campo de 32 bits en `fieldOffset` de `section`: la seccion
    // (desde 1) y el desplazamiento del simbolo mas el sumando guardado en el campo. Solo acepta las
    // relocaciones ADDR32NB y SECREL, que son las de .pdata y de CodeView.
    // EN: Where the relocation of the 32-bit field at `fieldOffset` of `section` points: the symbol's section
    // (1-based) and offset plus the addend stored in the field. It only accepts ADDR32NB and SECREL
    // relocations, which are the ones .pdata and CodeView use.
    bool relocationTarget(
        const CoffSection& section,
        std::uint32_t fieldOffset,
        std::int32_t& sectionNumber,
        std::uint32_t& sectionOffset) const;

private:
    bool loadSections(std::uint16_t count, std::size_t first, std::size_t stringTable, std::string& error);
    bool loadRelocations(
        CoffSection& section,
        std::uint32_t pointer,
        std::uint32_t count,
        bool extended,
        std::string& error);
    bool loadSymbols(std::size_t pointer, std::uint32_t count, std::string& error);
    bool sectionName(const std::uint8_t* field, std::size_t stringTable, std::string& name) const;

    std::vector<std::uint8_t> bytes_;
    std::vector<CoffSection> sections_;
    std::vector<CoffSymbol> symbols_;
    bool amd64_ = false;
};
}
