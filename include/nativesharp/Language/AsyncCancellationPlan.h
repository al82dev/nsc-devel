#pragma once

#include "nativesharp/Language/AsyncCleanupAction.h"
#include "nativesharp/Language/CancellationReason.h"

#include <cstddef>
#include <vector>

namespace nativesharp::language
{
struct AsyncCancellationPlan final
{
    CancellationReason reason = CancellationReason::Requested;
    std::size_t suspendedState = 0;
    std::vector<AsyncCleanupAction> actions;
    bool cooperative = true;
    bool usesThreadAbort = false;
};
}
