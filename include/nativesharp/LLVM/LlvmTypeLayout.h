#pragma once

#include "nativesharp/Frontend/BuiltinIntegerTypes.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <string_view>

// NSC305-L03. Struct/class layout + enum underlying-type lookups moved out of ProjectOopLlvmLowerer
// (pure move).
namespace nativesharp::llvm_backend
{
inline const frontend::ProjectOopMirStructLayout* findStructLayout(
    const frontend::ProjectOopMirModel& mirModel, std::string_view typeKey) noexcept
{
    for (const frontend::ProjectOopMirStructLayout& layout : mirModel.structLayouts())
    {
        if (layout.typeKey == typeKey) return &layout;
    }
    return nullptr;
}

inline const frontend::ProjectOopMirClassLayout* findClassLayout(
    const frontend::ProjectOopMirModel& mirModel, std::string_view typeKey) noexcept
{
    for (const frontend::ProjectOopMirClassLayout& layout : mirModel.classLayouts())
    {
        if (layout.typeKey == typeKey) return &layout;
    }
    return nullptr;
}

inline const frontend::builtins::IntegerTypeInfo* findEnumUnderlyingInfo(
    const frontend::ProjectNameLookup& nameLookup, std::string_view typeKey)
{
    if (!typeKey.starts_with("type:")) return nullptr;
    const std::string_view key = typeKey.substr(5);
    for (const frontend::NamedTypeDeclaration& type : nameLookup.namedTypes())
    {
        if (type.identity.key() != key ||
            type.kind != frontend::NamedTypeDeclarationKind::Enum)
            continue;
        return frontend::builtins::findIntegerType(frontend::enumUnderlyingTypeKey(type.syntax));
    }
    return nullptr;
}

inline bool isEnumTypeKey(
    const frontend::ProjectNameLookup& nameLookup, std::string_view typeKey)
{
    return findEnumUnderlyingInfo(nameLookup, typeKey) != nullptr;
}
} // namespace nativesharp::llvm_backend
