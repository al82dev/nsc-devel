#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend::internal
{
struct FunctionLookupEnvironment final
{
    std::vector<std::string> importedNamespaces;
    std::unordered_map<std::string, std::string> aliases;
};
}
