#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace nativesharp::llvm_backend
{
// One live entry of the artifact-cache prune index: what prune needs to order and evict an entry
// without walking the cache folders.
struct CachePruneIndexRecord final
{
    // 'o' for an object (objects/<xx>/<key>.obj), 'p' for a code-partition plan
    // (partitions/<xx>/<key>.plan).
    char kind = 'o';
    std::string key;
    // Last write time of the entry's .meta file: the publication time the folder walk reads.
    std::filesystem::file_time_type publication;
    // Size of the payload file in bytes.
    std::uintmax_t size = 0;
};
}
