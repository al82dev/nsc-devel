#include "CoffObjectFile.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace nativesharp::llvm_backend
{
namespace
{
constexpr std::uint16_t kMachineAmd64 = 0x8664;
constexpr std::size_t kFileHeaderSize = 20;
constexpr std::size_t kSectionHeaderSize = 40;
constexpr std::size_t kRelocationSize = 10;
constexpr std::size_t kSymbolSize = 18;
constexpr std::uint32_t kRelocationOverflow = 0x01000000;
constexpr std::uint16_t kRelocationAddr32Nb = 0x0003;
constexpr std::uint16_t kRelocationSecRel = 0x000B;

bool relocationOffsetLess(const CoffRelocation& left, const CoffRelocation& right)
{
    return left.offset < right.offset;
}

bool relocationBeforeOffset(const CoffRelocation& relocation, std::uint32_t offset)
{
    return relocation.offset < offset;
}
}

bool CoffObjectFile::load(const std::filesystem::path& path, std::string& error)
{
    bytes_.clear();
    sections_.clear();
    symbols_.clear();
    amd64_ = false;
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error = "cannot open COFF object '" + path.string() + "'";
        return false;
    }
    bytes_.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    std::uint16_t machine = 0;
    if (!readU16(0, machine) || machine != kMachineAmd64) return true;

    std::uint16_t sectionCount = 0;
    std::uint32_t symbolPointer = 0;
    std::uint32_t symbolCount = 0;
    std::uint16_t optionalHeaderSize = 0;
    if (!readU16(2, sectionCount) || !readU32(8, symbolPointer) || !readU32(12, symbolCount) ||
        !readU16(16, optionalHeaderSize))
    {
        error = "COFF object '" + path.string() + "' has a truncated file header";
        return false;
    }
    const std::size_t stringTable = static_cast<std::size_t>(symbolPointer) +
        static_cast<std::size_t>(symbolCount) * kSymbolSize;
    if (!loadSections(sectionCount, kFileHeaderSize + optionalHeaderSize, stringTable, error) ||
        !loadSymbols(symbolPointer, symbolCount, error))
    {
        error = "COFF object '" + path.string() + "': " + error;
        return false;
    }
    amd64_ = true;
    return true;
}

bool CoffObjectFile::save(const std::filesystem::path& path, std::string& error) const
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "cannot rewrite COFF object '" + path.string() + "'";
        return false;
    }
    stream.write(reinterpret_cast<const char*>(bytes_.data()), static_cast<std::streamsize>(bytes_.size()));
    if (!stream)
    {
        error = "cannot write COFF object '" + path.string() + "'";
        return false;
    }
    return true;
}

bool CoffObjectFile::isAmd64() const noexcept
{
    return amd64_;
}

const std::vector<CoffSection>& CoffObjectFile::sections() const noexcept
{
    return sections_;
}

const std::uint8_t* CoffObjectFile::data(std::size_t offset, std::size_t size) const noexcept
{
    if (offset > bytes_.size() || size > bytes_.size() - offset) return nullptr;
    return bytes_.data() + offset;
}

bool CoffObjectFile::readU16(std::size_t offset, std::uint16_t& value) const noexcept
{
    const std::uint8_t* field = data(offset, 2);
    if (field == nullptr) return false;
    value = static_cast<std::uint16_t>(field[0] | (field[1] << 8U));
    return true;
}

bool CoffObjectFile::readU32(std::size_t offset, std::uint32_t& value) const noexcept
{
    const std::uint8_t* field = data(offset, 4);
    if (field == nullptr) return false;
    value = static_cast<std::uint32_t>(field[0]) | (static_cast<std::uint32_t>(field[1]) << 8U) |
        (static_cast<std::uint32_t>(field[2]) << 16U) | (static_cast<std::uint32_t>(field[3]) << 24U);
    return true;
}

