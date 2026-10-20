#pragma once

#include <cstddef>
#include <cstdint>

namespace nativesharp::llvm_backend
{
struct CachePruneTelemetry final
{
    bool performed = false;
    std::uint64_t elapsedMicros = 0;
    std::size_t inspectedEntries = 0;
    std::uintmax_t inspectedBytes = 0;
    std::size_t evictedEntries = 0;
    std::uintmax_t evictedBytes = 0;
};
}
