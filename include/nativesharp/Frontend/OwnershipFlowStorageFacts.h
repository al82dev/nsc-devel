#pragma once

#include "nativesharp/Frontend/OwnershipFlowIntegerRange.h"

#include <cstddef>
#include <optional>

namespace nativesharp::frontend
{
// Flow facts for one dense named-storage slot. Empty optionals represent absence, so a snapshot
// copies one contiguous vector instead of three hash maps and one hash set.
struct OwnershipFlowStorageFacts final
{
    std::optional<std::size_t> activeUnionMember;
    bool nonNull = false;
    std::optional<OwnershipFlowIntegerRange> integerRange;
    std::optional<std::size_t> activeVariantCase;
};
}
