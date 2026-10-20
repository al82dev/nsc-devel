#include "nativesharp/LLVM/LlvmTypeLayout.h"

namespace nativesharp::llvm_backend
{
const frontend::ProjectOopMirStructLayout* findStructLayout(
    const frontend::ProjectOopMirModel& mirModel, std::string_view typeKey) noexcept
{
    for (const frontend::ProjectOopMirStructLayout& layout : mirModel.structLayouts())
    {
        if (layout.typeKey == typeKey) return &layout;
    }
    return nullptr;
}

const frontend::ProjectOopMirClassLayout* findClassLayout(
    const frontend::ProjectOopMirModel& mirModel, std::string_view typeKey) noexcept
{
    for (const frontend::ProjectOopMirClassLayout& layout : mirModel.classLayouts())
    {
        if (layout.typeKey == typeKey) return &layout;
    }
    return nullptr;
}

const frontend::builtins::IntegerTypeInfo* findEnumUnderlyingInfo(
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

bool isEnumTypeKey(
    const frontend::ProjectNameLookup& nameLookup, std::string_view typeKey)
{
    return findEnumUnderlyingInfo(nameLookup, typeKey) != nullptr;
}
} // namespace nativesharp::llvm_backend
