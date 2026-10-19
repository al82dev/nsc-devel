#pragma once

#include <string>
#include <vector>

namespace nativesharp::language
{
struct BorrowPlace final
{
    std::string root;
    std::vector<std::string> projections;
};
}
