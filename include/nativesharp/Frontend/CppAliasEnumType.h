#pragma once

#include "nativesharp/Frontend/CppAliasBuiltinType.h"

#include <optional>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
struct CppAliasEnumType
{
    std::string name;
    CppAliasBuiltinType base;
    std::size_t pointerDepth;
    bool pointeeConst;
};

inline std::optional<CppAliasEnumType> cppAliasEnumType(const interop::CppType& type)
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
    if (current->kind != interop::CppTypeKind::Enumeration || current->isVolatile ||
        (current->isConst && pointers != 1) || !current->enumIdentity ||
        current->enumIdentity->anonymousContext || current->enumIdentity->qualifiedName.empty() ||
        !current->enumUnderlying) return std::nullopt;
    auto base = cppAliasBuiltinType(*current->enumUnderlying);
    if (!base || base->pointerDepth != 0) return std::nullopt;
    std::string name = current->enumIdentity->qualifiedName;
    std::size_t pos = 0;
    while ((pos = name.find("::", pos)) != std::string::npos) name.replace(pos, 2, ".");
    return CppAliasEnumType{std::move(name), *base, pointers, current->isConst};
}
}
