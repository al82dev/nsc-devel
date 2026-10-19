#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace nativesharp::frontend
{
struct ClassInterfaceTablePlan final
{
    std::size_t classTypeOrdinal = 0;
    std::size_t interfaceTypeOrdinal = 0;
    std::uint64_t stableInterfaceTypeId = 0;
    std::vector<std::size_t> interfaceMethodMemberOrdinals;
    std::vector<std::optional<std::size_t>> implementationMemberOrdinals;
};
}
