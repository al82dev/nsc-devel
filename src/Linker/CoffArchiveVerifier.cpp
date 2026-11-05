#include "nativesharp/Linker/CoffArchiveVerifier.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::linker
{
namespace
{
constexpr std::string_view kArchiveMagic = "!<arch>\n";
constexpr std::size_t kMemberHeaderSize = 60;
constexpr std::uint64_t kMaximumInspectedLinkerMember = 256ULL * 1024ULL * 1024ULL;
constexpr std::uint32_t kMaximumArchiveSymbols = 16U * 1024U * 1024U;

bool parseDecimalField(std::string_view text, std::uint64_t& value)
{
    value = 0;
    bool sawDigit = false;
    for (const char character : text)
    {
        if (character == ' ')
        {
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(character)))
        {
            return false;
        }
        sawDigit = true;
        const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U)
        {
            return false;
        }
        value = (value * 10U) + digit;
    }
    return sawDigit;
}

std::uint32_t readBigEndian32(const char* bytes)
{
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[0])) << 24U) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[1])) << 16U) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[2])) << 8U) |
        static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[3]));
}

bool readFirstLinkerMember(
    std::span<const char> bytes,
    std::vector<std::string>& symbolNames,
    std::string& error)
{
    if (bytes.size() < 4)
    {
        error = "COFF archive first linker member is truncated";
        return false;
    }
    const std::uint32_t symbolCount = readBigEndian32(bytes.data());
    if (symbolCount > kMaximumArchiveSymbols)
    {
        error = "COFF archive linker member exceeds the symbol-count safety limit";
        return false;
    }
    const std::uint64_t namesOffset64 =
        4ULL + (static_cast<std::uint64_t>(symbolCount) * 4ULL);
    if (namesOffset64 > bytes.size())
    {
        error = "COFF archive first linker member has a truncated symbol-offset table";
        return false;
    }

    std::size_t cursor = static_cast<std::size_t>(namesOffset64);
    symbolNames.reserve(symbolCount);
    for (std::uint32_t index = 0; index < symbolCount; ++index)
    {
        const std::size_t start = cursor;
        while (cursor < bytes.size() && bytes[cursor] != '\0') ++cursor;
        if (cursor == bytes.size())
        {
            error = "COFF archive first linker member has a truncated symbol name table";
            return false;
        }
        if (cursor == start)
        {
            error = "COFF archive first linker member contains an empty symbol name";
            return false;
        }
        symbolNames.emplace_back(bytes.data() + start, cursor - start);
        ++cursor;
    }
    return true;
}

bool openCoffArchive(
    const std::filesystem::path& path,
    std::ifstream& stream,
    std::streamoff& fileSize,
    std::string& error)
{
    stream.open(path, std::ios::binary);
    if (!stream)
    {
        error = "could not open COFF archive";
        return false;
    }
    stream.seekg(0, std::ios::end);
    fileSize = stream.tellg();
    stream.seekg(0, std::ios::beg);
    if (fileSize < static_cast<std::streamoff>(kArchiveMagic.size()))
    {
        error = "file is too small to be a COFF archive";
        return false;
    }
    std::array<char, 8> magic{};
    stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (stream.gcount() != static_cast<std::streamsize>(magic.size()) ||
        std::string_view(magic.data(), magic.size()) != kArchiveMagic)
    {
        error = "file does not begin with the COFF archive magic";
        return false;
    }
    return true;
}

