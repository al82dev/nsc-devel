#pragma once

#include <string>
#include <vector>

#include "nativesharp/Language/ListLengthDomain.h"

namespace nativesharp::language
{
struct MatchArmCoverage final
{
    using ListLengthDomain = nativesharp::language::ListLengthDomain;

    std::vector<std::string> cases;
    bool wildcard = false;
    bool hasGuard = false;
    // One source arm may join several list alternatives with OR. Each exact domain covers one
    // length; a rest domain covers every length greater than or equal to its fixed prefix.
    std::vector<ListLengthDomain> listLengths;
};
}
