#pragma once

#include <cstdint>
#include <string>

namespace nativesharp::compilation
{
struct TypeNameSegment final
{
    std::string name;
    std::uint32_t arity = 0;

    bool operator==(const TypeNameSegment&) const noexcept = default;
};
}
