#include "CachePruneRecordTable.h"

#include <utility>

namespace nativesharp::llvm_backend
{
CachePruneRecordTable::CachePruneRecordTable(std::span<const CachePruneIndexRecord> initial)
{
    for (const CachePruneIndexRecord& record : initial) publish(record);
}

void CachePruneRecordTable::publish(const CachePruneIndexRecord& record)
{
    const std::string identity = std::string(1, record.kind) + record.key;
    const auto found = positions_.find(identity);
    if (found != positions_.end())
    {
        records_[found->second] = record;
        return;
    }
    positions_.emplace(identity, records_.size());
    records_.push_back(record);
    live_.push_back(true);
}

void CachePruneRecordTable::remove(char kind, std::string_view key)
{
    const auto found = positions_.find(std::string(1, kind) + std::string(key));
    if (found == positions_.end()) return;
    live_[found->second] = false;
    positions_.erase(found);
}

std::vector<CachePruneIndexRecord> CachePruneRecordTable::take()
{
    std::vector<CachePruneIndexRecord> result;
    result.reserve(records_.size());
    for (std::size_t index = 0; index < records_.size(); ++index)
    {
        if (live_[index]) result.push_back(std::move(records_[index]));
    }
    records_.clear();
    live_.clear();
    positions_.clear();
    return result;
}
}
