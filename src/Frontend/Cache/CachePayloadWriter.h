#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend::internal
{
class CachePayloadWriter final
{
public:
    static void byte(std::string& out, std::uint8_t value);
    static void u32(std::string& out, std::uint32_t value);
    static void u64(std::string& out, std::uint64_t value);
    static void string(std::string& out, std::string_view value);
    static void stringVector(std::string& out, const std::vector<std::string>& values);
    static void sizeVector(std::string& out, const std::vector<std::size_t>& values);
    static void int64Vector(std::string& out, const std::vector<std::int64_t>& values);
    static void u32Vector(std::string& out, const std::vector<std::uint32_t>& values);
};
}
