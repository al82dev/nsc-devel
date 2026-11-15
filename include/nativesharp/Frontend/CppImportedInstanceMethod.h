#pragma once

#include "nativesharp/Frontend/ClassMemberAccessibility.h"
#include "nativesharp/Frontend/ParameterPassKind.h"

#include <cstddef>
#include <cstdint>
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
    std::vector<std::uint8_t> objectSizeModes;
    bool sharedExternalSymbolVerified = false;
    // T11-005: `T&` is a ref and `const T&` an in parameter; a `T&` result is a ref return bound to this.
    std::vector<ParameterPassKind> parameterPassKinds;
    bool returnsReference = false;
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
    // T11-006: `T&` is a ref and `const T&` an in parameter; T11-007: the declared access.
    std::vector<ParameterPassKind> parameterPassKinds = {};
    ClassMemberAccessibility accessibility = ClassMemberAccessibility::Public;
};
}
