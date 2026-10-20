#pragma once

#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

namespace nativesharp::frontend
{
std::optional<std::size_t> snapshotLocalIndex(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t globalLocalOrdinal) noexcept;
std::optional<std::size_t> snapshotStorageIndex(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t storageKey) noexcept;
OwnershipFlowStorageFacts* mutableSnapshotStorageFacts(
    OwnershipFlowSnapshot& snapshot,
    std::size_t storageKey) noexcept;
bool snapshotLocalHasAnyFlag(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t localOrdinal,
    std::uint8_t flags) noexcept;
bool snapshotParameterHasAnyFlag(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t parameterOrdinal,
    std::uint8_t flags) noexcept;
}
