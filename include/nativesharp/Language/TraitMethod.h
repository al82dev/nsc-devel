#pragma once

#include <string>

namespace nativesharp::language
{
struct TraitMethod final
{
    std::string signature;
    bool hasDefault = false;
    bool isGeneric = false;
    bool returnsSelfByValue = false;
};
}
