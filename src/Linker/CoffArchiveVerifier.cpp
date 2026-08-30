#include "nativesharp/Linker/CoffArchiveVerifier.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>

namespace nativesharp::linker
{
namespace
{
constexpr std::string_view kArchiveMagic = "!<arch>\n";
constexpr std::size_t kMemberHeaderSize = 60;

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

    std::array<char, 8> magic{};
    stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (stream.gcount() != static_cast<std::streamsize>(magic.size()) ||
        std::string_view(magic.data(), magic.size()) != kArchiveMagic)
    {
        error_ = "file does not begin with the COFF archive magic";
        return false;
    }

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
        stream.seekg(static_cast<std::streamoff>(memberSize), std::ios::cur);
        if (!stream)
        {
            error_ = "COFF archive member extends past end of file";
            return false;
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
