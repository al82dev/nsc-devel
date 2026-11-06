#pragma once

#include "nativesharp/Interop/CppSessionDiagnostic.h"
#include "nativesharp/Interop/CppType.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::interop
{
struct CppEnumeratorDeclaration
{
    std::string name;
    // Exact decimal spelling preserves values outside the int64_t range.
    std::string decimalValue;
    std::uint32_t valueBits = 0;
    bool valueIsUnsigned = false;
    std::optional<CppSourcePoint> location;
    // Type of the enumerator expression, which need not be the enum's storage type.
    CppType valueType;
    std::optional<std::string> clangUsr;
};

struct CppEnumDeclaration
{
    // Anonymous enums have empty names but may still contain usable constants.
    std::string name;
    std::string qualifiedName;
    bool complete = false;
    bool scoped = false;
    std::optional<CppType> underlyingType;
    std::optional<CppSourcePoint> location;
    std::vector<CppEnumeratorDeclaration> enumerators;
    bool isCxx = true;
    std::optional<std::uint64_t> sizeBits;
    std::optional<std::uint64_t> alignmentBits;
    std::optional<std::string> clangUsr;
};
}
