#include "nativesharp/Sema/TypeContext.h"

#include <string>

namespace nativesharp::sema
{
TypeContext::TypeContext()
{
    builtins_.resize(static_cast<std::size_t>(TypeKind::StringLiteral) + 1, nullptr);

    const TypeKind kinds[] = {
        TypeKind::Error,
        TypeKind::Void,
        TypeKind::Bool,
        TypeKind::Byte,
        TypeKind::Char,
        TypeKind::Short,
        TypeKind::UShort,
        TypeKind::Int,
        TypeKind::UInt,
        TypeKind::Long,
        TypeKind::ULong,
        TypeKind::NInt,
        TypeKind::NUInt,
        TypeKind::Float,
        TypeKind::Double,
        TypeKind::NullLiteral};

    for (TypeKind kind : kinds)
    {
        Type* type = create(kind);
        type->setName(std::string(typeKindName(kind)));
        builtins_[static_cast<std::size_t>(kind)] = type;
    }
}

TypeContext::~TypeContext()
{
    for (Type* type : ownedTypes_)
    {
        delete type;
    }
}

Type* TypeContext::builtin(TypeKind kind) const noexcept
{
    const std::size_t index = static_cast<std::size_t>(kind);
    if (index >= builtins_.size())
    {
        return nullptr;
    }
    return builtins_[index];
}

Type* TypeContext::qualified(Type* type, OwnershipKind ownership)
{
    if (type == nullptr)
    {
        return builtin(TypeKind::Error);
    }

    Type* base = type->unqualifiedType();
    if (ownership == OwnershipKind::None || base->kind() == TypeKind::Error)
    {
        return base;
    }

    for (Type* candidate : ownedTypes_)
    {
        if (candidate->isOwnershipQualified() &&
            candidate->ownership() == ownership &&
            candidate->unqualifiedType() == base)
        {
            return candidate;
        }
    }

    Type* result = new Type(base, ownership);
    ownedTypes_.push_back(result);
    return result;
}

Type* TypeContext::pointerTo(Type* pointee)
{
    for (Type* type : ownedTypes_)
    {
        if (!type->isOwnershipQualified() && type->kind() == TypeKind::Pointer && type->elementType() == pointee)
        {
            return type;
        }
    }

    Type* type = create(TypeKind::Pointer);
    type->setElementType(pointee);
    return type;
}

Type* TypeContext::arrayOf(Type* element, std::uint64_t count)
{
    for (Type* type : ownedTypes_)
    {
        if (!type->isOwnershipQualified() && type->kind() == TypeKind::Array && type->elementType() == element && type->count() == count)
        {
            return type;
        }
    }

    Type* type = create(TypeKind::Array);
    type->setElementType(element);
    type->setCount(count);
    return type;
}

Type* TypeContext::stringLiteral(std::uint64_t count)
{
    for (Type* type : ownedTypes_)
    {
        if (!type->isOwnershipQualified() && type->kind() == TypeKind::StringLiteral && type->count() == count)
        {
            return type;
        }
    }

    Type* type = create(TypeKind::StringLiteral);
    type->setElementType(builtin(TypeKind::Char));
    type->setCount(count);
    return type;
}

Type* TypeContext::functionType(Type* returnType, const std::vector<Type*>& parameters)
{
    for (Type* type : ownedTypes_)
    {
        if (!type->isOwnershipQualified() && type->kind() == TypeKind::Function && type->returnType() == returnType &&
            type->parameterTypes() == parameters)
        {
            return type;
        }
    }

    Type* type = create(TypeKind::Function);
    type->setReturnType(returnType);
    type->setParameterTypes(parameters);
    return type;
}

Type* TypeContext::createStruct(std::string_view name, const ast::AstNode* declaration)
{
    Type* type = create(TypeKind::Struct);
    type->setName(std::string(name));
    type->setComplete(false);
    type->setDeclaration(declaration);
    return type;
}

Type* TypeContext::createEnum(std::string_view name, const ast::AstNode* declaration)
{
    Type* type = create(TypeKind::Enum);
    type->setName(std::string(name));
    type->setComplete(true);
    type->setDeclaration(declaration);
    return type;
}

Type* TypeContext::create(TypeKind kind)
{
    Type* type = new Type(kind);
    ownedTypes_.push_back(type);
    return type;
}
}
