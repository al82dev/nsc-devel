#include "nativesharp/Sema/SemanticAnalyzer.h"

#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "../Internal/SemanticAnalyzerSupport.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>

namespace nativesharp::sema
{
namespace
{
using lexer::TokenKind;
using semantic_analyzer::bitsFromSigned;
using semantic_analyzer::isNegativeConstant;
using semantic_analyzer::maskForWidth;
using semantic_analyzer::signedFromBits;
using semantic_analyzer::underlyingBinaryOperator;
}

Type* SemanticAnalyzer::resolveType(ast::TypeSyntax* syntax)
{
    if (syntax == nullptr)
    {
        return types_.builtin(TypeKind::Error);
    }

    Type* type = resolveBaseType(syntax);
    if (type == nullptr)
    {
        return types_.builtin(TypeKind::Error);
    }

    for (const ast::TypeModifierSyntax& modifier : syntax->modifiers())
    {
        if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer)
        {
            type = types_.pointerTo(type);
            continue;
        }

        if (type->kind() == TypeKind::Void || !type->isComplete())
        {
            report(
                modifier.range,
                diagnostics::DiagnosticId::InvalidArrayCount,
                "fixed array element type must be complete and non-void in NativeSharp 2.0");
            type = types_.builtin(TypeKind::Error);
            continue;
        }

        ExpressionInfo countInfo = analyzeExpression(modifier.arrayCount);
        const std::optional<std::int64_t> signedCount = constantAsSigned(countInfo);
        const std::optional<std::uint64_t> unsignedCountValue = constantAsUnsigned(countInfo);
        if (!countInfo.isConstant || !isInteger(countInfo.type) ||
            (signedCount.has_value() && *signedCount <= 0) ||
            !unsignedCountValue.has_value() ||
            *unsignedCountValue == 0)
        {
            report(
                modifier.range,
                diagnostics::DiagnosticId::InvalidArrayCount,
                "fixed array count must be a positive integer constant expression representable as nuint");
            type = types_.builtin(TypeKind::Error);
            continue;
        }

        const std::uint64_t unsignedCount = *unsignedCountValue;
        const std::optional<target::TypeLayout> elementLayout = layoutOfType(type);
        if (!elementLayout.has_value() || !dataLayout_.layoutArray(*elementLayout, unsignedCount).has_value())
        {
            report(
                modifier.range,
                diagnostics::DiagnosticId::InvalidArrayCount,
                "fixed array size overflows the target object-size calculation");
            type = types_.builtin(TypeKind::Error);
            continue;
        }

        type = types_.arrayOf(type, unsignedCount);
    }

    OwnershipKind ownership = OwnershipKind::None;
    switch (syntax->ownershipQualifier())
    {
        case ast::OwnershipQualifierSyntaxKind::None:
            ownership = OwnershipKind::None;
            break;
        case ast::OwnershipQualifierSyntaxKind::Owned:
            ownership = OwnershipKind::Owned;
            break;
        case ast::OwnershipQualifierSyntaxKind::Borrowed:
            ownership = OwnershipKind::Borrowed;
            break;
    }

    return types_.qualified(type, ownership);
}

