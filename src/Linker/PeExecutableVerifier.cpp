#include "nativesharp/Linker/PeExecutableVerifier.h"
#include "Internal/SectionView.h"

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
constexpr std::size_t kDebugDirectoryIndex = 6;
constexpr std::size_t kClrDirectoryIndex = 14;
constexpr std::size_t kImportDescriptorSize = 20;
constexpr std::size_t kDebugDirectoryEntrySize = 28;
constexpr std::uint32_t kImageDebugTypeCodeView = 2;

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

bool readPeBytes(
    const std::filesystem::path& path,
    std::vector<unsigned char>& bytes,
    std::string& error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error = "could not open PE executable";
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff fileSize = stream.tellg();
    if (fileSize < 0 || static_cast<std::uintmax_t>(fileSize) >
            std::numeric_limits<std::size_t>::max())
    {
        error = "could not read PE executable";
        return false;
    }
    stream.seekg(0, std::ios::beg);
    if (!stream)
    {
        error = "could not read PE executable";
        return false;
    }
    bytes.resize(static_cast<std::size_t>(fileSize));
    constexpr std::size_t ReadBlockSize = 64U * 1024U;
    for (std::size_t offset = 0; offset < bytes.size(); offset += ReadBlockSize)
    {
        const std::size_t blockSize = std::min(ReadBlockSize, bytes.size() - offset);
        stream.read(
            reinterpret_cast<char*>(bytes.data() + offset),
            static_cast<std::streamsize>(blockSize));
        if (stream.gcount() != static_cast<std::streamsize>(blockSize))
        {
            error = "could not read PE executable";
            return false;
        }
    }
    if (bytes.size() < 64)
    {
        error = "PE executable is smaller than the DOS header";
        return false;
    }
    if (bytes[0] != 'M' || bytes[1] != 'Z')
    {
        error = "PE executable does not begin with the MZ signature";
        return false;
    }
    return true;
}

bool readPeCoffHeader(
    const std::vector<unsigned char>& bytes,
    PeExecutableInfo& info,
    std::size_t& optionalOffset,
    std::uint16_t& optionalHeaderSize,
    std::string& error)
{
    const std::size_t peOffset = static_cast<std::size_t>(readU32(bytes, kDosPeOffsetField));
    if (!rangeFits(peOffset, kPeSignatureSize + kCoffHeaderSize, bytes.size()))
    {
        error = "PE header offset points outside the file";
        return false;
    }
    if (bytes[peOffset] != 'P' || bytes[peOffset + 1] != 'E' ||
        bytes[peOffset + 2] != 0 || bytes[peOffset + 3] != 0)
    {
        error = "PE executable does not contain the PE\\0\\0 signature";
        return false;
    }
    const std::size_t coffOffset = peOffset + kPeSignatureSize;
    info.machine = readU16(bytes, coffOffset);
    info.sectionCount = readU16(bytes, coffOffset + 2);
    optionalHeaderSize = readU16(bytes, coffOffset + 16);
    info.characteristics = readU16(bytes, coffOffset + 18);
    if (info.machine != kImageFileMachineAmd64)
    {
        error = "PE machine is not AMD64 (0x8664)";
        return false;
    }
    if (info.sectionCount == 0)
    {
        error = "PE executable contains no sections";
        return false;
    }
    if ((info.characteristics & kImageFileExecutableImage) == 0)
    {
        error = "PE image is not marked executable";
        return false;
    }
    optionalOffset = coffOffset + kCoffHeaderSize;
    const std::size_t minimumOptionalSize = kPe32PlusDataDirectoryOffset + ((kClrDirectoryIndex + 1) * 8);
    if (optionalHeaderSize < minimumOptionalSize ||
        !rangeFits(optionalOffset, optionalHeaderSize, bytes.size()))
    {
        error = "PE32+ optional header is truncated";
        return false;
    }
    return true;
}

