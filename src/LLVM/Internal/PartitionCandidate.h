#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nativesharp::llvm_backend
{
struct PartitionCandidate final
{
    std::size_t functionOrdinal = 0;
    std::uint64_t cost = 0;
    std::string_view stableName;
};
}