Type* SemanticAnalyzer::resolveBaseType(ast::TypeSyntax* syntax)
{
    switch (syntax->baseTokenKind())
    {
        case TokenKind::KwVoid: return types_.builtin(TypeKind::Void);
        case TokenKind::KwBool: return types_.builtin(TypeKind::Bool);
        case TokenKind::KwByte: return types_.builtin(TypeKind::Byte);
        case TokenKind::KwChar: return types_.builtin(TypeKind::Char);
        case TokenKind::KwShort: return types_.builtin(TypeKind::Short);
        case TokenKind::KwUShort: return types_.builtin(TypeKind::UShort);
        case TokenKind::KwInt: return types_.builtin(TypeKind::Int);
        case TokenKind::KwUInt: return types_.builtin(TypeKind::UInt);
        case TokenKind::KwLong: return types_.builtin(TypeKind::Long);
        case TokenKind::KwULong: return types_.builtin(TypeKind::ULong);
        case TokenKind::KwNInt: return types_.builtin(TypeKind::NInt);
        case TokenKind::KwNUInt: return types_.builtin(TypeKind::NUInt);
        case TokenKind::KwFloat: return types_.builtin(TypeKind::Float);
        case TokenKind::KwDouble: return types_.builtin(TypeKind::Double);
        case TokenKind::Identifier:
        {
            Symbol* symbol = globalScope_->lookupLocal(syntax->baseName());
            if (symbol == nullptr || (symbol->kind() != SymbolKind::Struct && symbol->kind() != SymbolKind::Enum))
            {
                report(
                    syntax->range(),
                    diagnostics::DiagnosticId::UnknownTypeName,
                    "unknown type name '" + syntax->baseName() + "'");
                return types_.builtin(TypeKind::Error);
            }
            return symbol->type();
        }
        default:
            break;
    }

    report(syntax->range(), diagnostics::DiagnosticId::ExpectedType, "invalid type syntax");
    return types_.builtin(TypeKind::Error);
}

bool SemanticAnalyzer::validateObjectType(Type* type, SourceRange range, bool allowArray)
{
    if (type == nullptr || type->kind() == TypeKind::Error)
    {
        return false;
    }
    if (type->kind() == TypeKind::Void)
    {
        report(range, diagnostics::DiagnosticId::InvalidType, "object cannot have type void");
        return false;
    }
    if (type->kind() == TypeKind::Function)
    {
        report(range, diagnostics::DiagnosticId::InvalidType, "object cannot have function type");
        return false;
    }
    if (!allowArray && type->kind() == TypeKind::Array)
    {
        report(range, diagnostics::DiagnosticId::InvalidType, "array type is not valid in this context");
        return false;
    }
    if (!type->isComplete())
    {
        report(range, diagnostics::DiagnosticId::IncompleteType, "object requires a complete type");
        return false;
    }
    return true;
}

bool SemanticAnalyzer::validateExternCSignature(
    ast::AstNode* functionNode,
    Type* returnType,
    std::span<Type* const> parameterTypes)
{
    bool valid = true;
    if (!isCAbiCompatibleDirectType(returnType, true))
    {
        report(
            functionNode->range(),
            diagnostics::DiagnosticId::InvalidExternCSignature,
            "extern C return type '" + returnType->displayName() + "' is not ABI-compatible in NativeSharp 2.0");
        valid = false;
    }

    for (std::size_t index = 0; index < parameterTypes.size(); ++index)
    {
        Type* parameterType = parameterTypes[index];
        if (!isCAbiCompatibleDirectType(parameterType, false))
        {
            SourceRange range = functionNode->range();
            if (index < functionNode->children().size())
            {
                range = functionNode->children()[index]->range();
            }
            report(
                range,
                diagnostics::DiagnosticId::InvalidExternCSignature,
                "extern C parameter type '" + parameterType->displayName() +
                    "' is not ABI-compatible in NativeSharp 2.0");
            valid = false;
        }
    }
    return valid;
}

bool SemanticAnalyzer::isCAbiCompatibleDirectType(Type* type, bool allowVoid) const
{
    if (type == nullptr || type->kind() == TypeKind::Error)
    {
        return true;
    }
    if (type->kind() == TypeKind::Void)
    {
        return allowVoid;
    }
    if (!type->isComplete())
    {
        return false;
    }

    switch (type->kind())
    {
        case TypeKind::Bool:
        case TypeKind::Byte:
        case TypeKind::Char:
        case TypeKind::Short:
        case TypeKind::UShort:
        case TypeKind::Int:
        case TypeKind::UInt:
        case TypeKind::Long:
        case TypeKind::ULong:
        case TypeKind::NInt:
        case TypeKind::NUInt:
        case TypeKind::Float:
        case TypeKind::Double:
        case TypeKind::Pointer:
        case TypeKind::Enum:
            return true;
        case TypeKind::Struct:
            return isCAbiCompatibleStruct(type);
        default:
            return false;
    }
}

