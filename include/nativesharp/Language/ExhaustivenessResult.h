#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::language
{
struct ExhaustivenessResult final
{
    bool exhaustive = false;
    std::vector<std::string> missingCases;
    std::vector<std::size_t> unreachableArms;
};
}
