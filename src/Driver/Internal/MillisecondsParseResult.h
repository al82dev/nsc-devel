#pragma once

#include <cstdint>

namespace nativesharp::driver
{
namespace
{
enum class MillisecondsParseResult : std::uint8_t
{
    Valid,
    Malformed,
    OutOfRange
};
} // namespace
} // namespace nativesharp::driver
