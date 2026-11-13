#pragma once

#include "nativesharp/Frontend/CppAliasEnumType.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Interop/CppScalarTypeMapping.h"

namespace nativesharp::interop
{
inline std::optional<compilation::TypeIdentity> projectCppEnumTypeIdentity(
    const CppType& type, std::string_view moduleName)
{
    const auto enumeration = frontend::cppAliasEnumType(type);
    if (!enumeration || enumeration->pointerDepth || moduleName.empty()) return std::nullopt;
    const auto last = enumeration->name.rfind('.');
    std::vector<std::string> namespaces;
    std::size_t begin = 0;
    while (last != std::string::npos && begin < last)
    {
        const auto end = enumeration->name.find('.', begin);
        if (end == std::string::npos || end > last || end == begin) return std::nullopt;
        namespaces.push_back(enumeration->name.substr(begin, end - begin));
        begin = end + 1;
    }
    const std::string name = last == std::string::npos ? enumeration->name : enumeration->name.substr(last + 1);
    compilation::TypeIdentity identity(std::string(moduleName), std::move(namespaces), {}, name);
    if (!identity.isValid()) return std::nullopt;
    return identity;
}

// Map only an enum whose imported declaration agrees with Clang's representation.
inline std::optional<CppScalarTypeMapping> projectCppSemanticEnumType(
    const CppType& type, std::size_t projectOrdinal, std::string_view moduleName,
    const frontend::ProjectNameLookup* lookup)
{
    if (!lookup || type.kind != CppTypeKind::Enumeration || type.isVolatile || moduleName.empty())
        return std::nullopt;
    CppType unqualified = type;
    unqualified.isConst = false;
    const auto enumeration = frontend::cppAliasEnumType(unqualified);
    const auto identity = projectCppEnumTypeIdentity(unqualified, moduleName);
    if (!enumeration || !identity) return std::nullopt;
    const auto ordinal = lookup->typeOrdinalForIdentity(identity->key());
    const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
    const bool reconciled = lookup->isReconciledCppTypeIdentity(identity->key());
    if (!imported || (!reconciled && imported->projectOrdinal != projectOrdinal) || !imported->enumBase ||
        (!identity->namespaceSegments().empty() && imported->origin != frontend::CppImportedTypeOrigin::CppEnum) ||
        (type.enumIdentity && type.enumIdentity->clangUsr && imported->clangUsr &&
         type.enumIdentity->clangUsr != imported->clangUsr) ||
        !imported->deferredImportError.empty() || imported->enumBase->token != enumeration->base.token ||
        !imported->objectLayout || !type.sizeBits || *type.sizeBits % 8 ||
        *type.sizeBits / 8 != imported->objectLayout->sizeBytes || !type.alignmentBits ||
        *type.alignmentBits % 8 || *type.alignmentBits / 8 != imported->objectLayout->alignmentBytes)
        return std::nullopt;
    return CppScalarTypeMapping{"type:" + imported->identity.key(), type.canonicalSpelling, type.isConst, false};
}
}
