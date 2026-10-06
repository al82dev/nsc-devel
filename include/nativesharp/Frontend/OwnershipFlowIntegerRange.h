#pragma once

#include <cstdint>
#include <optional>

namespace nativesharp::frontend
{
struct OwnershipFlowIntegerRange final
{
    std::optional<std::int64_t> minimum;
    std::optional<std::int64_t> maximum;
    bool excludesZero = false;
};
}
