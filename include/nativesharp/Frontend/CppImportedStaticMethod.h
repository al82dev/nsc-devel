#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
// Experimental static C++ method declaration; no invocation support is implied.
struct CppImportedStaticMethod final
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    std::string name;
    std::string externalSymbol;
    std::string returnTypeKey;
    std::vector<std::string> parameterTypeKeys;
    bool dllImport = false;
    bool returnPointeeReadonly = false;
    std::vector<bool> parameterPointeeReadonly;
};
}
