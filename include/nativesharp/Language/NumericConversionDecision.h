#pragma once

#include "nativesharp/Language/NumericConversionKind.h"

namespace nativesharp::language
{
struct NumericConversionDecision final
{
    NumericConversionKind kind = NumericConversionKind::Invalid;
    unsigned overloadRank = 255;
    bool implicit = false;
    bool lossless = false;
    bool requiresOverflowPolicy = false;
};
}
