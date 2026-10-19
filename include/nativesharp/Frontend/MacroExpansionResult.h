#pragma once

#include "nativesharp/Frontend/MacroExpansionRecord.h"

#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct MacroExpansionResult final
{
    bool success = false;
    std::string source;
    std::string error;
    std::vector<MacroExpansionRecord> expansions;
};
}
