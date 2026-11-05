#pragma once

#include "nativesharp/Interop/CppType.h"

#include <optional>
#include <string>

namespace nativesharp::interop
{
// A value-representation mapping only. It does not classify argument/return ABI or determine
// C++ overload equivalence; canonicalCppType must remain available for Clang's semantic rules.
struct CppScalarTypeMapping
{
    std::string nativeSharpTypeKey;
    std::string canonicalCppType;
    bool isConst = false;
    bool isVolatile = false;
};

std::optional<CppScalarTypeMapping> mapCppScalarType(const CppType& type);
}
