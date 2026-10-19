#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace nativesharp::interop
{
class ByteWriter final
{
public:
    void u8(std::uint8_t value);
    void u16(std::uint16_t value);
    void u32(std::uint32_t value);
    void u64(std::uint64_t value);
    void raw(std::span<const std::uint8_t> bytes);
    void text(std::string_view value);
    const std::vector<std::uint8_t>& bytes() const noexcept;

private:
    std::vector<std::uint8_t> bytes_;
};
}
