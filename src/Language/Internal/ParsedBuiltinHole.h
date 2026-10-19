#pragma once

#include <cstddef>
#include <cstdint>

namespace nativesharp::language
{
namespace
{
struct ParsedBuiltinHole final
{
    bool boolean = false;
    bool booleanValue = false;
    bool negative = false;
    std::uint64_t magnitude = 0;
    char mode = 'D';
    std::size_t width = 0;
};
}
}