bool SemanticAnalyzer::isCAbiCompatibleStruct(Type* type) const
{
    std::unordered_set<Type*> active;
    std::unordered_set<Type*> validated;
    const auto validateField = [&](auto&& self, Type* fieldType) -> bool
    {
        if (fieldType != nullptr)
        {
            fieldType = fieldType->unqualifiedType();
        }
        if (fieldType == nullptr || fieldType->kind() == TypeKind::Error || !fieldType->isComplete())
        {
            return false;
        }

        if (fieldType->kind() == TypeKind::Array)
        {
            return fieldType->count() != 0 && self(self, fieldType->elementType());
        }
        if (fieldType->kind() != TypeKind::Struct)
        {
            return isCAbiCompatibleDirectType(fieldType, false);
        }
        if (active.contains(fieldType))
        {
            return false;
        }
        if (validated.contains(fieldType))
        {
            return true;
        }

        const auto iterator = structFieldTypes_.find(fieldType);
        if (iterator == structFieldTypes_.end() || iterator->second.empty())
        {
            return false;
        }

        active.insert(fieldType);
        for (Type* nestedField : iterator->second)
        {
            if (!self(self, nestedField))
            {
                active.erase(fieldType);
                return false;
            }
        }
        active.erase(fieldType);

        if (!layoutOfType(fieldType).has_value())
        {
            return false;
        }
        validated.insert(fieldType);
        return true;
    };

    return validateField(validateField, type);
}

bool SemanticAnalyzer::isCAbiCompatibleStructField(Type* type) const
{
    if (type == nullptr || type->kind() == TypeKind::Error || !type->isComplete())
    {
        return false;
    }

    if (type->kind() == TypeKind::Array)
    {
        return type->count() != 0 && isCAbiCompatibleStructField(type->elementType());
    }
    if (type->kind() == TypeKind::Struct)
    {
        return isCAbiCompatibleStruct(type);
    }
    return isCAbiCompatibleDirectType(type, false);
}

bool SemanticAnalyzer::isInteger(Type* type) const noexcept
{
    if (type == nullptr)
    {
        return false;
    }
    return type->kind() == TypeKind::Byte || type->kind() == TypeKind::Char || type->kind() == TypeKind::Short ||
        type->kind() == TypeKind::UShort || type->kind() == TypeKind::Int || type->kind() == TypeKind::UInt ||
        type->kind() == TypeKind::Long || type->kind() == TypeKind::ULong || type->kind() == TypeKind::NInt ||
        type->kind() == TypeKind::NUInt;
}

bool SemanticAnalyzer::isSignedInteger(Type* type) const noexcept
{
    return type != nullptr && (type->kind() == TypeKind::Short || type->kind() == TypeKind::Int ||
        type->kind() == TypeKind::Long || type->kind() == TypeKind::NInt);
}

bool SemanticAnalyzer::isUnsignedInteger(Type* type) const noexcept
{
    return isInteger(type) && !isSignedInteger(type);
}

bool SemanticAnalyzer::isFloating(Type* type) const noexcept
{
    return type != nullptr && (type->kind() == TypeKind::Float || type->kind() == TypeKind::Double);
}

bool SemanticAnalyzer::isNumeric(Type* type) const noexcept
{
    return isInteger(type) || isFloating(type);
}

bool SemanticAnalyzer::isPointer(Type* type) const noexcept
{
    return type != nullptr && type->kind() == TypeKind::Pointer;
}

bool SemanticAnalyzer::isScalarIncrementType(Type* type) const noexcept
{
    return isNumeric(type) ||
        (isPointer(type) &&
         type->elementType()->kind() != TypeKind::Void &&
         type->elementType()->isComplete());
}

bool SemanticAnalyzer::isSized(Type* type) const noexcept
{
    return sizeOf(type).has_value();
}