bool readArchiveMemberHeader(
    std::ifstream& stream,
    const std::streamoff fileSize,
    std::array<char, kMemberHeaderSize>& header,
    std::uint64_t& memberSize,
    std::string& error,
    bool& finished)
{
    stream.read(header.data(), static_cast<std::streamsize>(header.size()));
    if (stream.gcount() == 0 && stream.eof())
    {
        finished = true;
        return true;
    }
    if (stream.gcount() != static_cast<std::streamsize>(header.size()))
    {
        error = "COFF archive contains a truncated member header";
        return false;
    }
    if (header[58] != '`' || header[59] != '\n')
    {
        error = "COFF archive member header has an invalid terminator";
        return false;
    }
    if (!parseDecimalField(std::string_view(header.data() + 48, 10), memberSize))
    {
        error = "COFF archive member has an invalid size field";
        return false;
    }
    if (memberSize > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
    {
        error = "COFF archive member is too large to seek";
        return false;
    }
    const std::streamoff memberStart = stream.tellg();
    if (memberStart < 0 || memberStart > fileSize ||
        static_cast<std::uint64_t>(fileSize - memberStart) < memberSize)
    {
        error = "COFF archive member extends past end of file";
        return false;
    }
    return true;
}

bool consumeArchiveMember(
    std::ifstream& stream,
    const std::array<char, kMemberHeaderSize>& header,
    const std::uint64_t memberSize,
    bool& readLinkerSymbols,
    CoffArchiveInfo& info,
    std::string& error)
{
    std::string_view memberName(header.data(), 16);
    const std::size_t nameEnd = memberName.find(' ');
    if (nameEnd != std::string_view::npos) memberName = memberName.substr(0, nameEnd);
    if (!readLinkerSymbols && memberName == "/")
    {
        if (memberSize > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
            memberSize > kMaximumInspectedLinkerMember)
        {
            error = "COFF archive linker member exceeds the 256 MiB inspection limit";
            return false;
        }
        std::vector<char> bytes(static_cast<std::size_t>(memberSize));
        stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (stream.gcount() != static_cast<std::streamsize>(bytes.size()) ||
            !readFirstLinkerMember(bytes, info.symbolNames, error))
        {
            if (error.empty()) error = "COFF archive first linker member is truncated";
            return false;
        }
        readLinkerSymbols = true;
    }
    else
    {
        stream.seekg(static_cast<std::streamoff>(memberSize), std::ios::cur);
        if (!stream)
        {
            error = "COFF archive member extends past end of file";
            return false;
        }
    }
    if ((memberSize & 1U) != 0U)
    {
        stream.seekg(1, std::ios::cur);
        if (!stream)
        {
            error = "COFF archive alignment byte is missing";
            return false;
        }
    }
    ++info.memberCount;
    return true;
}

std::uint16_t little16(const unsigned char* p)
{
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8U));
}

std::uint32_t little32(const unsigned char* p)
{
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8U) |
           (static_cast<std::uint32_t>(p[2]) << 16U) | (static_cast<std::uint32_t>(p[3]) << 24U);
}

