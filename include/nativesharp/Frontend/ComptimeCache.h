#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace nativesharp::frontend
{
class ComptimeCache final
{
public:
    struct Entry final
    {
        std::int64_t value = 0;
        std::size_t executedSteps = 0;
    };

    std::optional<Entry> find(std::string_view key) const;
    void store(std::string key, Entry entry);
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};
}
