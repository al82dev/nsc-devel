#include "ByteWriter.h"

namespace nativesharp::interop
{
void ByteWriter::u8(std::uint8_t value)
{
    bytes_.push_back(value);
}

void ByteWriter::u16(std::uint16_t value)
{
    u8(static_cast<std::uint8_t>(value & 0xffU));
    u8(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void ByteWriter::u32(std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32U; shift += 8U)
    {
        u8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void ByteWriter::u64(std::uint64_t value)
{
    for (unsigned shift = 0; shift < 64U; shift += 8U)
    {
        u8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void ByteWriter::raw(std::span<const std::uint8_t> bytes)
{
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
}

void ByteWriter::text(std::string_view value)
{
    u32(static_cast<std::uint32_t>(value.size()));
    bytes_.insert(bytes_.end(), value.begin(), value.end());
}

const std::vector<std::uint8_t>& ByteWriter::bytes() const noexcept
{
    return bytes_;
}
}
