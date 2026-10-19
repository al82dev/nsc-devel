#pragma once

#include <cstdint>
#include <optional>

namespace nativesharp::target
{
struct StructFieldLayout
{
    std::uint64_t offset;
    std::uint64_t size;
    std::uint64_t alignment;
    // Item 328. Present only for a bit-field. `offset` names the beginning of the MSVC
    // allocation unit and `bitOffset` the least-significant bit within that unit.
    std::optional<std::uint64_t> bitOffset;
    std::optional<std::uint64_t> bitWidth;
};
}
