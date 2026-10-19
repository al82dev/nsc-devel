#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "nativesharp/Language/GeneratorFrame.h"
#include "nativesharp/Language/GeneratorFrameSlot.h"
#include "nativesharp/Language/GeneratorState.h"
#include "nativesharp/Language/GeneratorStorage.h"

namespace nativesharp::language
{
struct SynchronousGeneratorPlan final
{
    std::string itemType;
    GeneratorStorage storage = GeneratorStorage::Stack;
    GeneratorFrame frame{1, 1, false};
    std::vector<GeneratorFrameSlot> slots;
    std::vector<GeneratorState> states;
    std::size_t completedState = 1;
    bool yieldsBorrowedItems = false;
    bool hasImplicitHeapAllocation = false;
};
}
