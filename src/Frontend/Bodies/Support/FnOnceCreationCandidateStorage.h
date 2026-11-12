#pragma once
#include "nativesharp/Frontend/FnOnceCreationCandidate.h"
#include <cstddef>
#include <vector>
namespace nativesharp::frontend::detail
{
inline void appendFnOnceCreationCandidate(std::vector<FnOnceCreationCandidate>& candidates,
    bool& limitReached, FnOnceCreationCandidate candidate, std::size_t limit)
{
    if (candidates.size() >= limit) { limitReached = true; return; }
    candidates.push_back(candidate);
}
inline void mergeFnOnceCandidateLimit(bool& destination, bool source) noexcept
{
    destination = destination || source;
}
}