std::uint32_t SemanticAnalyzer::integerWidth(Type* type) const noexcept
{
    switch (type->kind())
    {
        case TypeKind::Byte:
        case TypeKind::Char:
            return 8;
        case TypeKind::Short:
        case TypeKind::UShort:
            return 16;
        case TypeKind::Int:
        case TypeKind::UInt:
            return 32;
        case TypeKind::Long:
        case TypeKind::ULong:
        case TypeKind::NInt:
        case TypeKind::NUInt:
            return 64;
        default:
            return 0;
    }
}

Type* SemanticAnalyzer::promoteInteger(Type* type) const noexcept
{
    if (type->kind() == TypeKind::Byte || type->kind() == TypeKind::Char || type->kind() == TypeKind::Short ||
        type->kind() == TypeKind::UShort)
    {
        return types_.builtin(TypeKind::Int);
    }
    return type;
}

Type* SemanticAnalyzer::commonIntegralType(Type* left, Type* right) const noexcept
{
    if (!isInteger(left) || !isInteger(right))
    {
        return nullptr;
    }

    left = promoteInteger(left);
    right = promoteInteger(right);
    if (left == right)
    {
        return left;
    }

    const std::array<TypeKind, 6> candidates = {
        TypeKind::Int,
        TypeKind::UInt,
        TypeKind::Long,
        TypeKind::ULong,
        TypeKind::NInt,
        TypeKind::NUInt};

    for (TypeKind kind : candidates)
    {
        Type* candidate = types_.builtin(kind);
        if (canStandardImplicitConvert(left, candidate) && canStandardImplicitConvert(right, candidate))
        {
            return candidate;
        }
    }
    return nullptr;
}

Type* SemanticAnalyzer::commonNumericType(Type* left, Type* right) const noexcept
{
    if (!isNumeric(left) || !isNumeric(right))
    {
        return nullptr;
    }

    if (left->kind() == TypeKind::Double || right->kind() == TypeKind::Double)
    {
        Type* destination = types_.builtin(TypeKind::Double);
        return canStandardImplicitConvert(left, destination) && canStandardImplicitConvert(right, destination)
            ? destination
            : nullptr;
    }

    if (left->kind() == TypeKind::Float || right->kind() == TypeKind::Float)
    {
        Type* destination = types_.builtin(TypeKind::Float);
        return canStandardImplicitConvert(left, destination) && canStandardImplicitConvert(right, destination)
            ? destination
            : nullptr;
    }

    return commonIntegralType(left, right);
}

ConversionKind SemanticAnalyzer::classifyImplicitConversion(
    const ExpressionInfo& source,
    Type* destination) const noexcept
{
    if (source.type == destination)
    {
        return ConversionKind::Identity;
    }

    if (isInteger(source.type) && destination->kind() == TypeKind::Int &&
        (source.type->kind() == TypeKind::Byte || source.type->kind() == TypeKind::Char ||
         source.type->kind() == TypeKind::Short || source.type->kind() == TypeKind::UShort))
    {
        return ConversionKind::IntegralPromotion;
    }

    if (source.isConstant && isInteger(source.type) && isInteger(destination) &&
        !canStandardImplicitConvert(source.type, destination) && constantFits(source.constantValue, destination))
    {
        return ConversionKind::ImplicitConstantInteger;
    }

    if (source.type->kind() == TypeKind::NullLiteral && isPointer(destination))
    {
        return ConversionKind::ImplicitNullPointer;
    }

    if (source.type->kind() == TypeKind::StringLiteral && isPointer(destination) &&
        destination->elementType()->kind() == TypeKind::Char)
    {
        return ConversionKind::ImplicitStringLiteralToCharPointer;
    }

    if (isPointer(source.type) && isPointer(destination) && destination->elementType()->kind() == TypeKind::Void)
    {
        return ConversionKind::ImplicitPointerToVoid;
    }

    return ConversionKind::ImplicitNumeric;
}