void readPeOptionalHeader(
    const std::vector<unsigned char>& bytes,
    const std::size_t optionalOffset,
    PeExecutableInfo& info)
{
    info.optionalHeaderMagic = readU16(bytes, optionalOffset);
    info.entryPointRva = readU32(bytes, optionalOffset + 16);
    info.subsystem = readU16(bytes, optionalOffset + 68);
    const std::size_t directories = optionalOffset + kPe32PlusDataDirectoryOffset;
    info.exportDirectoryRva = readU32(bytes, directories + (kExportDirectoryIndex * 8));
    info.exportDirectorySize = readU32(bytes, directories + (kExportDirectoryIndex * 8) + 4);
    info.importDirectoryRva = readU32(bytes, directories + (kImportDirectoryIndex * 8));
    info.exceptionDirectoryRva = readU32(bytes, directories + (kExceptionDirectoryIndex * 8));
    info.exceptionDirectorySize = readU32(bytes, directories + (kExceptionDirectoryIndex * 8) + 4);
    info.debugDirectoryRva = readU32(bytes, directories + (kDebugDirectoryIndex * 8));
    info.debugDirectorySize = readU32(bytes, directories + (kDebugDirectoryIndex * 8) + 4);
    info.clrDirectoryRva = readU32(bytes, directories + (kClrDirectoryIndex * 8));
}

bool validatePeImageKind(
    const bool dynamicLibrary,
    const PeExecutableInfo& info,
    std::string& error)
{
    if (info.optionalHeaderMagic != kPe32PlusMagic)
    {
        error = "PE optional header is not PE32+ (0x20B)";
        return false;
    }
    if (dynamicLibrary)
    {
        if ((info.characteristics & kImageFileDll) == 0)
        {
            error = "PE image is not marked as a DLL";
            return false;
        }
        // A metadata-only NativeSharp product is still a valid PE DLL even when
        // its contract consists exclusively of enums or generic declarations.
        // Its absent export directory means exactly zero physical exports; the
        // caller compares that fact with NSMETA instead of requiring a fabricated
        // entry point here.
    }
    else
    {
        if ((info.characteristics & kImageFileDll) != 0)
        {
            error = "PE executable is unexpectedly marked as a DLL";
            return false;
        }
        if (info.entryPointRva == 0)
        {
            error = "PE executable has a zero entry-point RVA";
            return false;
        }
        if (info.subsystem != kWindowsCuiSubsystem && info.subsystem != kWindowsGuiSubsystem)
        {
            error = "PE executable subsystem is neither Windows GUI nor Windows CUI";
            return false;
        }
        if (info.importDirectoryRva == 0)
        {
            error = "PE executable has no import directory";
            return false;
        }
    }
    if (info.clrDirectoryRva != 0)
    {
        error = "PE executable unexpectedly contains a CLR runtime header";
        return false;
    }
    return true;
}

bool readPeSections(
    const std::vector<unsigned char>& bytes,
    const std::size_t optionalOffset,
    const std::uint16_t optionalHeaderSize,
    const PeExecutableInfo& info,
    std::vector<SectionView>& sections,
    std::string& error)
{
    const std::size_t sectionTable = optionalOffset + optionalHeaderSize;
    const std::size_t sectionTableSize = static_cast<std::size_t>(info.sectionCount) * kSectionHeaderSize;
    if (!rangeFits(sectionTable, sectionTableSize, bytes.size()))
    {
        error = "PE section table is truncated";
        return false;
    }
    sections.reserve(info.sectionCount);
    for (std::uint16_t index = 0; index < info.sectionCount; ++index)
    {
        const std::size_t offset = sectionTable + (static_cast<std::size_t>(index) * kSectionHeaderSize);
        sections.push_back(SectionView{
            readU32(bytes, offset + 12),
            readU32(bytes, offset + 8),
            readU32(bytes, offset + 16),
            readU32(bytes, offset + 20)});
    }
    return true;
}

