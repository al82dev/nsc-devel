#pragma once

#include "nativesharp/Frontend/MacroExpansionContext.h"

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::frontend::internal
{
struct Definition final
{
    std::vector<std::string> parameters;
    std::string body;
    MacroExpansionContext context = MacroExpansionContext::Statement;
    std::size_t definitionOffset = 0;
    std::size_t definitionLength = 0;
};
}
