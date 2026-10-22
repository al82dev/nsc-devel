#include "nativesharp/LLVM/LlvmTypeLayout.h"

namespace nativesharp::llvm_backend
{
const frontend::ProjectOopMirStructLayout* findStructLayout(
    const frontend::ProjectOopMirModel& mirModel, std::string_view typeKey) noexcept
{
    return mirModel.structLayoutForTypeKey(typeKey);
}

const frontend::ProjectOopMirClassLayout* findClassLayout(
    const frontend::ProjectOopMirModel& mirModel, std::string_view typeKey) noexcept
{
    return mirModel.classLayoutForTypeKey(typeKey);
}

const frontend::builtins::IntegerTypeInfo* findEnumUnderlyingInfo(
    const frontend::ProjectNameLookup& nameLookup, std::string_view typeKey)
{
    if (!typeKey.starts_with("type:")) return nullptr;
    const std::optional<std::size_t> ordinal =
        nameLookup.typeOrdinalForIdentity(typeKey.substr(5));
    const frontend::NamedTypeDeclaration* type = ordinal.has_value()
        ? nameLookup.typeForOrdinal(*ordinal) : nullptr;
    if (type == nullptr || type->kind != frontend::NamedTypeDeclarationKind::Enum) return nullptr;
    return frontend::builtins::findIntegerType(frontend::enumUnderlyingTypeKey(type->syntax));
}

bool isEnumTypeKey(
    const frontend::ProjectNameLookup& nameLookup, std::string_view typeKey)
{
    return findEnumUnderlyingInfo(nameLookup, typeKey) != nullptr;
}
} // namespace nativesharp::llvm_backend
