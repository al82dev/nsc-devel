#include "nativesharp/Interop/CppScalarTypeMapping.h"

#include <string_view>

namespace nativesharp::interop
{
std::optional<CppScalarTypeMapping> mapCppScalarType(const CppType& type)
{
    if (type.canonicalSpelling.empty()) return std::nullopt;
    std::string_view key;
    if (type.kind == CppTypeKind::Void) key = "builtin:void";
    else if (type.kind == CppTypeKind::Boolean && type.sizeBits == 8) key = "builtin:bool";
    else if (type.kind == CppTypeKind::SignedInteger)
    {
        if (type.sizeBits == 8) key = "builtin:sbyte";
        else if (type.sizeBits == 16) key = "builtin:short";
        else if (type.sizeBits == 32) key = "builtin:int";
        else if (type.sizeBits == 64) key = "builtin:long";
    }
    else if (type.kind == CppTypeKind::UnsignedInteger)
    {
        if (type.sizeBits == 8) key = "builtin:byte";
        else if (type.sizeBits == 16) key = "builtin:ushort";
        else if (type.sizeBits == 32) key = "builtin:uint";
        else if (type.sizeBits == 64) key = "builtin:ulong";
    }
    else if (type.kind == CppTypeKind::Floating)
    {
        if (type.unqualifiedCanonicalSpelling == "float" && type.sizeBits == 32) key = "builtin:float";
        else if (type.unqualifiedCanonicalSpelling == "double" && type.sizeBits == 64) key = "builtin:double";
    }
    if (key.empty()) return std::nullopt;
    return CppScalarTypeMapping{std::string(key), type.canonicalSpelling, type.isConst, type.isVolatile};
}
}
