#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::llvm_backend
{
struct ProjectOopCodeUnit final
{
    std::size_t ordinal = 0;
    std::string stableKey;
    // NSC305-L01. Bounded historical cost (a capped moving average of previous builds' observed
    // costs), so a hot unit's weight saturates instead of growing unbounded.
    std::uint64_t historicalCost = 0;
    std::uint64_t estimatedCost = 0;
    std::vector<std::size_t> functionOrdinals;
    std::vector<std::size_t> declarationOrdinals;
};
}
