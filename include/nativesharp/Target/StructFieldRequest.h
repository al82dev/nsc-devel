#pragma once

#include "nativesharp/Target/TypeLayout.h"

#include <cstdint>
#include <optional>

namespace nativesharp::target
{
struct StructFieldRequest
{
    TypeLayout layout;
    std::optional<std::uint64_t> explicitOffset;
    std::optional<std::uint64_t> minimumAlignment;
    bool isFlexibleArray = false;
    std::optional<std::uint64_t> bitWidth;
    bool isUnnamedBitField = false;
};
}
