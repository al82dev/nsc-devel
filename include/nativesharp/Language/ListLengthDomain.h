#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct ListLengthDomain final
{
    std::size_t length = 0;
    bool hasRest = false;

    bool operator==(const ListLengthDomain&) const = default;
};
}
