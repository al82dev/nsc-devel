#pragma once

#include <cstdint>
#include <string>

namespace nativesharp::llvm_backend
{
// Header fields of the artifact-cache prune index file (see CachePruneIndex).
struct CachePruneIndexHeader final
{
    // Token that changes on every rewrite of the file.
    std::string generation;
    // Number of publication lines the rewrite wrote after the header.
    std::uintmax_t records = 0;
    // Lines appended since the last folder walk, before the ones that follow the records.
    std::uintmax_t linesSinceWalk = 0;
};
}
