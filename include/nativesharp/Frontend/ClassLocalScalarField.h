#pragma once

#include "nativesharp/Frontend/ClassMemberSymbol.h"
#include "nativesharp/Frontend/BuiltinIntegerTypes.h"

#include <vector>

namespace nativesharp::frontend
{
[[nodiscard]] inline bool classLocalCallableMaySuspend(const ClassMemberSymbol& callable)
{
    return callable.effects.contains(sema::Effect::Async) ||
        callable.effects.contains(sema::Effect::Generator);
}

[[nodiscard]] inline bool isClassLocalScalarType(std::string_view typeKey)
{
    return builtins::isIntegerTypeKey(typeKey) || typeKey == "builtin:bool" ||
        typeKey == "builtin:float" || typeKey == "builtin:double";
}

[[nodiscard]] inline bool isClassLocalScalarLiteral(const ast::AstNode* value, std::string_view typeKey)
{
    if (value == nullptr) return false;
    return (builtins::isIntegerTypeKey(typeKey) &&
            value->kind() == ast::AstNodeKind::IntegerLiteralExpression) ||
        (typeKey == "builtin:bool" && value->kind() == ast::AstNodeKind::BooleanLiteralExpression) ||
        ((typeKey == "builtin:float" || typeKey == "builtin:double") &&
         value->kind() == ast::AstNodeKind::FloatingLiteralExpression);
}

// Bounded literal-only expressions cannot refer to a receiver or call user code.
// The final construction gate must additionally verify semantic types for every node.
[[nodiscard]] inline bool isClassLocalScalarExpression(const ast::AstNode* value, std::string_view typeKey)
{
    if (isClassLocalScalarLiteral(value, typeKey)) return true;
    if (typeKey != "builtin:int" && typeKey != "builtin:bool") return false;
    std::vector<const ast::AstNode*> pending{value};
    std::size_t count = 0;
    while (!pending.empty())
    {
        const ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node == nullptr || ++count > 64) return false;
        if (typeKey == "builtin:int")
        {
            if (node->kind() == ast::AstNodeKind::IntegerLiteralExpression && node->integerValue() <= 1000000)
                continue;
            if (node->kind() != ast::AstNodeKind::BinaryExpression ||
                (node->tokenKind() != lexer::TokenKind::Plus && node->tokenKind() != lexer::TokenKind::Minus))
                return false;
            pending.push_back(node->first());
            pending.push_back(node->second());
        }
        else
        {
            if (node->kind() == ast::AstNodeKind::BooleanLiteralExpression) continue;
            if (node->kind() != ast::AstNodeKind::UnaryExpression || node->tokenKind() != lexer::TokenKind::Bang)
                return false;
            pending.push_back(node->first());
        }
    }
    return true;
}

// Source-declared scalar fields use only zero initialization or a scalar literal;
// neither form can call user code or carry a reference to the object.
[[nodiscard]] inline bool isClassLocalScalarFieldCandidate(const ClassMemberSymbol& field)
{
    const ast::AstNode* initial = field.syntax != nullptr ? field.syntax->first() : nullptr;
    const bool directZeroArgumentCall = initial != nullptr &&
        initial->kind() == ast::AstNodeKind::CallExpression && initial->first() != nullptr &&
        initial->first()->kind() == ast::AstNodeKind::IdentifierExpression && initial->children().empty() &&
        initial->second() == nullptr && initial->third() == nullptr;
    return field.kind == ClassMemberKind::Field && !field.isStatic && !field.isPrecompiled &&
        !field.isCompilerGenerated && !field.isEventBacking && !field.isBitField &&
        !field.isAnonymousMemberPromotion && !field.explicitFieldOffset.has_value() &&
        field.declaredOwnership == sema::OwnershipKind::None && isClassLocalScalarType(field.declaredTypeKey) &&
        field.syntax != nullptr && !field.syntax->isReference() &&
        (initial == nullptr || isClassLocalScalarExpression(initial, field.declaredTypeKey) || directZeroArgumentCall) &&
        field.syntax->auxiliaryText().empty();
}

[[nodiscard]] inline bool isClassLocalPlainScalarField(const ClassMemberSymbol& field)
{
    return isClassLocalScalarFieldCandidate(field) &&
        (field.syntax->first() == nullptr || field.syntax->first()->kind() != ast::AstNodeKind::CallExpression);
}

// An uninitialized ordinary int-array field is a non-owning, zeroed slot.
// Uses of the field still need their own receiver and lifetime proof.
[[nodiscard]] inline bool isClassLocalEmptyIntArrayField(const ClassMemberSymbol& field)
{
    return field.kind == ClassMemberKind::Field && !field.isStatic && !field.isPrecompiled &&
        !field.isCompilerGenerated && !field.isEventBacking && !field.isBitField && !field.isVolatile &&
        !field.isAnonymousMemberPromotion && !field.explicitFieldOffset.has_value() &&
        field.declaredOwnership == sema::OwnershipKind::None && field.declaredTypeKey == "builtin:int[]" &&
        field.syntax != nullptr && !field.syntax->isReference() && field.syntax->first() == nullptr &&
        field.syntax->auxiliaryText().empty();
}

// A plain class-reference field with no initializer is a zeroed, non-owning
// slot. Callers must resolve the field type to a class before using this test.
[[nodiscard]] inline bool isClassLocalEmptyClassReferenceField(const ClassMemberSymbol& field, bool isClass)
{
    return isClass && field.kind == ClassMemberKind::Field && !field.isStatic && !field.isPrecompiled &&
        !field.isCompilerGenerated && !field.isEventBacking && !field.isBitField && !field.isVolatile &&
        !field.isAnonymousMemberPromotion && !field.explicitFieldOffset.has_value() &&
        field.declaredOwnership == sema::OwnershipKind::None && field.syntax != nullptr &&
        !field.syntax->isReference() && !field.syntax->hasModifier(lexer::TokenKind::KwRaii) &&
        field.syntax->first() == nullptr && field.syntax->auxiliaryText().empty();
}
}
