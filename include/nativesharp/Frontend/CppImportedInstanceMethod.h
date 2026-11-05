#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
// Experimental declaration for isolated member indexing; this is not a call ABI contract.
struct CppImportedInstanceMethod
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    std::string name;
    std::string externalSymbol;
    std::string returnTypeKey;
    std::vector<std::string> parameterTypeKeys;
    bool receiverConst;
    bool dllImport = false;
    bool returnPointeeReadonly = false;
    std::vector<bool> parameterPointeeReadonly;
};

enum class CppImportedLifetimeKind { Constructor, Destructor };
struct CppImportedLifetimeMethod
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    CppImportedLifetimeKind kind;
    std::string externalSymbol;
    std::vector<std::string> parameterTypeKeys;
    bool dllImport = false;
    bool isNonThrowing = false;
    bool isImplicit = false;
    bool isTrivial = false;
    std::vector<bool> parameterPointeeReadonly;
};
}
