#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend
{
struct ComptimeEvaluation final
{
    std::size_t offset = 0;
    std::size_t length = 0;
    std::int64_t value = 0;
    std::string dependencyDigest;
    bool cacheHit = false;
};

struct ComptimeDependency final
{
    std::string kind;
    std::string identity;
    std::string digest;
};

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

struct ComptimeLimits final
{
    std::size_t maximumSourceBytes = 64U * 1024U * 1024U;
    std::size_t maximumDirectives = 1024;
    std::size_t maximumSteps = 100'000;
    std::size_t maximumNesting = 128;
};

struct ComptimeResult final
{
    bool success = false;
    std::string source;
    std::string error;
    std::vector<ComptimeEvaluation> evaluations;
    std::vector<ComptimeDependency> dependencies;
    std::size_t executedSteps = 0;
    std::size_t cacheHits = 0;
    std::size_t cacheMisses = 0;
    std::string reproducibilityDigest;
};

// Executes the deliberately small NativeSharp 2.0 comptime core. `comptime` is a contextual
// marker and `#run(integer-expression)` is replaced with its signed 64-bit result before lexing.
// Replacements retain the original byte count, so every later source range still names the
// original file offset. The engine never invokes native code or the host environment.
class ComptimeEngine final
{
public:
    explicit ComptimeEngine(
        ComptimeLimits limits = {},
        std::shared_ptr<ComptimeCache> cache = {}) noexcept;
    ComptimeResult execute(std::string_view source) const;

private:
    ComptimeLimits limits_;
    std::shared_ptr<ComptimeCache> cache_;
};
}
