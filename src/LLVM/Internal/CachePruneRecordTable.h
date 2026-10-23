#pragma once

#include "CachePruneIndexRecord.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nativesharp::llvm_backend
{
// Live prune-index records by (kind, key) while the index lines are applied in order: a later
// publication replaces an earlier one and a removal drops it.
class CachePruneRecordTable final
{
public:
    explicit CachePruneRecordTable(std::span<const CachePruneIndexRecord> initial);

    // Records the publication of an entry, replacing a live record with the same kind and key.
    void publish(const CachePruneIndexRecord& record);

    // Drops the live record with this kind and key, if any.
    void remove(char kind, std::string_view key);

    // Moves out the live records, in the order they were first published.
    std::vector<CachePruneIndexRecord> take();

private:
    std::unordered_map<std::string, std::size_t> positions_;
    std::vector<CachePruneIndexRecord> records_;
    std::vector<bool> live_;
};
}
