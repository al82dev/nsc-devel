#pragma once

#include "nativesharp/Frontend/MacroExpansionContext.h"

#include <cstddef>
#include <string>

namespace nativesharp::frontend
{
struct MacroExpansionRecord final
{
    std::string macroName;
    std::size_t invocationOffset = 0;
    std::size_t invocationLength = 0;
    std::size_t expansionLength = 0;
    std::size_t hygieneScope = 0;
    MacroExpansionContext context = MacroExpansionContext::Statement;
    // NS304-T14-F04: source mapping of a generated expansion — the template (definition) location
    // that produced it, so a diagnostic can navigate to both the invocation and the template.
    std::size_t definitionOffset = 0;
    std::size_t definitionLength = 0;
};
}
