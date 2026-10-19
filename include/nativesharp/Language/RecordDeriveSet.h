#pragma once

#include "nativesharp/Language/RecordDerive.h"

#include <cstdint>

namespace nativesharp::language
{
class RecordDeriveSet final
{
public:
    constexpr RecordDeriveSet() noexcept = default;
    constexpr explicit RecordDeriveSet(RecordDerive derive) noexcept
        : bits_(static_cast<std::uint8_t>(derive)) {}
    constexpr bool contains(RecordDerive derive) const noexcept
    {
        return (bits_ & static_cast<std::uint8_t>(derive)) != 0;
    }
    constexpr RecordDeriveSet with(RecordDerive derive) const noexcept
    {
        RecordDeriveSet result;
        result.bits_ = bits_ | static_cast<std::uint8_t>(derive);
        return result;
    }
private:
    std::uint8_t bits_ = 0;
};
}
