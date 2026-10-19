#pragma once

#include "nativesharp/Language/FrameStorage.h"
#include "nativesharp/Language/FutureStart.h"
#include "nativesharp/Language/IterationSemantics.h"

#include <string>

namespace nativesharp::language
{
struct AsyncFunctionPlan final
{
    std::string returnType;
    std::string opaqueFutureIdentity;
    FutureStart start = FutureStart::Lazy;
    FrameStorage storage = FrameStorage::CallerStack;
    SynchronousGeneratorPlan stateMachine;
    bool completesSynchronously = false;
    bool hasImplicitHeapAllocation = false;
};
}
