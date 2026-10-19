#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::language
{
struct VariantCase final
{
    std::string name;
    std::size_t payloadSize = 0;
    std::size_t payloadAlignment = 1;
};
}
