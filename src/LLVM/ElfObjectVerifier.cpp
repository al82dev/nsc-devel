#include "nativesharp/LLVM/ElfObjectVerifier.h"

#include <array>
#include <cstddef>
#include <fstream>
#include <system_error>

namespace nativesharp::llvm_backend
{
namespace
{
constexpr std::size_t kElfHeaderSize = 64;
constexpr std::uint16_t kRelocatableType = 1;
constexpr std::uint16_t kX86_64Machine = 62;
constexpr std::uint16_t kSectionHeaderSize = 64;

std::uint16_t readU16(const std::array<unsigned char, kElfHeaderSize>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

std::uint64_t readU64(const std::array<unsigned char, kElfHeaderSize>& bytes, std::size_t offset)
{
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8; ++index)
    {
        value |= static_cast<std::uint64_t>(bytes[offset + index]) << (8 * index);
    }
    return value;
}
}

bool ElfObjectVerifier::verify(const std::filesystem::path& path)
{
    info_ = {};
    error_.clear();
    std::error_code sizeError;
    const std::uintmax_t fileSize = std::filesystem::file_size(path, sizeError);
    if (sizeError)
    {
        error_ = "cannot read the size of ELF object '" + path.string() + "'";
        return false;
    }
    if (fileSize < kElfHeaderSize)
    {
        error_ = "ELF object is smaller than its header";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    std::array<unsigned char, kElfHeaderSize> header{};
    if (!input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size())))
    {
        error_ = "cannot read the header of ELF object '" + path.string() + "'";
        return false;
    }
    if (header[0] != 0x7f || header[1] != 'E' || header[2] != 'L' || header[3] != 'F')
    {
        error_ = "ELF object signature is missing";
        return false;
    }
    if (header[4] != 2 || header[5] != 1)
    {
        error_ = "ELF object is not 64-bit little-endian";
        return false;
    }
    if (readU16(header, 16) != kRelocatableType)
    {
        error_ = "ELF object is not relocatable";
        return false;
    }
    info_.machine = readU16(header, 18);
    if (info_.machine != kX86_64Machine)
    {
        error_ = "ELF object machine is not x86-64 (62)";
        return false;
    }
    const std::uint64_t sectionTableOffset = readU64(header, 40);
    const std::uint16_t sectionHeaderSize = readU16(header, 58);
    info_.sectionCount = readU16(header, 60);
    if (info_.sectionCount == 0 || sectionHeaderSize != kSectionHeaderSize)
    {
        error_ = "ELF object section header table is malformed";
        return false;
    }
    const std::uint64_t tableSize = static_cast<std::uint64_t>(info_.sectionCount) * sectionHeaderSize;
    if (sectionTableOffset > fileSize || tableSize > fileSize - sectionTableOffset)
    {
        error_ = "ELF object section header table lies outside the file";
        return false;
    }
    return true;
}

const ElfObjectInfo& ElfObjectVerifier::info() const noexcept
{
    return info_;
}

const std::string& ElfObjectVerifier::error() const noexcept
{
    return error_;
}
}
