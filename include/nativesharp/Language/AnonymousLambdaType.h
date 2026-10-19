#pragma once

#include "nativesharp/Language/FunctionType.h"

#include <cstddef>

namespace nativesharp::language
{
// T08-F02. A non-capturing lambda which is not immediately target-converted is a nominal,
// zero-size value. Its identity keeps the source-order owner/lambda ordinals separate from the
// structural call signature: two textually distinct lambdas never become the same type even when
// their parameter and result types are identical. The structural signature is embedded so later
// semantic and backend stages can invoke or convert the value without consulting source syntax.
struct AnonymousLambdaType final
{
    std::size_t ownerOrdinal = 0;
    std::size_t lambdaOrdinal = 0;
    FunctionType signature;
};
}
