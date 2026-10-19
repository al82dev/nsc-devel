#pragma once

#include <cstddef>
#include <vector>

namespace nativesharp::frontend
{
struct InterfaceTypeInfo final
{
    std::size_t typeOrdinal = 0;
    bool isTrait = false;
    std::vector<std::size_t> directBaseInterfaceOrdinals;
    std::vector<std::size_t> transitiveBaseInterfaceOrdinals;
    std::vector<std::size_t> effectiveMethodMemberOrdinals;
    std::vector<std::size_t> requiredStaticMemberOrdinals;
};
}
