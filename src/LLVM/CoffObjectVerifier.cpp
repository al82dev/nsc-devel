#include "nativesharp/LLVM/CoffObjectVerifier.h"

#include <array>
#include <fstream>

namespace nativesharp::llvm_backend
{
namespace
{
constexpr std::uint16_t kImageFileMachineAmd64 = 0x8664;
constexpr std::size_t kCoffHeaderSize = 20;

std::uint16_t readU16(const std::array<unsigned char, kCoffHeaderSize>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}
}

bool CoffObjectVerifier::verify(const std::filesystem::path& path)
{
    info_ = {};
    error_.clear();

    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error_ = "could not open COFF object file";
        return false;
    }

    std::array<unsigned char, kCoffHeaderSize> header{};
    stream.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (stream.gcount() != static_cast<std::streamsize>(header.size()))
    {
        error_ = "COFF object is smaller than the 20-byte file header";
        return false;
    }

    info_.machine = readU16(header, 0);
    info_.sectionCount = readU16(header, 2);
    info_.optionalHeaderSize = readU16(header, 16);

    if (info_.machine != kImageFileMachineAmd64)
    {
        error_ = "COFF object machine is not AMD64 (0x8664)";
        return false;
    }
    if (info_.sectionCount == 0)
    {
        error_ = "COFF object contains no sections";
        return false;
    }
    if (info_.optionalHeaderSize != 0)
    {
        error_ = "COFF relocatable object unexpectedly contains a PE optional header";
        return false;
    }
    return true;
}

const CoffObjectInfo& CoffObjectVerifier::info() const noexcept
{
    return info_;
}

const std::string& CoffObjectVerifier::error() const noexcept
{
    return error_;
}
}
