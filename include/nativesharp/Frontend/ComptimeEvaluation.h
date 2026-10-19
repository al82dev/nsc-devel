#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

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
}
