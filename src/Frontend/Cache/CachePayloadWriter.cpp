#include "CachePayloadWriter.h"

namespace nativesharp::frontend::internal
{
void CachePayloadWriter::byte(std::string& out, std::uint8_t value)
{
    out.push_back(static_cast<char>(value));
}

void CachePayloadWriter::u32(std::string& out, std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32U; shift += 8U)
        out.push_back(static_cast<char>((value >> shift) & 0xFFu));
}

void CachePayloadWriter::u64(std::string& out, std::uint64_t value)
{
    for (unsigned shift = 0; shift < 64U; shift += 8U)
        out.push_back(static_cast<char>((value >> shift) & 0xFFu));
}

void CachePayloadWriter::string(std::string& out, std::string_view value)
{
    u32(out, static_cast<std::uint32_t>(value.size()));
    out.append(value);
}

void CachePayloadWriter::stringVector(
    std::string& out,
    const std::vector<std::string>& values)
{
    u32(out, static_cast<std::uint32_t>(values.size()));
    for (const std::string& value : values) string(out, value);
}

void CachePayloadWriter::sizeVector(
    std::string& out,
    const std::vector<std::size_t>& values)
{
    u32(out, static_cast<std::uint32_t>(values.size()));
    for (const std::size_t value : values) u64(out, static_cast<std::uint64_t>(value));
}

void CachePayloadWriter::int64Vector(
    std::string& out,
    const std::vector<std::int64_t>& values)
{
    u32(out, static_cast<std::uint32_t>(values.size()));
    for (const std::int64_t value : values) u64(out, static_cast<std::uint64_t>(value));
}

void CachePayloadWriter::u32Vector(
    std::string& out,
    const std::vector<std::uint32_t>& values)
{
    u32(out, static_cast<std::uint32_t>(values.size()));
    for (const std::uint32_t value : values) u32(out, value);
}
}
