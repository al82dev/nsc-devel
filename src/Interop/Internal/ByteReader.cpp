#include "ByteReader.h"

#include <algorithm>

namespace nativesharp::interop
{
namespace
{
constexpr std::uint32_t kMaximumStringSize = 16U * 1024U * 1024U;
}

ByteReader::ByteReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

bool ByteReader::u8(std::uint8_t& value)
{
    if (position_ >= bytes_.size()) return false;
    value = bytes_[position_++];
    return true;
}

bool ByteReader::u16(std::uint16_t& value)
{
    std::uint8_t a = 0;
    std::uint8_t b = 0;
    if (!u8(a) || !u8(b)) return false;
    value = static_cast<std::uint16_t>(a) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(b) << 8U);
    return true;
}

bool ByteReader::u32(std::uint32_t& value)
{
    value = 0;
    for (unsigned shift = 0; shift < 32U; shift += 8U)
    {
        std::uint8_t byte = 0;
        if (!u8(byte)) return false;
        value |= static_cast<std::uint32_t>(byte) << shift;
    }
    return true;
}

bool ByteReader::u64(std::uint64_t& value)
{
    value = 0;
    for (unsigned shift = 0; shift < 64U; shift += 8U)
    {
        std::uint8_t byte = 0;
        if (!u8(byte)) return false;
        value |= static_cast<std::uint64_t>(byte) << shift;
    }
    return true;
}

bool ByteReader::text(std::string& value)
{
    std::uint32_t size = 0;
    if (!u32(size) || size > kMaximumStringSize || remaining() < size) return false;
    value.assign(
        reinterpret_cast<const char*>(bytes_.data() + position_),
        static_cast<std::size_t>(size));
    position_ += size;
    return true;
}

bool ByteReader::raw(std::span<std::uint8_t> destination)
{
    if (remaining() < destination.size()) return false;
    std::copy_n(
        bytes_.begin() + static_cast<std::ptrdiff_t>(position_),
        destination.size(),
        destination.begin());
    position_ += destination.size();
    return true;
}

std::size_t ByteReader::remaining() const noexcept
{
    return bytes_.size() - position_;
}

bool ByteReader::atEnd() const noexcept
{
    return position_ == bytes_.size();
}
}
