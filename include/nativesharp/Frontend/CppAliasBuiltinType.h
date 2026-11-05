#pragma once

#include "nativesharp/Interop/CppType.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <cstddef>
#include <optional>
#include <string_view>

namespace nativesharp::frontend
{
struct CppAliasBuiltinType
{
    lexer::TokenKind token;
    std::string_view name;
    std::size_t pointerDepth;
    bool pointeeConst;
};

// Maps only qualifiers that TypeSyntax can preserve without changing meaning.
// The caller must retain and diagnose unrepresentable types rather than erase qualifiers.
inline std::optional<CppAliasBuiltinType> cppAliasBuiltinType(const interop::CppType& type)
{
    const interop::CppType* current = &type;
    std::size_t pointers = 0;
    while (current->kind == interop::CppTypeKind::Pointer)
    {
        if (current->isConst || current->isVolatile || current->sizeBits != 64 || !current->element)
            return std::nullopt;
        current = current->element.get();
        ++pointers;
    }
    if (current->isVolatile || (current->isConst && pointers != 1)) return std::nullopt;
    using K = interop::CppTypeKind;
    using T = lexer::TokenKind;
    T token = T::Invalid;
    std::string_view name;
    const auto set = [&](T value, std::string_view spelling) { token = value; name = spelling; };
    if (current->kind == K::Void) set(T::KwVoid, "void");
    else if (current->kind == K::Boolean && current->sizeBits == 8) set(T::KwBool, "bool");
    else if (current->kind == K::SignedInteger)
    {
        if (current->sizeBits == 8) set(T::KwSByte, "sbyte");
        else if (current->sizeBits == 16) set(T::KwShort, "short");
        else if (current->sizeBits == 32) set(T::KwInt, "int");
        else if (current->sizeBits == 64) set(T::KwLong, "long");
    }
    else if (current->kind == K::UnsignedInteger)
    {
        if (current->sizeBits == 8) set(T::KwByte, "byte");
        else if (current->sizeBits == 16) set(T::KwUShort, "ushort");
        else if (current->sizeBits == 32) set(T::KwUInt, "uint");
        else if (current->sizeBits == 64) set(T::KwULong, "ulong");
    }
    else if (current->kind == K::Floating)
    {
        if (current->unqualifiedCanonicalSpelling == "float" && current->sizeBits == 32)
            set(T::KwFloat, "float");
        else if (current->unqualifiedCanonicalSpelling == "double" && current->sizeBits == 64)
            set(T::KwDouble, "double");
    }
    if (token == T::Invalid) return std::nullopt;
    return CppAliasBuiltinType{token, name, pointers, current->isConst};
}
}