bool readPeExports(
    const std::vector<unsigned char>& bytes,
    const std::vector<SectionView>& sections,
    PeExecutableInfo& info,
    std::string& error)
{
    if (info.exportDirectoryRva == 0) return true;
    const auto exportOffset = rvaToOffset(info.exportDirectoryRva, sections, bytes.size());
    if (!exportOffset.has_value() || !rangeFits(*exportOffset, 40, bytes.size()))
    {
        error = "PE export directory RVA does not map to file data";
        return false;
    }
    const std::uint32_t nameCount = readU32(bytes, *exportOffset + 24);
    const std::uint32_t addressOfNamesRva = readU32(bytes, *exportOffset + 32);
    if (nameCount > 65536)
    {
        error = "PE export directory contains an unreasonable number of names";
        return false;
    }
    if (nameCount == 0) return true;
    const auto namesOffset = rvaToOffset(addressOfNamesRva, sections, bytes.size());
    if (!namesOffset.has_value() || !rangeFits(*namesOffset, static_cast<std::size_t>(nameCount) * 4, bytes.size()))
    {
        error = "PE export name table RVA does not map to file data";
        return false;
    }
    for (std::uint32_t index = 0; index < nameCount; ++index)
    {
        const std::uint32_t nameRva = readU32(bytes, *namesOffset + (index * 4));
        const auto nameOffset = rvaToOffset(nameRva, sections, bytes.size());
        if (!nameOffset.has_value())
        {
            error = "PE exported symbol name RVA does not map to file data";
            return false;
        }
        const auto symbolName = readCString(bytes, *nameOffset);
        if (!symbolName.has_value() || symbolName->empty())
        {
            error = "PE exported symbol name is invalid";
            return false;
        }
        info.exportedSymbols.push_back(*symbolName);
    }
    return true;
}

bool readPeDebugDirectory(
    const std::vector<unsigned char>& bytes,
    const std::vector<SectionView>& sections,
    PeExecutableInfo& info,
    std::string& error)
{
    if (info.debugDirectoryRva == 0 && info.debugDirectorySize == 0) return true;
    if (info.debugDirectoryRva == 0 || info.debugDirectorySize == 0 ||
        info.debugDirectorySize % kDebugDirectoryEntrySize != 0)
    {
        error = "PE debug directory has an invalid RVA/size pair";
        return false;
    }
    const auto debugOffset = rvaToOffset(info.debugDirectoryRva, sections, bytes.size());
    if (!debugOffset.has_value() || !rangeFits(*debugOffset, info.debugDirectorySize, bytes.size()))
    {
        error = "PE debug directory RVA does not map to file data";
        return false;
    }
    const std::size_t entryCount = info.debugDirectorySize / kDebugDirectoryEntrySize;
    for (std::size_t index = 0; index < entryCount; ++index)
    {
        const std::size_t entryOffset = *debugOffset + (index * kDebugDirectoryEntrySize);
        if (readU32(bytes, entryOffset + 12) != kImageDebugTypeCodeView) continue;
        const std::uint32_t dataSize = readU32(bytes, entryOffset + 16);
        const std::uint32_t dataOffset = readU32(bytes, entryOffset + 24);
        if (dataSize < 25 || !rangeFits(dataOffset, dataSize, bytes.size()))
        {
            error = "PE CodeView debug record is truncated";
            return false;
        }
        if (bytes[dataOffset] != 'R' || bytes[dataOffset + 1] != 'S' ||
            bytes[dataOffset + 2] != 'D' || bytes[dataOffset + 3] != 'S')
            continue;
        if (info.hasCodeViewRsds)
        {
            error = "PE contains more than one RSDS CodeView record";
            return false;
        }
        info.hasCodeViewRsds = true;
        std::copy_n(bytes.begin() + dataOffset + 4, info.codeViewGuid.size(), info.codeViewGuid.begin());
        info.codeViewAge = readU32(bytes, dataOffset + 20);
        const auto pdbName = readCString(bytes, dataOffset + 24);
        if (!pdbName.has_value() || pdbName->empty() || pdbName->size() + 25 > dataSize)
        {
            error = "PE RSDS record has an invalid PDB file name";
            return false;
        }
        info.codeViewPdbFileName = *pdbName;
    }
    return true;
}

