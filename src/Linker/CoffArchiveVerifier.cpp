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
}

bool CoffArchiveVerifier::verify(const std::filesystem::path& path)
{
    info_ = {};
    error_.clear();

    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error_ = "could not open COFF archive";
        return false;
    }

    stream.seekg(0, std::ios::end);
    const std::streamoff fileSize = stream.tellg();
    stream.seekg(0, std::ios::beg);
    if (fileSize < static_cast<std::streamoff>(kArchiveMagic.size()))
    {
        error_ = "file is too small to be a COFF archive";
        return false;
    }

    std::array<char, 8> magic{};
    stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (stream.gcount() != static_cast<std::streamsize>(magic.size()) ||
        std::string_view(magic.data(), magic.size()) != kArchiveMagic)
    {
        error_ = "file does not begin with the COFF archive magic";
        return false;
    }

    bool readLinkerSymbols = false;
    while (true)
    {
        std::array<char, kMemberHeaderSize> header{};
        stream.read(header.data(), static_cast<std::streamsize>(header.size()));
        if (stream.gcount() == 0 && stream.eof())
        {
            break;
        }
        if (stream.gcount() != static_cast<std::streamsize>(header.size()))
        {
            error_ = "COFF archive contains a truncated member header";
            return false;
        }
        if (header[58] != '`' || header[59] != '\n')
        {
            error_ = "COFF archive member header has an invalid terminator";
            return false;
        }

        std::uint64_t memberSize = 0;
        if (!parseDecimalField(std::string_view(header.data() + 48, 10), memberSize))
        {
            error_ = "COFF archive member has an invalid size field";
            return false;
        }
        if (memberSize > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
        {
            error_ = "COFF archive member is too large to seek";
            return false;
        }
        const std::streamoff memberStart = stream.tellg();
        if (memberStart < 0 || memberStart > fileSize ||
            static_cast<std::uint64_t>(fileSize - memberStart) < memberSize)
        {
            error_ = "COFF archive member extends past end of file";
            return false;
        }
        std::string_view memberName(header.data(), 16);
        const std::size_t nameEnd = memberName.find(' ');
        if (nameEnd != std::string_view::npos) memberName = memberName.substr(0, nameEnd);
        if (!readLinkerSymbols && memberName == "/")
        {
            if (memberSize > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
                memberSize > kMaximumInspectedLinkerMember)
            {
                error_ = "COFF archive linker member exceeds the 256 MiB inspection limit";
                return false;
            }
            std::vector<char> bytes(static_cast<std::size_t>(memberSize));
            stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            if (stream.gcount() != static_cast<std::streamsize>(bytes.size()) ||
                !readFirstLinkerMember(bytes, info_.symbolNames, error_))
            {
                if (error_.empty()) error_ = "COFF archive first linker member is truncated";
                return false;
            }
            readLinkerSymbols = true;
        }
        else
        {
            stream.seekg(static_cast<std::streamoff>(memberSize), std::ios::cur);
            if (!stream)
            {
                error_ = "COFF archive member extends past end of file";
                return false;
            }
        }
        if ((memberSize & 1U) != 0U)
        {
            stream.seekg(1, std::ios::cur);
            if (!stream)
            {
                error_ = "COFF archive alignment byte is missing";
                return false;
            }
        }
        ++info_.memberCount;
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
