#pragma once

#include "CachePruneIndexRecord.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::llvm_backend
{
// What prune read from the artifact-cache prune index.
struct CachePruneIndexSnapshot final
{
    // False when the index is missing, unreadable, of another version, marked stale or due for a
    // reconciliation walk; prune then walks the cache folders as before and regenerates it.
    bool valid = false;
    // Header token of the file that was read; empty when it had no readable header.
    std::string generation;
    // Bytes of the file that were read: lines after this offset were appended later.
    std::uintmax_t consumedBytes = 0;
    // Lines appended since the last folder walk, carried over by rewrites.
    std::uintmax_t linesSinceWalk = 0;
    // Live entries, one per (kind, key), with their latest publication.
    std::vector<CachePruneIndexRecord> records;
};
}
