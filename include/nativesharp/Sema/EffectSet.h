#pragma once

#include "nativesharp/Sema/Effect.h"

#include <cstdint>

namespace nativesharp::sema
{
class EffectSet final
{
public:
    constexpr EffectSet() noexcept = default;
    constexpr explicit EffectSet(Effect effect) noexcept
        : bits_(static_cast<std::uint32_t>(effect)) {}

    constexpr EffectSet with(Effect effect) const noexcept
    {
        EffectSet result = *this;
        result.bits_ |= static_cast<std::uint32_t>(effect);
        return result;
    }

    constexpr bool contains(Effect effect) const noexcept
    {
        return (bits_ & static_cast<std::uint32_t>(effect)) != 0;
    }

    constexpr std::uint32_t bits() const noexcept { return bits_; }

private:
    std::uint32_t bits_ = 0;
};
}
