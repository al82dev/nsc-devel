#pragma once

#include "nativesharp/Language/FrameStorage.h"
#include "nativesharp/Language/IterationSemantics.h"

#include <string>

namespace nativesharp::language
{
struct AsyncGeneratorPlan final
{
    std::string itemType;
    std::string errorType;
    std::string cancellationType;
    FrameStorage storage = FrameStorage::CallerStack;
    SynchronousGeneratorPlan stateMachine;
    bool explicitBackpressure = true;
    bool moveOnlyItems = false;
};
}
