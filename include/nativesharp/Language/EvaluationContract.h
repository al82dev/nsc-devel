#pragma once

#include "nativesharp/Language/TraversalCategory.h"

namespace nativesharp::language
{
struct EvaluationContract final
{
    TraversalCategory category = TraversalCategory::SinglePass;
    bool lazy = true;
    bool preservesLeftToRightEffects = true;
    bool mayShortCircuit = false;
    bool repeatsElements = false;
    bool buffersElements = false;
    bool yieldsBorrowedItems = false;
};
}
