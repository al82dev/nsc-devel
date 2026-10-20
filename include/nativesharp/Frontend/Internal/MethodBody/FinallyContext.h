#pragma once

struct FinallyContext final
{
    std::size_t cleanupRegionOrdinal = 0;
    std::size_t loopDepthAtEntry = 0;
};
