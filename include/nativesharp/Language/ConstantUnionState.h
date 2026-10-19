#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::language
{
struct ConstantUnionState final
{
    std::string activeMember;
    std::size_t storageSize = 0;
};
}
