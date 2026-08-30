#include "nativesharp/Sema/Type.h"

#include <sstream>
#include <utility>

namespace nativesharp::sema
{
Type::Type(TypeKind kind)
    : kind_(kind)
{
}

Type::Type(Type* unqualifiedType, OwnershipKind ownership)
    : kind_(unqualifiedType != nullptr ? unqualifiedType->kind() : TypeKind::Error),
      ownership_(ownership),
      unqualifiedType_(unqualifiedType != nullptr ? unqualifiedType->unqualifiedType() : nullptr)
{
}

TypeKind Type::kind() const noexcept
{
    return unqualifiedType_ != nullptr ? unqualifiedType_->kind() : kind_;
}
OwnershipKind Type::ownership() const noexcept { return ownership_; }
bool Type::isOwnershipQualified() const noexcept { return ownership_ != OwnershipKind::None; }
Type* Type::unqualifiedType() noexcept { return unqualifiedType_ != nullptr ? unqualifiedType_ : this; }
const Type* Type::unqualifiedType() const noexcept { return unqualifiedType_ != nullptr ? unqualifiedType_ : this; }
const std::string& Type::name() const noexcept { return unqualifiedType_ != nullptr ? unqualifiedType_->name() : name_; }
void Type::setName(std::string name)
{
    if (unqualifiedType_ != nullptr)
    {
        unqualifiedType_->setName(std::move(name));
        return;
    }
    name_ = std::move(name);
}
Type* Type::elementType() const noexcept { return unqualifiedType_ != nullptr ? unqualifiedType_->elementType() : elementType_; }
void Type::setElementType(Type* type) noexcept
{
    if (unqualifiedType_ != nullptr)
    {
        unqualifiedType_->setElementType(type);
        return;
    }
    elementType_ = type;
}
std::uint64_t Type::count() const noexcept { return unqualifiedType_ != nullptr ? unqualifiedType_->count() : count_; }
void Type::setCount(std::uint64_t count) noexcept
{
    if (unqualifiedType_ != nullptr)
    {
        unqualifiedType_->setCount(count);
        return;
    }
    count_ = count;
}
Type* Type::returnType() const noexcept { return unqualifiedType_ != nullptr ? unqualifiedType_->returnType() : returnType_; }
void Type::setReturnType(Type* type) noexcept
{
    if (unqualifiedType_ != nullptr)
    {
        unqualifiedType_->setReturnType(type);
        return;
    }
    returnType_ = type;
}
const std::vector<Type*>& Type::parameterTypes() const noexcept { return unqualifiedType_ != nullptr ? unqualifiedType_->parameterTypes() : parameterTypes_; }
void Type::setParameterTypes(std::vector<Type*> types)
{
    if (unqualifiedType_ != nullptr)
    {
        unqualifiedType_->setParameterTypes(std::move(types));
        return;
    }
    parameterTypes_ = std::move(types);
}
bool Type::isComplete() const noexcept { return unqualifiedType_ != nullptr ? unqualifiedType_->isComplete() : complete_; }
void Type::setComplete(bool complete) noexcept
{
    if (unqualifiedType_ != nullptr)
    {
        unqualifiedType_->setComplete(complete);
        return;
    }
    complete_ = complete;
}
const ast::AstNode* Type::declaration() const noexcept { return unqualifiedType_ != nullptr ? unqualifiedType_->declaration() : declaration_; }
void Type::setDeclaration(const ast::AstNode* declaration) noexcept
{
    if (unqualifiedType_ != nullptr)
    {
        unqualifiedType_->setDeclaration(declaration);
        return;
    }
    declaration_ = declaration;
}

std::string Type::displayName() const
{
    if (unqualifiedType_ != nullptr)
    {
        return std::string(ownershipKindName(ownership_)) + " " + unqualifiedType_->displayName();
    }

    if (kind_ == TypeKind::Pointer)
    {
        return elementType_->displayName() + "*";
    }

    if (kind_ == TypeKind::Array)
    {
        return elementType_->displayName() + "[" + std::to_string(count_) + "]";
    }

    if (kind_ == TypeKind::StringLiteral)
    {
        return "StringLiteralType<" + std::to_string(count_) + ">";
    }

    if (kind_ == TypeKind::Function)
    {
        std::ostringstream stream;
        stream << '(';
        for (std::size_t index = 0; index < parameterTypes_.size(); ++index)
        {
            if (index != 0)
            {
                stream << ", ";
            }
            stream << parameterTypes_[index]->displayName();
        }
        stream << ") -> " << returnType_->displayName();
        return stream.str();
    }

    if (!name_.empty())
    {
        return name_;
    }

    return std::string(typeKindName(kind_));
}
}
