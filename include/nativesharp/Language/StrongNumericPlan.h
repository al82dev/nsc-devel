#pragma once

#include <cstddef>
#include <vector>

#include "nativesharp/Language/StrongNumericOperator.h"
#include "nativesharp/Language/StrongNumericType.h"

namespace nativesharp::language
{
struct StrongNumericPlan final
{
    StrongNumericType type;
    std::size_t size = 0;
    std::size_t alignment = 1;
    std::size_t constGenericArity = 0;
    bool abiTransparent = false;
    bool zeroRuntimeOverhead = true;
    bool constructionIsExplicit = true;
    bool extractionIsExplicit = true;
    bool derivesCopy = false;
    bool derivesMove = true;
    std::vector<StrongNumericOperator> operators;
};
}
