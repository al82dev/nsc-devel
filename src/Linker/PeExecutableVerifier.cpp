#include "nativesharp/Linker/PeExecutableVerifier.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <optional>
#include <vector>

namespace nativesharp::linker
{
namespace
{
constexpr std::uint16_t kImageFileMachineAmd64 = 0x8664;
constexpr std::uint16_t kImageFileExecutableImage = 0x0002;
constexpr std::uint16_t kImageFileDll = 0x2000;
constexpr std::uint16_t kPe32PlusMagic = 0x020b;
constexpr std::uint16_t kWindowsGuiSubsystem = 2;
constexpr std::uint16_t kWindowsCuiSubsystem = 3;
constexpr std::size_t kDosPeOffsetField = 0x3c;
constexpr std::size_t kCoffHeaderSize = 20;
constexpr std::size_t kPeSignatureSize = 4;
constexpr std::size_t kSectionHeaderSize = 40;
constexpr std::size_t kPe32PlusDataDirectoryOffset = 112;
constexpr std::size_t kExportDirectoryIndex = 0;
constexpr std::size_t kImportDirectoryIndex = 1;
constexpr std::size_t kExceptionDirectoryIndex = 3;
constexpr std::size_t kClrDirectoryIndex = 14;
constexpr std::size_t kImportDescriptorSize = 20;

std::uint16_t readU16(const std::vector<unsigned char>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::uint32_t readU32(const std::vector<unsigned char>& bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

std::uint64_t readU64(const std::vector<unsigned char>& bytes, std::size_t offset)
{
    return static_cast<std::uint64_t>(readU32(bytes, offset)) |
        (static_cast<std::uint64_t>(readU32(bytes, offset + 4)) << 32U);
}

bool rangeFits(std::size_t offset, std::size_t size, std::size_t total)
{
    return offset <= total && size <= total - offset;
}

struct SectionView final
{
    std::uint32_t virtualAddress = 0;
    std::uint32_t virtualSize = 0;
    std::uint32_t rawSize = 0;
    std::uint32_t rawOffset = 0;
};

std::optional<std::size_t> rvaToOffset(
    std::uint32_t rva,
    const std::vector<SectionView>& sections,
    std::size_t fileSize)
{
    for (const SectionView& section : sections)
    {
        const std::uint32_t span = std::max(section.virtualSize, section.rawSize);
        if (span == 0 || rva < section.virtualAddress)
        {
            continue;
        }
        const std::uint32_t relative = rva - section.virtualAddress;
        if (relative >= span || relative >= section.rawSize)
        {
            continue;
        }
        const std::uint64_t offset =
            static_cast<std::uint64_t>(section.rawOffset) + relative;
        if (offset >= fileSize || offset > std::numeric_limits<std::size_t>::max())
        {
            return std::nullopt;
        }
        return static_cast<std::size_t>(offset);
    }
    return std::nullopt;
}

std::optional<std::string> readCString(
    const std::vector<unsigned char>& bytes,
    std::size_t offset)
{
    if (offset >= bytes.size())
    {
        return std::nullopt;
    }
    std::string value;
    for (std::size_t index = offset; index < bytes.size(); ++index)
    {
        if (bytes[index] == 0)
        {
            return value;
        }
        value.push_back(static_cast<char>(bytes[index]));
        if (value.size() > 4096)
        {
            return std::nullopt;
        }
    }
    return std::nullopt;
}
}

bool PeExecutableVerifier::verify(const std::filesystem::path& path)
{
    return verifyImage(path, false);
}

bool PeExecutableVerifier::verifyDynamicLibrary(const std::filesystem::path& path)
{
    return verifyImage(path, true);
}

bool PeExecutableVerifier::verifyImage(
    const std::filesystem::path& path,
    bool dynamicLibrary)
{
    info_ = {};
    error_.clear();

    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error_ = "could not open PE executable";
        return false;
    }

    std::vector<unsigned char> bytes;
    char character = 0;
    while (stream.get(character))
    {
        bytes.push_back(static_cast<unsigned char>(character));
    }
    if (bytes.size() < 64)
    {
        error_ = "PE executable is smaller than the DOS header";
        return false;
    }
    if (bytes[0] != 'M' || bytes[1] != 'Z')
    {
        error_ = "PE executable does not begin with the MZ signature";
        return false;
    }

    const std::uint32_t peOffset32 = readU32(bytes, kDosPeOffsetField);
    const std::size_t peOffset = static_cast<std::size_t>(peOffset32);
    if (!rangeFits(peOffset, kPeSignatureSize + kCoffHeaderSize, bytes.size()))
    {
        error_ = "PE header offset points outside the file";
        return false;
    }
    if (bytes[peOffset] != 'P' || bytes[peOffset + 1] != 'E' ||
        bytes[peOffset + 2] != 0 || bytes[peOffset + 3] != 0)
    {
        error_ = "PE executable does not contain the PE\\0\\0 signature";
        return false;
    }

    const std::size_t coffOffset = peOffset + kPeSignatureSize;
    info_.machine = readU16(bytes, coffOffset);
    info_.sectionCount = readU16(bytes, coffOffset + 2);
    const std::uint16_t optionalHeaderSize = readU16(bytes, coffOffset + 16);
    info_.characteristics = readU16(bytes, coffOffset + 18);

    if (info_.machine != kImageFileMachineAmd64)
    {
        error_ = "PE machine is not AMD64 (0x8664)";
        return false;
    }
    if (info_.sectionCount == 0)
    {
        error_ = "PE executable contains no sections";
        return false;
    }
    if ((info_.characteristics & kImageFileExecutableImage) == 0)
    {
        error_ = "PE image is not marked executable";
        return false;
    }

    const std::size_t optionalOffset = coffOffset + kCoffHeaderSize;
    const std::size_t minimumOptionalSize =
        kPe32PlusDataDirectoryOffset + ((kClrDirectoryIndex + 1) * 8);
    if (optionalHeaderSize < minimumOptionalSize ||
        !rangeFits(optionalOffset, optionalHeaderSize, bytes.size()))
    {
        error_ = "PE32+ optional header is truncated";
        return false;
    }

    info_.optionalHeaderMagic = readU16(bytes, optionalOffset);
    info_.entryPointRva = readU32(bytes, optionalOffset + 16);
    info_.subsystem = readU16(bytes, optionalOffset + 68);
    info_.exportDirectoryRva = readU32(
        bytes,
        optionalOffset + kPe32PlusDataDirectoryOffset + (kExportDirectoryIndex * 8));
    info_.exportDirectorySize = readU32(
        bytes,
        optionalOffset + kPe32PlusDataDirectoryOffset + (kExportDirectoryIndex * 8) + 4);
    info_.importDirectoryRva = readU32(
        bytes,
        optionalOffset + kPe32PlusDataDirectoryOffset + (kImportDirectoryIndex * 8));
    info_.exceptionDirectoryRva = readU32(
        bytes,
        optionalOffset + kPe32PlusDataDirectoryOffset + (kExceptionDirectoryIndex * 8));
    info_.exceptionDirectorySize = readU32(
        bytes,
        optionalOffset + kPe32PlusDataDirectoryOffset + (kExceptionDirectoryIndex * 8) + 4);
    info_.clrDirectoryRva = readU32(
        bytes,
        optionalOffset + kPe32PlusDataDirectoryOffset + (kClrDirectoryIndex * 8));

    if (info_.optionalHeaderMagic != kPe32PlusMagic)
    {
        error_ = "PE optional header is not PE32+ (0x20B)";
        return false;
    }
    if (dynamicLibrary)
    {
        if ((info_.characteristics & kImageFileDll) == 0)
        {
            error_ = "PE image is not marked as a DLL";
            return false;
        }
        if (info_.exportDirectoryRva == 0 || info_.exportDirectorySize == 0)
        {
            error_ = "PE DLL has no export directory";
            return false;
        }
    }
    else
    {
        if ((info_.characteristics & kImageFileDll) != 0)
        {
            error_ = "PE executable is unexpectedly marked as a DLL";
            return false;
        }
        if (info_.entryPointRva == 0)
        {
            error_ = "PE executable has a zero entry-point RVA";
            return false;
        }
        if (info_.subsystem != kWindowsCuiSubsystem && info_.subsystem != kWindowsGuiSubsystem)
        {
            error_ = "PE executable subsystem is neither Windows GUI nor Windows CUI";
            return false;
        }
        if (info_.importDirectoryRva == 0)
        {
            error_ = "PE executable has no import directory";
            return false;
        }
    }
    if (info_.clrDirectoryRva != 0)
    {
        error_ = "PE executable unexpectedly contains a CLR runtime header";
        return false;
    }

    const std::size_t sectionTable = optionalOffset + optionalHeaderSize;
    const std::size_t sectionTableSize =
        static_cast<std::size_t>(info_.sectionCount) * kSectionHeaderSize;
    if (!rangeFits(sectionTable, sectionTableSize, bytes.size()))
    {
        error_ = "PE section table is truncated";
        return false;
    }

    std::vector<SectionView> sections;
    sections.reserve(info_.sectionCount);
    for (std::uint16_t index = 0; index < info_.sectionCount; ++index)
    {
        const std::size_t offset = sectionTable +
            (static_cast<std::size_t>(index) * kSectionHeaderSize);
        sections.push_back(SectionView{
            readU32(bytes, offset + 12),
            readU32(bytes, offset + 8),
            readU32(bytes, offset + 16),
            readU32(bytes, offset + 20)});
    }

    if (info_.exportDirectoryRva != 0)
    {
        const std::optional<std::size_t> exportOffset =
            rvaToOffset(info_.exportDirectoryRva, sections, bytes.size());
        if (!exportOffset.has_value() || !rangeFits(*exportOffset, 40, bytes.size()))
        {
            error_ = "PE export directory RVA does not map to file data";
            return false;
        }
        const std::uint32_t nameCount = readU32(bytes, *exportOffset + 24);
        const std::uint32_t addressOfNamesRva = readU32(bytes, *exportOffset + 32);
        if (nameCount > 65536)
        {
            error_ = "PE export directory contains an unreasonable number of names";
            return false;
        }
        if (nameCount != 0)
        {
            const std::optional<std::size_t> namesOffset =
                rvaToOffset(addressOfNamesRva, sections, bytes.size());
            if (!namesOffset.has_value() ||
                !rangeFits(*namesOffset, static_cast<std::size_t>(nameCount) * 4, bytes.size()))
            {
                error_ = "PE export name table RVA does not map to file data";
                return false;
            }
            for (std::uint32_t index = 0; index < nameCount; ++index)
            {
                const std::uint32_t nameRva = readU32(bytes, *namesOffset + (index * 4));
                const std::optional<std::size_t> nameOffset =
                    rvaToOffset(nameRva, sections, bytes.size());
                if (!nameOffset.has_value())
                {
                    error_ = "PE exported symbol name RVA does not map to file data";
                    return false;
                }
                const std::optional<std::string> symbolName = readCString(bytes, *nameOffset);
                if (!symbolName.has_value() || symbolName->empty())
                {
                    error_ = "PE exported symbol name is invalid";
                    return false;
                }
                info_.exportedSymbols.push_back(*symbolName);
            }
        }
    }

    if (info_.importDirectoryRva == 0)
    {
        return true;
    }

    const std::optional<std::size_t> importOffset =
        rvaToOffset(info_.importDirectoryRva, sections, bytes.size());
    if (!importOffset.has_value())
    {
        error_ = "PE import directory RVA does not map to file data";
        return false;
    }

    std::size_t descriptorOffset = *importOffset;
    for (std::size_t descriptorCount = 0; descriptorCount < 128; ++descriptorCount)
    {
        if (!rangeFits(descriptorOffset, kImportDescriptorSize, bytes.size()))
        {
            error_ = "PE import descriptor table is truncated";
            return false;
        }

        const std::uint32_t originalFirstThunk = readU32(bytes, descriptorOffset);
        const std::uint32_t timeDateStamp = readU32(bytes, descriptorOffset + 4);
        const std::uint32_t forwarderChain = readU32(bytes, descriptorOffset + 8);
        const std::uint32_t nameRva = readU32(bytes, descriptorOffset + 12);
        const std::uint32_t firstThunk = readU32(bytes, descriptorOffset + 16);
        if (originalFirstThunk == 0 && timeDateStamp == 0 && forwarderChain == 0 &&
            nameRva == 0 && firstThunk == 0)
        {
            break;
        }

        const std::optional<std::size_t> nameOffset =
            rvaToOffset(nameRva, sections, bytes.size());
        if (!nameOffset.has_value())
        {
            error_ = "PE import library name RVA does not map to file data";
            return false;
        }
        const std::optional<std::string> libraryName = readCString(bytes, *nameOffset);
        if (!libraryName.has_value() || libraryName->empty())
        {
            error_ = "PE import library name is invalid";
            return false;
        }
        info_.importedLibraries.push_back(*libraryName);

        const std::uint32_t thunkRva = originalFirstThunk != 0 ? originalFirstThunk : firstThunk;
        const std::optional<std::size_t> thunkOffset = rvaToOffset(thunkRva, sections, bytes.size());
        if (!thunkOffset.has_value())
        {
            error_ = "PE import thunk RVA does not map to file data";
            return false;
        }
        constexpr std::uint64_t kImportByOrdinal64 = 0x8000000000000000ULL;
        for (std::size_t thunkIndex = 0; thunkIndex < 4096; ++thunkIndex)
        {
            const std::size_t entryOffset = *thunkOffset + (thunkIndex * 8);
            if (!rangeFits(entryOffset, 8, bytes.size()))
            {
                error_ = "PE import thunk table is truncated";
                return false;
            }
            const std::uint64_t thunk = readU64(bytes, entryOffset);
            if (thunk == 0)
            {
                break;
            }
            if ((thunk & kImportByOrdinal64) != 0)
            {
                continue;
            }
            if (thunk > std::numeric_limits<std::uint32_t>::max())
            {
                error_ = "PE import-by-name RVA exceeds the PE32+ address range";
                return false;
            }
            const std::optional<std::size_t> hintNameOffset =
                rvaToOffset(static_cast<std::uint32_t>(thunk), sections, bytes.size());
            if (!hintNameOffset.has_value() || !rangeFits(*hintNameOffset, 2, bytes.size()))
            {
                error_ = "PE import-by-name RVA does not map to file data";
                return false;
            }
            const std::optional<std::string> symbolName = readCString(bytes, *hintNameOffset + 2);
            if (!symbolName.has_value() || symbolName->empty())
            {
                error_ = "PE imported symbol name is invalid";
                return false;
            }
            info_.importedSymbols.push_back(PeImportedSymbol{*libraryName, *symbolName});
        }
        descriptorOffset += kImportDescriptorSize;
    }

    if (info_.importedLibraries.empty())
    {
        error_ = "PE import directory contains no imported libraries";
        return false;
    }
    return true;
}

const PeExecutableInfo& PeExecutableVerifier::info() const noexcept
{
    return info_;
}

const std::string& PeExecutableVerifier::error() const noexcept
{
    return error_;
}
}
