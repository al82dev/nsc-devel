#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct PointerContract final
{
    bool nullable = true;
    bool mutablePointee = true;
    bool hasProvenance = true;
    std::size_t knownAlignment = 1;
    unsigned addressSpace = 0;
};
}
