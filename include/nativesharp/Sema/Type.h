#pragma once

#include "nativesharp/Sema/OwnershipKind.h"
#include "nativesharp/Sema/TypeKind.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::sema
{
class Type final
{
public:
    explicit Type(TypeKind kind);
    Type(Type* unqualifiedType, OwnershipKind ownership);

    TypeKind kind() const noexcept;
    OwnershipKind ownership() const noexcept;
    bool isOwnershipQualified() const noexcept;
    Type* unqualifiedType() noexcept;
    const Type* unqualifiedType() const noexcept;
    const std::string& name() const noexcept;
    void setName(std::string name);

    Type* elementType() const noexcept;
    void setElementType(Type* type) noexcept;

    std::uint64_t count() const noexcept;
    void setCount(std::uint64_t count) noexcept;

    Type* returnType() const noexcept;
    void setReturnType(Type* type) noexcept;

    const std::vector<Type*>& parameterTypes() const noexcept;
    void setParameterTypes(std::vector<Type*> types);

    bool isComplete() const noexcept;
    void setComplete(bool complete) noexcept;

    const ast::AstNode* declaration() const noexcept;
    void setDeclaration(const ast::AstNode* declaration) noexcept;

    std::string displayName() const;

private:
    TypeKind kind_;
    OwnershipKind ownership_ = OwnershipKind::None;
    Type* unqualifiedType_ = nullptr;
    std::string name_;
    Type* elementType_ = nullptr;
    std::uint64_t count_ = 0;
    Type* returnType_ = nullptr;
    std::vector<Type*> parameterTypes_;
    bool complete_ = true;
    const ast::AstNode* declaration_ = nullptr;
};
}
