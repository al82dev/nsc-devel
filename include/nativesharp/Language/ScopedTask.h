#pragma once

#include <string>

namespace nativesharp::language
{
struct ScopedTask final
{
    std::string name;
    bool borrowsFromScope = false;
    bool borrowOutlivesScope = false;
    bool joinedBeforeScopeExit = true;
};
}
