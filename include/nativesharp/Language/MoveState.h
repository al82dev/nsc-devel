#pragma once

#include <vector>

namespace nativesharp::language
{
struct MoveState final
{
    bool isCopy = false;
    bool hasCustomDestructor = false;
    std::vector<bool> initializedFields;
};
}
