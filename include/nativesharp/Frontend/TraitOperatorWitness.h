#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::frontend
{
// NativeSharp 3.0 T05-F07. A trait's static operator requirement is resolved to one concrete
// witness while interface/coherence information is complete. Operator expressions consume this
// table; they never rediscover a trait contract by matching ABI method names later in Sema.
struct TraitOperatorWitness final
{
    std::size_t implementingTypeOrdinal = 0;
    std::size_t traitTypeOrdinal = 0;
    std::size_t requirementMemberOrdinal = 0;
    std::size_t implementationMemberOrdinal = 0;
    std::string langItem;
};
}