ConversionKind SemanticAnalyzer::classifyExplicitConversion(Type* source, Type* destination) const noexcept
{
    if (source == destination)
    {
        return ConversionKind::Identity;
    }
    if (destination->kind() == TypeKind::Void)
    {
        return ConversionKind::ToVoid;
    }
    if (source->kind() == TypeKind::NullLiteral && isPointer(destination))
    {
        return ConversionKind::ExplicitNullPointer;
    }
    if ((source->kind() == TypeKind::Enum && isInteger(destination)) ||
        (isInteger(source) && destination->kind() == TypeKind::Enum))
    {
        return ConversionKind::ExplicitEnum;
    }
    if ((isPointer(source) && (destination->kind() == TypeKind::NInt || destination->kind() == TypeKind::NUInt)) ||
        (isPointer(destination) && (source->kind() == TypeKind::NInt || source->kind() == TypeKind::NUInt)))
    {
        return ConversionKind::ExplicitPointerInteger;
    }
    if (isPointer(source) && isPointer(destination))
    {
        return ConversionKind::ExplicitPointer;
    }
    return ConversionKind::ExplicitNumeric;
}

void SemanticAnalyzer::recordImplicitConversion(
    const ast::AstNode* owner,
    const ast::AstNode* operand,
    const ExpressionInfo& source,
    Type* destination,
    int operandIndex)
{
    if (owner == nullptr ||
        operand == nullptr ||
        source.type == nullptr ||
        destination == nullptr ||
        source.type == destination ||
        source.type->kind() == TypeKind::Error || destination->kind() == TypeKind::Error)
    {
        return;
    }

    model_.addConversion(
        owner,
        ConversionInfo{
            classifyImplicitConversion(source, destination),
            operandIndex >= 0 ? ConversionRole::Operand : ConversionRole::Contextual,
            operand,
            source.type,
            destination,
            operandIndex});
}

void SemanticAnalyzer::recordIntegralPromotion(
    const ast::AstNode* owner,
    const ast::AstNode* operand,
    Type* source,
    Type* destination,
    int operandIndex)
{
    if (owner == nullptr || operand == nullptr || source == nullptr || destination == nullptr || source == destination)
    {
        return;
    }

    model_.addConversion(
        owner,
        ConversionInfo{
            ConversionKind::IntegralPromotion,
            ConversionRole::Operand,
            operand,
            source,
            destination,
            operandIndex});
}

void SemanticAnalyzer::recordExplicitConversion(
    const ast::AstNode* owner,
    const ast::AstNode* operand,
    Type* source,
    Type* destination)
{
    if (owner == nullptr || operand == nullptr || source == nullptr || destination == nullptr)
    {
        return;
    }

    model_.addConversion(
        owner,
        ConversionInfo{
            classifyExplicitConversion(source, destination),
            ConversionRole::Contextual,
            operand,
            source,
            destination,
            0});
}

void SemanticAnalyzer::recordOperator(
    const ast::AstNode* node,
    lexer::TokenKind tokenKind,
    Type* leftType,
    Type* rightType,
    Type* operationType,
    Type* resultType,
    bool unary)
{
    if (node == nullptr)
    {
        return;
    }

    model_.setOperator(
        node,
        ResolvedOperatorInfo{tokenKind, leftType, rightType, operationType, resultType, unary});
}

bool SemanticAnalyzer::canImplicitlyConvert(const ExpressionInfo& source, Type* destination) const
{
    if (source.type == destination || source.type->kind() == TypeKind::Error || destination->kind() == TypeKind::Error)
    {
        return true;
    }

    if (source.isConstant &&
        isInteger(source.type) &&
        isInteger(destination) &&
        constantFits(source.constantValue, destination))
    {
        return true;
    }

    if (source.type->kind() == TypeKind::NullLiteral && isPointer(destination))
    {
        return true;
    }

    if (source.type->kind() == TypeKind::StringLiteral && isPointer(destination) &&
        destination->elementType()->kind() == TypeKind::Char)
    {
        return true;
    }

    if (isPointer(source.type) && isPointer(destination) && destination->elementType()->kind() == TypeKind::Void)
    {
        return true;
    }

    return canStandardImplicitConvert(source.type, destination);
}

