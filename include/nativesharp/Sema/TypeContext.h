#pragma once

#include "nativesharp/Sema/Type.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::sema
{
class TypeContext final
{
public:
    TypeContext();
    TypeContext(const TypeContext&) = delete;
    TypeContext& operator=(const TypeContext&) = delete;
    ~TypeContext();

    Type* builtin(TypeKind kind) const noexcept;
    Type* qualified(Type* type, OwnershipKind ownership);
    Type* pointerTo(Type* pointee);
    Type* arrayOf(Type* element, std::uint64_t count);
    Type* stringLiteral(std::uint64_t count);
    Type* functionType(Type* returnType, const std::vector<Type*>& parameters);
    Type* createStruct(std::string_view name, const ast::AstNode* declaration);
    Type* createEnum(std::string_view name, const ast::AstNode* declaration);

private:
    Type* create(TypeKind kind);

    std::vector<Type*> ownedTypes_;
    std::vector<Type*> builtins_;
};
}
