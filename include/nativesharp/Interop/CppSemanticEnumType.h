#pragma once

#include "nativesharp/Frontend/CppAliasEnumType.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Interop/CppScalarTypeMapping.h"

namespace nativesharp::interop
{
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
    if (!enumeration || enumeration->pointerDepth || enumeration->name.find('.') != std::string::npos)
        return std::nullopt;
    const compilation::TypeIdentity identity(std::string(moduleName), {}, {}, enumeration->name);
    const auto ordinal = lookup->typeOrdinalForIdentity(identity.key());
    const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
    if (!imported || imported->projectOrdinal != projectOrdinal || !imported->enumBase ||
        !imported->deferredImportError.empty() || imported->enumBase->token != enumeration->base.token ||
        !imported->objectLayout || !type.sizeBits || *type.sizeBits % 8 ||
        *type.sizeBits / 8 != imported->objectLayout->sizeBytes || !type.alignmentBits ||
        *type.alignmentBits % 8 || *type.alignmentBits / 8 != imported->objectLayout->alignmentBytes)
        return std::nullopt;
    return CppScalarTypeMapping{"type:" + identity.key(), type.canonicalSpelling, type.isConst, false};
}
}