bool SemanticAnalyzer::canStandardImplicitConvert(Type* source, Type* destination) const noexcept
{
    if (source == destination)
    {
        return true;
    }

    switch (source->kind())
    {
        case TypeKind::Byte:
        case TypeKind::Char:
            return destination->kind() == TypeKind::Short || destination->kind() == TypeKind::UShort ||
                destination->kind() == TypeKind::Int || destination->kind() == TypeKind::UInt ||
                destination->kind() == TypeKind::Long || destination->kind() == TypeKind::ULong ||
                destination->kind() == TypeKind::NInt || destination->kind() == TypeKind::NUInt ||
                destination->kind() == TypeKind::Float || destination->kind() == TypeKind::Double;
        case TypeKind::Short:
            return destination->kind() == TypeKind::Int || destination->kind() == TypeKind::Long ||
                destination->kind() == TypeKind::NInt || destination->kind() == TypeKind::Float ||
                destination->kind() == TypeKind::Double;
        case TypeKind::UShort:
            return destination->kind() == TypeKind::Int || destination->kind() == TypeKind::UInt ||
                destination->kind() == TypeKind::Long || destination->kind() == TypeKind::ULong ||
                destination->kind() == TypeKind::NInt || destination->kind() == TypeKind::NUInt ||
                destination->kind() == TypeKind::Float || destination->kind() == TypeKind::Double;
        case TypeKind::Int:
            return destination->kind() == TypeKind::Long || destination->kind() == TypeKind::NInt ||
                destination->kind() == TypeKind::Float || destination->kind() == TypeKind::Double;
        case TypeKind::UInt:
            return destination->kind() == TypeKind::Long || destination->kind() == TypeKind::ULong ||
                destination->kind() == TypeKind::NUInt || destination->kind() == TypeKind::Float ||
                destination->kind() == TypeKind::Double;
        case TypeKind::Long:
        case TypeKind::ULong:
        case TypeKind::NInt:
        case TypeKind::NUInt:
            return destination->kind() == TypeKind::Float || destination->kind() == TypeKind::Double;
        case TypeKind::Float:
            return destination->kind() == TypeKind::Double;
        default:
            return false;
    }
}

bool SemanticAnalyzer::canExplicitlyConvert(Type* source, Type* destination) const noexcept
{
    if (source == destination)
    {
        return true;
    }
    if (destination->kind() == TypeKind::Void)
    {
        return true;
    }
    if (source->kind() == TypeKind::Bool || destination->kind() == TypeKind::Bool)
    {
        return false;
    }
    if (isNumeric(source) && isNumeric(destination))
    {
        return true;
    }
    if ((source->kind() == TypeKind::Enum && isInteger(destination)) ||
        (isInteger(source) && destination->kind() == TypeKind::Enum))
    {
        return true;
    }
    if (isPointer(source) && isPointer(destination))
    {
        return true;
    }
    if ((isPointer(source) && (destination->kind() == TypeKind::NInt || destination->kind() == TypeKind::NUInt)) ||
        (isPointer(destination) && (source->kind() == TypeKind::NInt || source->kind() == TypeKind::NUInt)))
    {
        return true;
    }
    if (source->kind() == TypeKind::NullLiteral && isPointer(destination))
    {
        return true;
    }
    return false;
}

