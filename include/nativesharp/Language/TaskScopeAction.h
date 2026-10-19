#pragma once

#include "nativesharp/Language/TaskScopeActionKind.h"

#include <string>

namespace nativesharp::language
{
struct TaskScopeAction final
{
    TaskScopeActionKind kind = TaskScopeActionKind::Join;
    std::string child;
};
}
