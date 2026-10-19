#pragma once

#include <cstddef>

namespace nativesharp::compilation
{
struct ProjectLoaderStatistics final
{
    std::size_t descriptorFileReads = 0;
    std::size_t descriptorCacheHits = 0;
};
}