bool readPeImportThunks(
    const std::vector<unsigned char>& bytes,
    const std::vector<SectionView>& sections,
    const std::uint32_t thunkRva,
    const std::string& libraryName,
    PeExecutableInfo& info,
    std::string& error)
{
    const auto thunkOffset = rvaToOffset(thunkRva, sections, bytes.size());
    if (!thunkOffset.has_value())
    {
        error = "PE import thunk RVA does not map to file data";
        return false;
    }
    constexpr std::uint64_t kImportByOrdinal64 = 0x8000000000000000ULL;
    for (std::size_t thunkIndex = 0; thunkIndex < 4096; ++thunkIndex)
    {
        const std::size_t entryOffset = *thunkOffset + (thunkIndex * 8);
        if (!rangeFits(entryOffset, 8, bytes.size()))
        {
            error = "PE import thunk table is truncated";
            return false;
        }
        const std::uint64_t thunk = readU64(bytes, entryOffset);
        if (thunk == 0) break;
        if ((thunk & kImportByOrdinal64) != 0) continue;
        if (thunk > std::numeric_limits<std::uint32_t>::max())
        {
            error = "PE import-by-name RVA exceeds the PE32+ address range";
            return false;
        }
        const auto hintNameOffset = rvaToOffset(static_cast<std::uint32_t>(thunk), sections, bytes.size());
        if (!hintNameOffset.has_value() || !rangeFits(*hintNameOffset, 2, bytes.size()))
        {
            error = "PE import-by-name RVA does not map to file data";
            return false;
        }
        const auto symbolName = readCString(bytes, *hintNameOffset + 2);
        if (!symbolName.has_value() || symbolName->empty())
        {
            error = "PE imported symbol name is invalid";
            return false;
        }
        info.importedSymbols.push_back(PeImportedSymbol{libraryName, *symbolName});
    }
    return true;
}

bool readPeImports(
    const std::vector<unsigned char>& bytes,
    const std::vector<SectionView>& sections,
    PeExecutableInfo& info,
    std::string& error)
{
    if (info.importDirectoryRva == 0) return true;
    const auto importOffset = rvaToOffset(info.importDirectoryRva, sections, bytes.size());
    if (!importOffset.has_value())
    {
        error = "PE import directory RVA does not map to file data";
        return false;
    }
    std::size_t descriptorOffset = *importOffset;
    for (std::size_t descriptorCount = 0; descriptorCount < 128; ++descriptorCount)
    {
        if (!rangeFits(descriptorOffset, kImportDescriptorSize, bytes.size()))
        {
            error = "PE import descriptor table is truncated";
            return false;
        }
        const std::uint32_t originalFirstThunk = readU32(bytes, descriptorOffset);
        const std::uint32_t timeDateStamp = readU32(bytes, descriptorOffset + 4);
        const std::uint32_t forwarderChain = readU32(bytes, descriptorOffset + 8);
        const std::uint32_t nameRva = readU32(bytes, descriptorOffset + 12);
        const std::uint32_t firstThunk = readU32(bytes, descriptorOffset + 16);
        if (originalFirstThunk == 0 && timeDateStamp == 0 && forwarderChain == 0 &&
            nameRva == 0 && firstThunk == 0)
            break;
        const auto nameOffset = rvaToOffset(nameRva, sections, bytes.size());
        if (!nameOffset.has_value())
        {
            error = "PE import library name RVA does not map to file data";
            return false;
        }
        const auto libraryName = readCString(bytes, *nameOffset);
        if (!libraryName.has_value() || libraryName->empty())
        {
            error = "PE import library name is invalid";
            return false;
        }
        info.importedLibraries.push_back(*libraryName);
        const std::uint32_t thunkRva = originalFirstThunk != 0 ? originalFirstThunk : firstThunk;
        if (!readPeImportThunks(bytes, sections, thunkRva, *libraryName, info, error)) return false;
        descriptorOffset += kImportDescriptorSize;
    }
    if (info.importedLibraries.empty())
    {
        error = "PE import directory contains no imported libraries";
        return false;
    }
    return true;
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
    const bool dynamicLibrary)
{
    info_ = {};
    error_.clear();
    std::vector<unsigned char> bytes;
    if (!readPeBytes(path, bytes, error_)) return false;
    std::size_t optionalOffset = 0;
    std::uint16_t optionalHeaderSize = 0;
    if (!readPeCoffHeader(bytes, info_, optionalOffset, optionalHeaderSize, error_)) return false;
    readPeOptionalHeader(bytes, optionalOffset, info_);
    if (!validatePeImageKind(dynamicLibrary, info_, error_)) return false;
    std::vector<SectionView> sections;
    if (!readPeSections(bytes, optionalOffset, optionalHeaderSize, info_, sections, error_)) return false;
    if (!readPeExports(bytes, sections, info_, error_)) return false;
    if (!readPeDebugDirectory(bytes, sections, info_, error_)) return false;
    return readPeImports(bytes, sections, info_, error_);
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
