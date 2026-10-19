#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace nativesharp::interop
{
class ByteReader final
{
public:
    explicit ByteReader(std::span<const std::uint8_t> bytes);
    bool u8(std::uint8_t& value);
    bool u16(std::uint16_t& value);
    bool u32(std::uint32_t& value);
    bool u64(std::uint64_t& value);
    bool text(std::string& value);
    bool raw(std::span<std::uint8_t> destination);
    std::size_t remaining() const noexcept;
    bool atEnd() const noexcept;

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_ = 0;
};
}
