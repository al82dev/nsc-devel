#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend::internal
{
class ByteReader final
{
public:
    explicit ByteReader(std::string_view bytes);

    bool byte(std::uint8_t& out);
    bool u32(std::uint32_t& out);
    bool u64(std::uint64_t& out);
    bool string(std::string& out);
    bool stringVector(std::vector<std::string>& out);

private:
    std::string_view bytes_;
    std::size_t position_ = 0;
};
}
