#pragma once

#include "nativesharp/Language/FusionRejection.h"
#include "nativesharp/Language/TraversalCategory.h"

namespace nativesharp::language
{
struct FusionDecision final
{
    bool allowed = false;
    FusionRejection rejection = FusionRejection::None;
    TraversalCategory resultCategory = TraversalCategory::SinglePass;
};
}
