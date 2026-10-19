#pragma once

#include <string>
#include <vector>

#include "nativesharp/Language/TraitMethod.h"

namespace nativesharp::language
{
struct TraitContract final
{
    std::string qualifiedName;
    std::vector<TraitMethod> methods;
    bool declaresInstanceStorage = false;
};
}
