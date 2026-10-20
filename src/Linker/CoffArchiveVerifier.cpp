#include "nativesharp/Linker/CoffArchiveVerifier.h"

#include <array>
#include <cctype>
#include <cstdint>
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

const CoffArchiveInfo& CoffArchiveVerifier::info() const noexcept
{
    return info_;
}

const std::string& CoffArchiveVerifier::error() const noexcept
{
    return error_;
}
}