bool SemanticAnalyzer::constantFits(const ConstantValue& value, Type* destination) const noexcept
{
    if (!isInteger(destination))
    {
        return false;
    }

    const std::uint32_t width = integerWidth(destination);
    if (value.kind() == ConstantKind::SignedInteger)
    {
        const std::int64_t source = value.signedInteger();
        if (isSignedInteger(destination))
        {
            if (width == 64) return true;
            const std::int64_t minimum = -(std::int64_t{1} << (width - 1));
            const std::int64_t maximum = (std::int64_t{1} << (width - 1)) - 1;
            return source >= minimum && source <= maximum;
        }
        if (source < 0)
        {
            return false;
        }
        const std::uint64_t maximum = maskForWidth(width);
        return static_cast<std::uint64_t>(source) <= maximum;
    }

    if (value.kind() == ConstantKind::UnsignedInteger)
    {
        const std::uint64_t source = value.unsignedInteger();
        if (isUnsignedInteger(destination))
        {
            return source <= maskForWidth(width);
        }
        const std::uint64_t maximum = width == 64
            ? static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
            : (std::uint64_t{1} << (width - 1)) - 1;
        return source <= maximum;
    }

    return false;
}

ConstantValue SemanticAnalyzer::convertConstant(
    const ConstantValue& value,
    Type* source,
    Type* destination) const
{
    if (destination->kind() == TypeKind::Enum)
    {
        destination = types_.builtin(TypeKind::Int);
    }
    if (source->kind() == TypeKind::Enum)
    {
        source = types_.builtin(TypeKind::Int);
    }

    if (isInteger(source) && isInteger(destination))
    {
        std::uint64_t bits = 0;
        if (value.kind() == ConstantKind::SignedInteger)
        {
            // Conversion to uint64_t is defined modulo 2^64 and therefore preserves the sign
            // extension needed when a signed source widens. The destination mask below also
            // gives the exact truncation for narrower targets.
            bits = static_cast<std::uint64_t>(value.signedInteger());
        }
        else
        {
            bits = value.unsignedInteger();
        }
        bits &= maskForWidth(integerWidth(destination));
        return isSignedInteger(destination)
            ? ConstantValue::signedInteger(signedFromBits(bits, integerWidth(destination)))
            : ConstantValue::unsignedInteger(bits);
    }

    if (isInteger(source) && isFloating(destination))
    {
        const double number = value.kind() == ConstantKind::SignedInteger
            ? static_cast<double>(value.signedInteger())
            : static_cast<double>(value.unsignedInteger());
        return ConstantValue::floating(
            destination->kind() == TypeKind::Float
                ? static_cast<double>(static_cast<float>(number))
                : number);
    }

    if (isFloating(source) && isFloating(destination))
    {
        const double number = value.floating();
        return ConstantValue::floating(
            destination->kind() == TypeKind::Float
                ? static_cast<double>(static_cast<float>(number))
                : number);
    }

    if (isFloating(source) && isInteger(destination))
    {
        const double truncated = std::trunc(value.floating());
        if (isSignedInteger(destination))
        {
            return ConstantValue::signedInteger(static_cast<std::int64_t>(truncated));
        }
        return ConstantValue::unsignedInteger(static_cast<std::uint64_t>(truncated));
    }

    return value;
}

