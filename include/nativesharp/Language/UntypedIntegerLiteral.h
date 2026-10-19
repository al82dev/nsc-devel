#pragma once

#include <cstdint>

#include "nativesharp/Language/IntegerLiteralBase.h"
#include "nativesharp/Language/IntegerLiteralSuffix.h"

namespace nativesharp::language
{
struct UntypedIntegerLiteral final
{
    std::uint64_t magnitude = 0;
    IntegerLiteralBase base = IntegerLiteralBase::Decimal;
    IntegerLiteralSuffix suffix = IntegerLiteralSuffix::None;
};
}
