#pragma once

#include <cstdint>

namespace nativesharp::frontend
{
enum class StageInformation : std::uint32_t
{
    None = 0,
    Syntax = 1U << 0U,
    Symbols = 1U << 1U,
    Types = 1U << 2U,
    Constants = 1U << 3U,
    ControlFlow = 1U << 4U,
    Specializations = 1U << 5U,
    MachineCode = 1U << 6U
};

constexpr StageInformation operator|(StageInformation left, StageInformation right) noexcept
{
    return static_cast<StageInformation>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr bool containsStageInformation(
    StageInformation available,
    StageInformation requested) noexcept
{
    return (static_cast<std::uint32_t>(available) & static_cast<std::uint32_t>(requested)) ==
        static_cast<std::uint32_t>(requested);
}
}
