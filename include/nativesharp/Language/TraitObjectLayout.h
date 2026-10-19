#pragma once

#include <cstddef>

namespace nativesharp::language
{
struct TraitObjectLayout final
{
    std::size_t size;
    std::size_t alignment;
    bool descriptorInterfaceMap;
    bool nullIsZero;
    bool conversionAllocates;
};
}
