#pragma once

#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Language/CallableSemantics.h"

#include <optional>
#include <string_view>

// NSC305-L03. Type classification moved out of ProjectOopLlvmLowerer (pure move).
namespace nativesharp::llvm_backend
{
inline bool isPointerLikeType(std::string_view typeKey) noexcept
{
    return (!typeKey.empty() && typeKey.back() == '*') ||
        frontend::ProjectTypeConversion::isFunctionPointer(typeKey) ||
        typeKey == "string-literal" ||
        frontend::ProjectTypeConversion::isNativeCoreReference(typeKey);
}

inline bool isDelegateReferenceType(
    const frontend::ProjectNameLookup& nameLookup, std::string_view typeKey) noexcept
{
    if (const std::optional<language::FunctionType> functionType =
            language::parseFunctionTypeIdentity(typeKey))
        return functionType->carrier == language::FunctionCarrier::Reference;
    if (!typeKey.starts_with("type:")) return false;
    const std::string_view key = typeKey.substr(5);
    for (const frontend::NamedTypeDeclaration& type : nameLookup.namedTypes())
    {
        if (type.identity.key() == key)
            return type.kind == frontend::NamedTypeDeclarationKind::Delegate;
    }
    return false;
}

inline bool isClassReferenceType(
    const frontend::ProjectNameLookup& nameLookup, std::string_view typeKey) noexcept
{
    if (!typeKey.starts_with("type:")) return false;
    const std::string_view key = typeKey.substr(5);
    for (const frontend::NamedTypeDeclaration& type : nameLookup.namedTypes())
    {
        if (type.identity.key() == key)
            return type.kind == frontend::NamedTypeDeclarationKind::Class ||
                type.kind == frontend::NamedTypeDeclarationKind::Interface;
    }
    return false;
}
} // namespace nativesharp::llvm_backend
