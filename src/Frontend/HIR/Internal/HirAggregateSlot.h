#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace nativesharp::frontend::internal
{
struct HirAggregateSlot final
{
    std::string typeKey;
    std::uint64_t offset = 0;
    std::optional<std::uint64_t> bitOffset;
    std::optional<std::uint64_t> bitWidth;
    std::uint64_t storageSize = 0;
    std::uint64_t storageAlignment = 1;
    bool volatileAccess = false;
};
}
