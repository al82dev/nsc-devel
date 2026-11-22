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
// C3-01: the NativeSharp carrier of C `_Float16`. / El portador NativeSharp de `_Float16` de C.
inline constexpr char kCppHalfTypeKey[] = "type:NativeSharpCore::NativeSharp.Core.Numerics.Half";
}