// Resolve the Name relocation of a classic AMD64 COFF import descriptor. The
// descriptor and its target must be inside this archive member; ordinary COFF
// objects without .idata$2 are left untouched.
bool readLongImportNames(std::ifstream& stream, std::streamoff start, std::uint64_t memberSize,
                         const std::array<unsigned char, 20>& header,
                         std::vector<std::string>& names, bool& hasLongThunk, std::string& error)
{
    if (little16(header.data()) != 0x8664) return true;
    const std::uint64_t count = little16(header.data() + 2);
    if (count == 0) return true;
    const std::uint64_t sectionOffset = 20ULL + little16(header.data() + 16);
    const auto readAt = [&](std::uint64_t offset, std::span<unsigned char> out) {
        if (offset > memberSize || out.size() > memberSize - offset) return false;
        stream.seekg(start + static_cast<std::streamoff>(offset));
        stream.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
        return static_cast<bool>(stream);
    };
    if (sectionOffset > memberSize || count * 40ULL > memberSize - sectionOffset)
    { error = "COFF object section table is truncated"; return false; }
    std::vector<unsigned char> sections(static_cast<std::size_t>(count * 40ULL));
    if (!readAt(sectionOffset, sections)) { error = "cannot read COFF object sections"; return false; }
    const std::uint64_t symbolOffset = little32(header.data() + 8);
    const std::uint64_t symbolCount = little32(header.data() + 12);
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto* section = sections.data() + index * 40;
        if ((std::memcmp(section, ".idata$4", 8) == 0 || std::memcmp(section, ".idata$5", 8) == 0) &&
            little16(section + 32) != 0)
            hasLongThunk = true;
        if (std::memcmp(section, ".idata$2", 8) != 0) continue;
        const std::uint64_t dataSize = little32(section + 16);
        const std::uint64_t dataOffset = little32(section + 20);
        const std::uint64_t relocationOffset = little32(section + 24);
        const std::uint64_t relocationCount = little16(section + 32);
        if (dataSize == 0 || dataSize % 20 != 0 || dataSize > 1024ULL * 1024ULL ||
            relocationCount == 0xffff || relocationOffset > memberSize ||
            relocationCount * 10ULL > memberSize - relocationOffset ||
            symbolOffset > memberSize || symbolCount * 18ULL > memberSize - symbolOffset)
        { error = "COFF long import descriptor has invalid bounds"; return false; }
        std::vector<unsigned char> data(static_cast<std::size_t>(dataSize));
        std::vector<unsigned char> relocations(static_cast<std::size_t>(relocationCount * 10ULL));
        if (!readAt(dataOffset, data) || !readAt(relocationOffset, relocations))
        { error = "COFF long import descriptor is truncated"; return false; }
        std::vector<const unsigned char*> nameRelocations(data.size() / 20, nullptr);
        std::vector<bool> hasRelocation(data.size() / 20, false);
        for (std::size_t r = 0; r < relocations.size(); r += 10)
        {
            const auto address = little32(relocations.data() + r);
            if (address >= data.size())
            { error = "COFF long import relocation is outside descriptor section"; return false; }
            const std::size_t descriptorIndex = address / 20U;
            hasRelocation[descriptorIndex] = true;
            if (address % 20U != 12) continue;
            if (nameRelocations[descriptorIndex])
            { error = "COFF long import has duplicate name relocation"; return false; }
            nameRelocations[descriptorIndex] = relocations.data() + r;
        }
        for (std::size_t descriptor = 0; descriptor < data.size(); descriptor += 20)
        {
            const auto* nameRelocation = nameRelocations[descriptor / 20];
            if (!nameRelocation)
            {
                const bool zero = std::all_of(data.begin() + descriptor, data.begin() + descriptor + 20,
                                              [](unsigned char c) { return c == 0; });
                if (zero && !hasRelocation[descriptor / 20]) continue;
                error = "COFF long import lacks a name relocation";
                return false;
            }
            const auto symbolIndex = little32(nameRelocation + 4);
            if (little16(nameRelocation + 8) != 3 || symbolIndex >= symbolCount)
            { error = "COFF long import has an invalid name relocation"; return false; }
            std::array<unsigned char, 18> symbol{};
            if (!readAt(symbolOffset + static_cast<std::uint64_t>(symbolIndex) * 18ULL, symbol))
            { error = "COFF long import name symbol is truncated"; return false; }
            const std::uint16_t targetNumber = little16(symbol.data() + 12);
            if (targetNumber == 0 || targetNumber > count)
            { error = "COFF long import name symbol has an invalid section"; return false; }
            const auto* target = sections.data() + (targetNumber - 1U) * 40U;
            if (std::memcmp(target, ".idata$6", 8) != 0)
            { error = "COFF long import name is not in .idata$6"; return false; }
            const std::uint64_t targetSize = little32(target + 16);
            const std::uint64_t targetOffset = little32(target + 20);
            const std::uint64_t nameOffset = static_cast<std::uint64_t>(little32(symbol.data() + 8)) +
                                             little32(data.data() + descriptor + 12);
            if (nameOffset >= targetSize)
            { error = "COFF long import name is outside its section"; return false; }
            const auto length = static_cast<std::size_t>(std::min<std::uint64_t>(targetSize - nameOffset, 4096));
            std::vector<unsigned char> name(length);
            if (!readAt(targetOffset + nameOffset, name))
            { error = "COFF long import name is truncated"; return false; }
            const auto end = std::find(name.begin(), name.end(), 0);
            if (end == name.begin() || end == name.end())
            { error = "COFF long import has invalid DLL name"; return false; }
            names.emplace_back(name.begin(), end);
        }
    }
    return true;
}
}