bool CoffObjectFile::writeU32(std::size_t offset, std::uint32_t value) noexcept
{
    if (offset > bytes_.size() || bytes_.size() - offset < 4) return false;
    bytes_[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes_[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes_[offset + 2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    bytes_[offset + 3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
    return true;
}

bool CoffObjectFile::relocationTarget(
    const CoffSection& section,
    std::uint32_t fieldOffset,
    std::int32_t& sectionNumber,
    std::uint32_t& sectionOffset) const
{
    const auto found = std::lower_bound(section.relocations.begin(), section.relocations.end(),
        fieldOffset, relocationBeforeOffset);
    if (found == section.relocations.end() || found->offset != fieldOffset) return false;
    if (found->type != kRelocationAddr32Nb && found->type != kRelocationSecRel) return false;
    if (found->symbolIndex >= symbols_.size() || !symbols_[found->symbolIndex].present) return false;
    const CoffSymbol& symbol = symbols_[found->symbolIndex];
    if (symbol.sectionNumber <= 0 || static_cast<std::size_t>(symbol.sectionNumber) > sections_.size())
        return false;
    std::uint32_t addend = 0;
    if (fieldOffset > section.dataSize || section.dataSize - fieldOffset < 4 ||
        !readU32(static_cast<std::size_t>(section.dataOffset) + fieldOffset, addend))
        return false;
    sectionNumber = symbol.sectionNumber;
    sectionOffset = symbol.value + addend;
    return true;
}

bool CoffObjectFile::loadSections(
    std::uint16_t count,
    std::size_t first,
    std::size_t stringTable,
    std::string& error)
{
    sections_.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index)
    {
        const std::size_t header = first + static_cast<std::size_t>(index) * kSectionHeaderSize;
        const std::uint8_t* nameField = data(header, 8);
        std::uint32_t dataSize = 0;
        std::uint32_t dataOffset = 0;
        std::uint32_t relocationPointer = 0;
        std::uint16_t relocationCount = 0;
        std::uint32_t characteristics = 0;
        if (nameField == nullptr || !readU32(header + 16, dataSize) || !readU32(header + 20, dataOffset) ||
            !readU32(header + 24, relocationPointer) || !readU16(header + 32, relocationCount) ||
            !readU32(header + 36, characteristics))
        {
            error = "truncated section header " + std::to_string(index + 1);
            return false;
        }
        CoffSection section;
        if (!sectionName(nameField, stringTable, section.name))
        {
            error = "section " + std::to_string(index + 1) + " has an invalid long name";
            return false;
        }
        if (dataSize != 0 && data(dataOffset, dataSize) == nullptr)
        {
            error = "section '" + section.name + "' lies outside the file";
            return false;
        }
        section.dataOffset = dataOffset;
        section.dataSize = dataSize;
        const bool extended = (characteristics & kRelocationOverflow) != 0 && relocationCount == 0xFFFF;
        if (!loadRelocations(section, relocationPointer, relocationCount, extended, error)) return false;
        sections_.push_back(std::move(section));
    }
    return true;
}

bool CoffObjectFile::loadRelocations(
    CoffSection& section,
    std::uint32_t pointer,
    std::uint32_t count,
    bool extended,
    std::string& error)
{
    std::size_t first = 0;
    if (extended)
    {
        // ES: Con IMAGE_SCN_LNK_NRELOC_OVFL la cuenta real esta en la primera relocacion, que la incluye a
        // ella misma y no es una relocacion verdadera.
        // EN: With IMAGE_SCN_LNK_NRELOC_OVFL the real count is in the first relocation, which counts itself
        // and is not a real relocation.
        if (!readU32(pointer, count) || count == 0)
        {
            error = "section '" + section.name + "' has an invalid extended relocation count";
            return false;
        }
        first = 1;
    }
    section.relocations.reserve(count);
    for (std::size_t index = first; index < count; ++index)
    {
        const std::size_t entry = static_cast<std::size_t>(pointer) + index * kRelocationSize;
        CoffRelocation relocation;
        if (!readU32(entry, relocation.offset) || !readU32(entry + 4, relocation.symbolIndex) ||
            !readU16(entry + 8, relocation.type))
        {
            error = "section '" + section.name + "' has truncated relocations";
            return false;
        }
        section.relocations.push_back(relocation);
    }
    std::stable_sort(section.relocations.begin(), section.relocations.end(), relocationOffsetLess);
    return true;
}

bool CoffObjectFile::loadSymbols(std::size_t pointer, std::uint32_t count, std::string& error)
{
    symbols_.assign(count, CoffSymbol{});
    std::uint32_t index = 0;
    while (index < count)
    {
        const std::size_t entry = pointer + static_cast<std::size_t>(index) * kSymbolSize;
        std::uint32_t value = 0;
        std::uint16_t sectionNumber = 0;
        const std::uint8_t* auxiliaryCount = data(entry + 17, 1);
        if (!readU32(entry + 8, value) || !readU16(entry + 12, sectionNumber) || auxiliaryCount == nullptr)
        {
            error = "truncated symbol table";
            return false;
        }
        symbols_[index].value = value;
        symbols_[index].sectionNumber = static_cast<std::int16_t>(sectionNumber);
        symbols_[index].present = true;
        index += 1U + *auxiliaryCount;
    }
    return true;
}

bool CoffObjectFile::sectionName(const std::uint8_t* field, std::size_t stringTable, std::string& name) const
{
    std::size_t length = 0;
    while (length < 8 && field[length] != 0) ++length;
    name.assign(reinterpret_cast<const char*>(field), length);
    if (name.empty() || name[0] != '/') return true;
    std::size_t offset = 0;
    for (std::size_t index = 1; index < name.size(); ++index)
    {
        if (name[index] < '0' || name[index] > '9') return false;
        offset = offset * 10U + static_cast<std::size_t>(name[index] - '0');
    }
    const std::size_t start = stringTable + offset;
    std::size_t end = start;
    while (data(end, 1) != nullptr && bytes_[end] != 0) ++end;
    if (data(end, 1) == nullptr) return false;
    name.assign(reinterpret_cast<const char*>(bytes_.data() + start), end - start);
    return true;
}
}
