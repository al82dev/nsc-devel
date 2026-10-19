#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::language
{
struct IntrospectionMember final
{
    std::string name;
    std::string typeKey;
    bool isPublic = false;
    std::size_t sourceOrdinal = 0;
};
}
