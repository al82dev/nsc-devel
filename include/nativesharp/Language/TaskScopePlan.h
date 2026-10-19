#pragma once

#include "nativesharp/Language/ScopedTask.h"

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::language
{
struct TaskScopePlan final
{
    std::string executorType;
    std::vector<ScopedTask> children;
    std::size_t nestingDepth = 0;
    bool noHeap = false;
    bool fixesThreadPool = false;
};
}
