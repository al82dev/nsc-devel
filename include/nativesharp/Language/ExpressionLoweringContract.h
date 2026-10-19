#pragma once

#include <string_view>

#include "nativesharp/Language/ExpressionFeature.h"

namespace nativesharp::language
{
struct ExpressionLoweringContract final
{
    ExpressionFeature feature = ExpressionFeature::NamedArguments;
    std::string_view spelling;
    bool sourceOrder = true;
    bool primaryEvaluatedOnce = true;
    bool mayAllocateImplicitly = false;
    bool preservesAbi = true;
};
}
