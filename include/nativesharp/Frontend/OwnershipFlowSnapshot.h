#pragma once

#include "nativesharp/Frontend/OwnershipFlowStorageFacts.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace nativesharp::frontend
{
// Compact branch state for one callable. Public local ordinals stay global; localFlags uses the
// dense index `globalOrdinal - localBaseOrdinal`. Storage facts place parameter slots first and
// then local slots, eliminating hash containers from the copied ordinal state.
struct OwnershipFlowSnapshot final
{
    bool reachable = true;
    std::size_t localBaseOrdinal = 0;
    std::size_t localCount = 0;
    std::vector<std::uint8_t> localFlags;
    std::vector<std::uint8_t> parameterFlags;
    std::unordered_set<std::string> partiallyMovedPlaces;
    std::unordered_set<std::string> maybePartiallyMovedPlaces;
    std::unordered_set<std::size_t> assignedRequiredFields;
    bool hasRequiredConstructorExit = false;
    std::unordered_set<std::size_t> requiredConstructorExitAssignments;
    std::vector<OwnershipFlowStorageFacts> storageFacts;
};
}
