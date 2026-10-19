#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "nativesharp/Language/NumericKind.h"
#include "nativesharp/Language/NumericType.h"
#include "nativesharp/Language/StrongNumericOperator.h"

namespace nativesharp::language
{
struct StrongNumericDefinition final
{
    std::string identity;
    NumericType representation{NumericKind::Integer, 32, true};
    std::size_t fieldCount = 1;
    bool reprTransparent = false;
    bool ffiExported = false;
    bool derivesCopy = false;
    bool derivesMove = true;
    std::size_t constGenericArity = 0;
    std::vector<StrongNumericOperator> operators;
};
}