bool CoffArchiveVerifier::verify(const std::filesystem::path& path)
{
    info_ = {};
    error_.clear();

    std::ifstream stream;
    std::streamoff fileSize = 0;
    if (!openCoffArchive(path, stream, fileSize, error_)) return false;

    bool readLinkerSymbols = false;
    while (true)
    {
        std::array<char, kMemberHeaderSize> header{};
        std::uint64_t memberSize = 0;
        bool finished = false;
        if (!readArchiveMemberHeader(stream, fileSize, header, memberSize, error_, finished)) return false;
        if (finished) break;
        if (!consumeArchiveMember(stream, header, memberSize, readLinkerSymbols, info_, error_)) return false;
    }

    if (info_.memberCount == 0)
    {
        error_ = "COFF archive contains no members";
        return false;
    }
    return true;
}

bool CoffArchiveVerifier::readImportDllNames(const std::filesystem::path& path,
                                              std::vector<std::string>& names)
{
    error_.clear();
    std::ifstream stream;
    std::streamoff fileSize = 0;
    if (!openCoffArchive(path, stream, fileSize, error_)) return false;
    std::vector<std::string> result;
    std::vector<std::string> descriptorNames;
    bool hasLongThunk = false;
    while (true)
    {
        std::array<char, kMemberHeaderSize> header{};
        std::uint64_t memberSize = 0;
        bool finished = false;
        if (!readArchiveMemberHeader(stream, fileSize, header, memberSize, error_, finished)) return false;
        if (finished) break;
        const std::streamoff start = stream.tellg();
        if (memberSize >= 20)
        {
            std::array<unsigned char, 20> importHeader{};
            stream.read(reinterpret_cast<char*>(importHeader.data()), importHeader.size());
            if (!stream) { error_ = "COFF archive import header is truncated"; return false; }
            // A version-zero short import has Sig1=0 and Sig2=0xffff. Bigobj
            // uses the same signature with a later version and is not an import.
            if (importHeader[0] == 0 && importHeader[1] == 0 &&
                importHeader[2] == 0xff && importHeader[3] == 0xff &&
                importHeader[4] == 0 && importHeader[5] == 0)
            {
                const std::uint32_t dataSize = static_cast<std::uint32_t>(importHeader[12]) |
                    (static_cast<std::uint32_t>(importHeader[13]) << 8U) |
                    (static_cast<std::uint32_t>(importHeader[14]) << 16U) |
                    (static_cast<std::uint32_t>(importHeader[15]) << 24U);
                if (dataSize < 4 || dataSize > memberSize - 20 || dataSize > 16U * 1024U * 1024U)
                { error_ = "COFF short import has invalid data size"; return false; }
                std::vector<char> data(dataSize);
                stream.read(data.data(), static_cast<std::streamsize>(data.size()));
                if (!stream) { error_ = "COFF short import data is truncated"; return false; }
                const auto symbolEnd = std::find(data.begin(), data.end(), '\0');
                if (symbolEnd == data.begin() || symbolEnd == data.end())
                { error_ = "COFF short import has invalid symbol name"; return false; }
                const auto dllStart = std::next(symbolEnd);
                const auto dllEnd = std::find(dllStart, data.end(), '\0');
                if (dllStart == dllEnd || dllEnd == data.end())
                { error_ = "COFF short import has invalid DLL name"; return false; }
                result.emplace_back(dllStart, dllEnd);
            }
            else if (!readLongImportNames(stream, start, memberSize, importHeader, descriptorNames, hasLongThunk, error_))
                return false;
        }
        stream.seekg(start + static_cast<std::streamoff>(memberSize) + static_cast<std::streamoff>(memberSize & 1U));
        if (!stream || stream.tellg() > fileSize)
        { error_ = "COFF archive member alignment extends past end of file"; return false; }
    }
    if (hasLongThunk)
    {
        error_ = "COFF long import thunk requires native linking support";
        return false;
    }
    const auto equalName = [](const std::string& a, const std::string& b) {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
            [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
    };
    for (const auto& descriptor : descriptorNames)
        if (std::none_of(result.begin(), result.end(),
                         [&](const auto& shortName) { return equalName(descriptor, shortName); }))
        {
            error_ = "COFF long import has no matching short entry; native linking is not yet supported for '" +
                     descriptor + "'";
            return false;
        }
    names = std::move(result);
    return true;
}

const CoffArchiveInfo& CoffArchiveVerifier::info() const noexcept
{
    return info_;
}

const std::string& CoffArchiveVerifier::error() const noexcept
{
    return error_;
}
}
