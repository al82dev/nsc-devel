#pragma once

#include <cstddef>

namespace nativesharp::frontend
{
// Structural counters for directed performance assurance. They intentionally exclude wall-clock
// time so they cannot perturb deterministic compilation artifacts.
struct ProjectOwnershipFlowStatistics final
{
    std::size_t snapshotsCaptured = 0;
    std::size_t snapshotsRestored = 0;
    std::size_t joinsComputed = 0;
    std::size_t denseLocalSlotsCaptured = 0;
    std::size_t denseStorageSlotsCaptured = 0;
    std::size_t partialPlaceEntriesCopied = 0;
    std::size_t legacyOrdinalHashEntriesCopied = 0;
};
}
