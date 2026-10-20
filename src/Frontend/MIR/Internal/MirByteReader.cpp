#include "MirByteReader.h"

#include <utility>

namespace nativesharp::frontend::internal
{
MirByteReader::MirByteReader(std::string_view bytes) : bytes_(bytes) {}

bool MirByteReader::u32(std::uint32_t& out)
{
    out = 0;
    for (unsigned shift = 0; shift < 32U; shift += 8U)
    {
        std::uint8_t value = 0;
        if (!byte(value)) return false;
        out |= static_cast<std::uint32_t>(value) << shift;
    }
    return true;
}

bool MirByteReader::u64(std::uint64_t& out)
{
    out = 0;
    for (unsigned shift = 0; shift < 64U; shift += 8U)
    {
        std::uint8_t value = 0;
        if (!byte(value)) return false;
        out |= static_cast<std::uint64_t>(value) << shift;
    }
    return true;
}

bool MirByteReader::string(std::string& out)
{
    std::uint32_t size = 0;
    if (!u32(size)) return false;
    if (position_ + size > bytes_.size()) return false;
    out.assign(bytes_.data() + position_, size);
    position_ += size;
    return true;
}

bool MirByteReader::stringVector(std::vector<std::string>& out)
{
    std::uint32_t count = 0;
    if (!u32(count)) return false;
    out.clear();
    out.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        std::string value;
        if (!string(value)) return false;
        out.push_back(std::move(value));
    }
    return true;
}

bool MirByteReader::byte(std::uint8_t& out)
{
    if (position_ >= bytes_.size()) return false;
    out = static_cast<std::uint8_t>(bytes_[position_++]);
    return true;
}

std::string_view MirByteReader::remaining() const noexcept
{
    return bytes_.substr(position_);
}
}
