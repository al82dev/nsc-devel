#include "nativesharp/LLVM/LlvmTypeClassification.h"

namespace nativesharp::llvm_backend
{
bool isPointerLikeType(std::string_view typeKey) noexcept
{
    return (!typeKey.empty() && typeKey.back() == '*') ||
        frontend::ProjectTypeConversion::isFunctionPointer(typeKey) ||
        typeKey == "string-literal" ||
        frontend::ProjectTypeConversion::isNativeCoreReference(typeKey);
}

bool isDelegateReferenceType(
    const frontend::ProjectNameLookup& nameLookup, std::string_view typeKey) noexcept
{
    if (const std::optional<language::FunctionType> functionType =
            language::parseFunctionTypeIdentity(typeKey))
        return functionType->carrier == language::FunctionCarrier::Reference;
    if (!typeKey.starts_with("type:")) return false;
    const std::optional<std::size_t> ordinal =
        nameLookup.typeOrdinalForIdentity(typeKey.substr(5));
    const frontend::NamedTypeDeclaration* type = ordinal.has_value()
        ? nameLookup.typeForOrdinal(*ordinal) : nullptr;
    return type != nullptr && type->kind == frontend::NamedTypeDeclarationKind::Delegate;
}

bool isClassReferenceType(
    const frontend::ProjectNameLookup& nameLookup, std::string_view typeKey) noexcept
{
    if (!typeKey.starts_with("type:")) return false;
    const std::optional<std::size_t> ordinal =
        nameLookup.typeOrdinalForIdentity(typeKey.substr(5));
    const frontend::NamedTypeDeclaration* type = ordinal.has_value()
        ? nameLookup.typeForOrdinal(*ordinal) : nullptr;
    return type != nullptr &&
        (type->kind == frontend::NamedTypeDeclarationKind::Class ||
         type->kind == frontend::NamedTypeDeclarationKind::Interface);
}
} // namespace nativesharp::llvm_backend
