#pragma once

#include "HirExpressionStatementMachineCommon.h"

namespace nativesharp::frontend::internal
{
struct AggregateCrossSlot final
{
    std::string typeKey;
    std::uint64_t offset = 0U;
    std::optional<std::uint64_t> bitOffset;
    std::optional<std::uint64_t> bitWidth;
    std::uint64_t storageSize = 0U;
    std::uint64_t storageAlignment = 1U;
    bool volatileAccess = false;
};
}