std::optional<target::TypeLayout> SemanticAnalyzer::layoutOfType(Type* type) const
{
    if (type != nullptr)
    {
        type = type->unqualifiedType();
    }
    if (type == nullptr || !type->isComplete())
    {
        return std::nullopt;
    }

    using target::BuiltinLayoutKind;
    switch (type->kind())
    {
        case TypeKind::Bool: return dataLayout_.layoutOf(BuiltinLayoutKind::Bool);
        case TypeKind::Byte: return dataLayout_.layoutOf(BuiltinLayoutKind::Byte);
        case TypeKind::Char: return dataLayout_.layoutOf(BuiltinLayoutKind::Char);
        case TypeKind::Short: return dataLayout_.layoutOf(BuiltinLayoutKind::Short);
        case TypeKind::UShort: return dataLayout_.layoutOf(BuiltinLayoutKind::UShort);
        case TypeKind::Int: return dataLayout_.layoutOf(BuiltinLayoutKind::Int);
        case TypeKind::UInt: return dataLayout_.layoutOf(BuiltinLayoutKind::UInt);
        case TypeKind::Long: return dataLayout_.layoutOf(BuiltinLayoutKind::Long);
        case TypeKind::ULong: return dataLayout_.layoutOf(BuiltinLayoutKind::ULong);
        case TypeKind::NInt: return dataLayout_.layoutOf(BuiltinLayoutKind::NInt);
        case TypeKind::NUInt: return dataLayout_.layoutOf(BuiltinLayoutKind::NUInt);
        case TypeKind::Float: return dataLayout_.layoutOf(BuiltinLayoutKind::Float);
        case TypeKind::Double: return dataLayout_.layoutOf(BuiltinLayoutKind::Double);
        case TypeKind::Pointer: return dataLayout_.layoutOf(BuiltinLayoutKind::Pointer);
        case TypeKind::Enum: return dataLayout_.layoutOf(BuiltinLayoutKind::Enum);
        case TypeKind::StringLiteral:
            return target::TypeLayout{type->count(), 1};
        case TypeKind::Array:
        {
            const std::optional<target::TypeLayout> element = layoutOfType(type->elementType());
            if (!element.has_value()) return std::nullopt;
            const auto layout = dataLayout_.layoutArray(*element, type->count());
            if (!layout.has_value()) return std::nullopt;
            return target::TypeLayout{layout->size, layout->alignment};
        }
        case TypeKind::Struct:
        {
            const auto iterator = structFieldTypes_.find(type);
            if (iterator == structFieldTypes_.end()) return std::nullopt;
            std::vector<target::TypeLayout> fields;
            fields.reserve(iterator->second.size());
            for (Type* fieldType : iterator->second)
            {
                const std::optional<target::TypeLayout> field = layoutOfType(fieldType);
                if (!field.has_value()) return std::nullopt;
                fields.push_back(*field);
            }
            const auto layout = dataLayout_.layoutStruct(fields);
            if (!layout.has_value()) return std::nullopt;
            return target::TypeLayout{layout->size, layout->alignment};
        }
        default:
            return std::nullopt;
    }
}

std::optional<std::uint64_t> SemanticAnalyzer::sizeOf(Type* type) const
{
    const std::optional<target::TypeLayout> layout = layoutOfType(type);
    return layout.has_value() ? std::optional<std::uint64_t>(layout->size) : std::nullopt;
}

std::optional<std::int64_t> SemanticAnalyzer::constantAsSigned(const ExpressionInfo& info) const noexcept
{
    if (!info.isConstant)
    {
        return std::nullopt;
    }
    if (info.constantValue.kind() == ConstantKind::SignedInteger)
    {
        return info.constantValue.signedInteger();
    }
    if (info.constantValue.kind() == ConstantKind::UnsignedInteger &&
        info.constantValue.unsignedInteger() <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
    {
        return static_cast<std::int64_t>(info.constantValue.unsignedInteger());
    }
    return std::nullopt;
}

std::optional<std::uint64_t> SemanticAnalyzer::constantAsUnsigned(const ExpressionInfo& info) const noexcept
{
    if (!info.isConstant)
    {
        return std::nullopt;
    }
    if (info.constantValue.kind() == ConstantKind::UnsignedInteger)
    {
        return info.constantValue.unsignedInteger();
    }
    if (info.constantValue.kind() == ConstantKind::SignedInteger && info.constantValue.signedInteger() >= 0)
    {
        return static_cast<std::uint64_t>(info.constantValue.signedInteger());
    }
    return std::nullopt;
}

ExpressionInfo SemanticAnalyzer::makeError(ast::AstNode* node)
{
    return record(node, makeRValue(types_.builtin(TypeKind::Error)));
}

ExpressionInfo SemanticAnalyzer::makeRValue(Type* type) const noexcept
{
    ExpressionInfo info;
    info.type = type;
    info.category = ValueCategory::RValue;
    return info;
}

ExpressionInfo SemanticAnalyzer::record(ast::AstNode* node, ExpressionInfo info)
{
    model_.set(node, info);
    if (info.symbol != nullptr)
    {
        model_.bindSymbol(node, info.symbol);
    }
    return info;
}


}
