#pragma once

#include <string>
#include <vector>

namespace nativesharp::language
{
struct ConstraintSet final
{
    std::vector<std::string> atoms;
    std::string canonical;
};
}
