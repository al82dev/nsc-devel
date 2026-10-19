#pragma once

#include <cstdint>

#include "nativesharp/Language/IntegerArithmeticOperation.h"
#include "nativesharp/Language/IntegerType.h"
#include "nativesharp/Language/NumericEvaluationDomain.h"
#include "nativesharp/Language/OverflowPolicy.h"

namespace nativesharp::language
{
struct IntegerArithmeticRequest final
{
    IntegerArithmeticOperation operation = IntegerArithmeticOperation::Add;
    std::uint64_t leftBits = 0;
    std::uint64_t rightBits = 0;
    IntegerType type;
    OverflowPolicy policy = OverflowPolicy::Checked;
    NumericEvaluationDomain domain = NumericEvaluationDomain::Runtime;
};
}
