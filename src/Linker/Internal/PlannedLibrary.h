#pragma once

#include <string>
#include <vector>

namespace nativesharp::linker
{
struct PlannedLibrary final
{
    std::string canonicalDll;
    std::vector<std::string> symbols;
};
}
