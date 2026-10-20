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

ExpressionInfo SemanticAnalyzer::analyzeExpression(ast::AstNode* node, bool allowFunctionDesignator)
{
    if (node == nullptr)
    {
        return makeRValue(types_.builtin(TypeKind::Void));
    }

    switch (node->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
            return analyzeIdentifier(node, allowFunctionDesignator);
        case ast::AstNodeKind::ThisExpression:
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidMemberAccess,
                "'this' is available only in the Stage 34 instance-method project pipeline");
            return makeError(node);
        case ast::AstNodeKind::IntegerLiteralExpression:
        case ast::AstNodeKind::FloatingLiteralExpression:
        case ast::AstNodeKind::CharacterLiteralExpression:
        case ast::AstNodeKind::StringLiteralExpression:
        case ast::AstNodeKind::BooleanLiteralExpression:
        case ast::AstNodeKind::NullLiteralExpression:
            return analyzeLiteral(node);
        case ast::AstNodeKind::UnaryExpression:
        case ast::AstNodeKind::PostfixExpression:
            return analyzeUnary(node);
        case ast::AstNodeKind::BinaryExpression:
            return analyzeBinary(node);
        case ast::AstNodeKind::ConditionalExpression:
            return analyzeConditional(node);
        case ast::AstNodeKind::AssignmentExpression:
            return analyzeAssignment(node);
        case ast::AstNodeKind::CallExpression:
            return analyzeCall(node);
        case ast::AstNodeKind::IndexExpression:
            return analyzeIndex(node);
        default:
            return analyzeExtendedExpression(node);
    }
}

ExpressionInfo SemanticAnalyzer::analyzeExtendedExpression(ast::AstNode* node)
{
    switch (node->kind())
    {
        case ast::AstNodeKind::MemberExpression:
            return analyzeMember(node, false);
        case ast::AstNodeKind::PointerMemberExpression:
            return analyzeMember(node, true);
        case ast::AstNodeKind::CastExpression:
            return analyzeCast(node);
        case ast::AstNodeKind::SizeofTypeExpression:
            return analyzeSizeof(node, true);
        case ast::AstNodeKind::SizeofExpression:
            return analyzeSizeof(node, false);
        case ast::AstNodeKind::AlignofTypeExpression:
            return analyzeAlignof(node, true);
        case ast::AstNodeKind::AlignofExpression:
            return analyzeAlignof(node, false);
        case ast::AstNodeKind::OffsetofExpression:
            return analyzeOffsetof(node);
        case ast::AstNodeKind::NewExpression:
            return analyzeNew(node);
        case ast::AstNodeKind::PlacementConstructionExpression:
            analyzeExpression(node->first());
            for (ast::AstNode* argument : node->children()) analyzeExpression(argument);
            report(
                node->range(),
                diagnostics::DiagnosticId::PlacementConstructionNotAvailable,
                "placement construction syntax is recognized but is not executable until item 318 lowering");
            return makeError(node);
        case ast::AstNodeKind::DeleteExpression:
            return analyzeDelete(node);
        case ast::AstNodeKind::StackallocExpression:
            return analyzeStackalloc(node);
        default:
            return makeError(node);
    }
}

ExpressionInfo SemanticAnalyzer::analyzeIdentifier(ast::AstNode* node, bool allowFunctionDesignator)
{
    Symbol* symbol = lookup(node->text());
    if (symbol == nullptr)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::UnknownIdentifier,
            "unknown identifier '" + node->text() + "'");
        return makeError(node);
    }

    if (symbol->kind() == SymbolKind::Struct || symbol->kind() == SymbolKind::Enum)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidOperand,
            "type name '" + node->text() + "' cannot be used as a runtime expression");
        return makeError(node);
    }

    if (symbol->kind() == SymbolKind::Function)
    {
        ExpressionInfo info = makeRValue(symbol->type());
        info.symbol = symbol;
        if (!allowFunctionDesignator)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "function name can only be used as the designator of a call in NativeSharp 2.0");
            info.type = types_.builtin(TypeKind::Error);
        }
        return record(node, info);
    }

    ExpressionInfo info;
    info.type = symbol->type();
    info.category = ValueCategory::LValue;
    info.modifiable = !symbol->isConst() && info.type->kind() != TypeKind::Array;
    info.addressable = !symbol->isConst();
    info.symbol = symbol;

    if (symbol->kind() == SymbolKind::EnumMember)
    {
        info.category = ValueCategory::RValue;
        info.modifiable = false;
        info.addressable = false;
        info.isConstant = true;
        info.constantValue = ConstantValue::signedInteger(symbol->enumValue());
    }
    else if (symbol->isConst() && symbol->hasConstantValue())
    {
        info.isConstant = true;
        info.constantValue = symbol->constantValue();
    }

    return record(node, info);
}

ExpressionInfo SemanticAnalyzer::analyzeLiteral(ast::AstNode* node)
{
    ExpressionInfo info;
    info.category = ValueCategory::RValue;
    info.isConstant = true;

    switch (node->kind())
    {
        case ast::AstNodeKind::IntegerLiteralExpression:
            return analyzeIntegerLiteral(node);

        case ast::AstNodeKind::FloatingLiteralExpression:
            info.type = (!node->text().empty() &&
                         (node->text().back() == 'f' || node->text().back() == 'F'))
                ? types_.builtin(TypeKind::Float)
                : types_.builtin(TypeKind::Double);
            info.constantValue = ConstantValue::floating(
                info.type->kind() == TypeKind::Float
                    ? static_cast<double>(static_cast<float>(node->floatingValue()))
                    : node->floatingValue());
            break;

        case ast::AstNodeKind::CharacterLiteralExpression:
            info.type = types_.builtin(TypeKind::Char);
            info.constantValue = ConstantValue::unsignedInteger(node->integerValue());
            break;

        case ast::AstNodeKind::StringLiteralExpression:
            info.type = types_.stringLiteral(static_cast<std::uint64_t>(node->text().size()) + 1);
            info.constantValue = ConstantValue::stringValue(node->text());
            break;

        case ast::AstNodeKind::BooleanLiteralExpression:
            info.type = types_.builtin(TypeKind::Bool);
            info.constantValue = ConstantValue::boolean(node->tokenKind() == TokenKind::KwTrue);
            break;

        case ast::AstNodeKind::NullLiteralExpression:
            info.type = types_.builtin(TypeKind::NullLiteral);
            info.constantValue = ConstantValue::nullValue();
            break;

        default:
            return makeError(node);
    }

    return record(node, info);
}

ExpressionInfo SemanticAnalyzer::analyzeIntegerLiteral(ast::AstNode* node)
{
    ExpressionInfo info;
    info.category = ValueCategory::RValue;
    info.isConstant = true;

    const std::string& text = node->text();
    bool unsignedSuffix = false;
    bool longSuffix = false;
    for (char value : text)
    {
        if (value == 'u' || value == 'U') unsignedSuffix = true;
        if (value == 'l' || value == 'L') longSuffix = true;
    }

    const std::uint64_t value = node->integerValue();
    if (unsignedSuffix && longSuffix)
    {
        info.type = types_.builtin(TypeKind::ULong);
        info.constantValue = ConstantValue::unsignedInteger(value);
    }
    else if (unsignedSuffix)
    {
        if (value <= std::numeric_limits<std::uint32_t>::max())
        {
            info.type = types_.builtin(TypeKind::UInt);
        }
        else
        {
            info.type = types_.builtin(TypeKind::ULong);
        }
        info.constantValue = ConstantValue::unsignedInteger(value);
    }
    else if (longSuffix)
    {
        if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidNumericLiteral,
                "L-suffixed integer literal must fit in NativeSharp long");
            return makeError(node);
        }
        info.type = types_.builtin(TypeKind::Long);
        info.constantValue = ConstantValue::signedInteger(static_cast<std::int64_t>(value));
    }
    else if (value <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
    {
        info.type = types_.builtin(TypeKind::Int);
        info.constantValue = ConstantValue::signedInteger(static_cast<std::int64_t>(value));
    }
    else if (value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
    {
        info.type = types_.builtin(TypeKind::Long);
        info.constantValue = ConstantValue::signedInteger(static_cast<std::int64_t>(value));
    }
    else
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidNumericLiteral,
            "unsuffixed integer literal does not fit in NativeSharp long; use an unsigned suffix");
        return makeError(node);
    }
    return record(node, info);
}

ExpressionInfo SemanticAnalyzer::analyzeUnary(ast::AstNode* node)
{
    ExpressionInfo operand = analyzeExpression(node->first());
    if (operand.type->kind() == TypeKind::Error)
    {
        return makeError(node);
    }

    const TokenKind operation = node->tokenKind();
    ExpressionInfo result;

    if (const auto storage = analyzeStorageUnary(node, operand, operation))
    {
        return *storage;
    }

    if (operation == TokenKind::Bang)
    {
        if (operand.type->kind() != TypeKind::Bool)
        {
            report(node->range(), diagnostics::DiagnosticId::InvalidOperand, "operator ! requires bool operand");
            return makeError(node);
        }
        result = makeRValue(types_.builtin(TypeKind::Bool));
        recordOperator(node, operation, operand.type, nullptr, operand.type, result.type, true);
        return record(node, evaluateConstantUnary(node, operand, result));
    }

    if (operation == TokenKind::Tilde)
    {
        if (!isInteger(operand.type))
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "operator ~ requires non-bool integer operand");
            return makeError(node);
        }
        result = makeRValue(promoteInteger(operand.type));
        recordIntegralPromotion(node, node->first(), operand.type, result.type, 0);
        recordOperator(node, operation, operand.type, nullptr, result.type, result.type, true);
        return record(node, evaluateConstantUnary(node, operand, result));
    }

    if (operation == TokenKind::Plus || operation == TokenKind::Minus)
    {
        if (!isNumeric(operand.type))
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "unary numeric operator requires numeric operand");
            return makeError(node);
        }
        result = makeRValue(isInteger(operand.type) ? promoteInteger(operand.type) : operand.type);
        if (isInteger(operand.type))
        {
            recordIntegralPromotion(node, node->first(), operand.type, result.type, 0);
        }
        recordOperator(node, operation, operand.type, nullptr, result.type, result.type, true);
        return record(node, evaluateConstantUnary(node, operand, result));
    }

    return makeError(node);
}

std::optional<ExpressionInfo> SemanticAnalyzer::analyzeStorageUnary(
    ast::AstNode* node,
    const ExpressionInfo& operand,
    TokenKind operation)
{
    ExpressionInfo result;
    if (operation == TokenKind::Amp)
    {
        if (!operand.addressable)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidAddressOf,
                "operand of & must be addressable and non-const");
            return makeError(node);
        }
        result = makeRValue(types_.pointerTo(operand.type));
        recordOperator(node, operation, operand.type, nullptr, operand.type, result.type, true);
        return record(node, result);
    }

    if (operation == TokenKind::Star)
    {
        if (!isPointer(operand.type) || operand.type->elementType()->kind() == TypeKind::Void ||
            !operand.type->elementType()->isComplete())
        {
            // The parser lowers `value->member` to `(*value).member` and marks the
            // synthesized dereference so diagnostics keep naming the operator the
            // programmer wrote. A source-level `*value` remains InvalidDereference.
            const bool writtenAsArrow = node->auxiliaryText() == "arrow";
            report(
                node->range(),
                writtenAsArrow
                    ? diagnostics::DiagnosticId::InvalidMemberAccess
                    : diagnostics::DiagnosticId::InvalidDereference,
                writtenAsArrow
                    ? "operator -> requires pointer to complete struct"
                    : "operand of * must point to a complete non-void type");
            return makeError(node);
        }
        result.type = operand.type->elementType();
        result.category = ValueCategory::LValue;
        result.modifiable = result.type->kind() != TypeKind::Array;
        result.addressable = true;
        recordOperator(node, operation, operand.type, nullptr, operand.type, result.type, true);
        return record(node, result);
    }

    if (operation == TokenKind::PlusPlus || operation == TokenKind::MinusMinus)
    {
        if (!operand.modifiable || !isScalarIncrementType(operand.type))
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidAssignmentTarget,
                "increment/decrement requires a modifiable integer, floating or non-void pointer lvalue");
            return makeError(node);
        }
        result = makeRValue(operand.type);
        recordOperator(node, operation, operand.type, nullptr, operand.type, result.type, true);
        return record(node, result);
    }
    return std::nullopt;
}

ExpressionInfo SemanticAnalyzer::analyzeBinary(ast::AstNode* node)
{
    const TokenKind operation = node->tokenKind();
    ExpressionInfo left = analyzeExpression(node->first());

    const bool shortCircuit = left.isConstant && left.constantValue.kind() == ConstantKind::Boolean &&
        ((operation == TokenKind::AmpAmp && !left.constantValue.boolean()) ||
         (operation == TokenKind::PipePipe && left.constantValue.boolean()));

    if (shortCircuit)
    {
        ++constantTrapSuppressionDepth_;
    }
    ExpressionInfo right = analyzeExpression(node->second());
    if (shortCircuit)
    {
        --constantTrapSuppressionDepth_;
    }

    if (left.type->kind() == TypeKind::Error || right.type->kind() == TypeKind::Error)
    {
        return makeError(node);
    }

    ExpressionInfo result;

    if (const auto result = analyzeLogicalBinary(node, left, right, operation, shortCircuit))
    {
        return *result;
    }
    if (const auto result = analyzeEqualityBinary(node, left, right, operation, shortCircuit))
    {
        return *result;
    }
    if (const auto result = analyzeRelationalBinary(node, left, right, operation, shortCircuit))
    {
        return *result;
    }
    if (const auto result = analyzeArithmeticBinary(node, left, right, operation, shortCircuit))
    {
        return *result;
    }

    return makeError(node);
}

std::optional<ExpressionInfo> SemanticAnalyzer::analyzeLogicalBinary(
    ast::AstNode* node,
    const ExpressionInfo& left,
    const ExpressionInfo& right,
    TokenKind operation,
    bool shortCircuit)
{
    ExpressionInfo result;
    if (operation == TokenKind::AmpAmp || operation == TokenKind::PipePipe)
    {
        if (left.type->kind() != TypeKind::Bool || right.type->kind() != TypeKind::Bool)
        {
            report(node->range(), diagnostics::DiagnosticId::InvalidOperand, "logical operators require bool operands");
            return makeError(node);
        }
        result = makeRValue(types_.builtin(TypeKind::Bool));
        recordOperator(node, operation, left.type, right.type, types_.builtin(TypeKind::Bool), result.type, false);
        if (shortCircuit)
        {
            result.isConstant = true;
            result.constantValue = ConstantValue::boolean(operation == TokenKind::PipePipe);
            return record(node, result);
        }
        return record(node, evaluateConstantBinary(node, left, right, result));
    }
    return std::nullopt;
}

std::optional<ExpressionInfo> SemanticAnalyzer::analyzeEqualityBinary(
    ast::AstNode* node,
    const ExpressionInfo& left,
    const ExpressionInfo& right,
    TokenKind operation,
    bool shortCircuit)
{
    (void)shortCircuit;
    ExpressionInfo result;
    if (operation == TokenKind::EqualEqual || operation == TokenKind::BangEqual)
    {
        bool valid = false;
        if (isNumeric(left.type) && isNumeric(right.type) && commonNumericType(left.type, right.type) != nullptr)
        {
            valid = true;
        }
        else if (left.type->kind() == TypeKind::Bool && right.type->kind() == TypeKind::Bool)
        {
            valid = true;
        }
        else if (left.type->kind() == TypeKind::Enum && left.type == right.type)
        {
            valid = true;
        }
        else if ((isPointer(left.type) && isPointer(right.type) &&
                  (left.type == right.type || left.type->elementType()->kind() == TypeKind::Void ||
                   right.type->elementType()->kind() == TypeKind::Void)) ||
                 (isPointer(left.type) && right.type->kind() == TypeKind::NullLiteral) ||
                 (isPointer(right.type) && left.type->kind() == TypeKind::NullLiteral))
        {
            valid = true;
        }

        if (!valid)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "operands are not compatible for equality comparison");
            return makeError(node);
        }
        result = makeRValue(types_.builtin(TypeKind::Bool));
        Type* operationType = left.type;
        if (isNumeric(left.type) && isNumeric(right.type))
        {
            operationType = commonNumericType(left.type, right.type);
            recordImplicitConversion(node, node->first(), left, operationType, 0);
            recordImplicitConversion(node, node->second(), right, operationType, 1);
        }
        else if (isPointer(left.type) && right.type->kind() == TypeKind::NullLiteral)
        {
            recordImplicitConversion(node, node->second(), right, left.type, 1);
        }
        else if (isPointer(right.type) && left.type->kind() == TypeKind::NullLiteral)
        {
            recordImplicitConversion(node, node->first(), left, right.type, 0);
            operationType = right.type;
        }
        else if (isPointer(left.type) && isPointer(right.type) && left.type != right.type)
        {
            Type* voidPointer = left.type->elementType()->kind() == TypeKind::Void ? left.type : right.type;
            if (left.type != voidPointer)
            {
                recordImplicitConversion(node, node->first(), left, voidPointer, 0);
            }
            if (right.type != voidPointer)
            {
                recordImplicitConversion(node, node->second(), right, voidPointer, 1);
            }
            operationType = voidPointer;
        }
        recordOperator(node, operation, left.type, right.type, operationType, result.type, false);
        return record(node, evaluateConstantBinary(node, left, right, result));
    }
    return std::nullopt;
}

std::optional<ExpressionInfo> SemanticAnalyzer::analyzeRelationalBinary(
    ast::AstNode* node,
    const ExpressionInfo& left,
    const ExpressionInfo& right,
    TokenKind operation,
    bool shortCircuit)
{
    (void)shortCircuit;
    ExpressionInfo result;
    if (operation == TokenKind::Less || operation == TokenKind::LessEqual ||
        operation == TokenKind::Greater || operation == TokenKind::GreaterEqual)
    {
        if (!isNumeric(left.type) || !isNumeric(right.type) || commonNumericType(left.type, right.type) == nullptr)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "relational operators require compatible numeric operands");
            return makeError(node);
        }
        result = makeRValue(types_.builtin(TypeKind::Bool));
        Type* common = commonNumericType(left.type, right.type);
        recordImplicitConversion(node, node->first(), left, common, 0);
        recordImplicitConversion(node, node->second(), right, common, 1);
        recordOperator(node, operation, left.type, right.type, common, result.type, false);
        return record(node, evaluateConstantBinary(node, left, right, result));
    }

    if (operation == TokenKind::Amp || operation == TokenKind::Pipe || operation == TokenKind::Caret)
    {
        Type* common = commonIntegralType(left.type, right.type);
        if (common == nullptr)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "bitwise operators require compatible integer operands");
            return makeError(node);
        }
        result = makeRValue(common);
        recordImplicitConversion(node, node->first(), left, common, 0);
        recordImplicitConversion(node, node->second(), right, common, 1);
        recordOperator(node, operation, left.type, right.type, common, result.type, false);
        return record(node, evaluateConstantBinary(node, left, right, result));
    }

    if (operation == TokenKind::LessLess || operation == TokenKind::GreaterGreater)
    {
        if (!isInteger(left.type) || !isInteger(right.type))
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "shift operators require integer operands");
            return makeError(node);
        }
        result = makeRValue(promoteInteger(left.type));
        recordIntegralPromotion(node, node->first(), left.type, result.type, 0);
        recordOperator(node, operation, left.type, right.type, result.type, result.type, false);
        return record(node, evaluateConstantBinary(node, left, right, result));
    }
    return std::nullopt;
}

std::optional<ExpressionInfo> SemanticAnalyzer::analyzeArithmeticBinary(
    ast::AstNode* node,
    const ExpressionInfo& left,
    const ExpressionInfo& right,
    TokenKind operation,
    bool shortCircuit)
{
    (void)shortCircuit;
    ExpressionInfo result;
    if (operation == TokenKind::Plus || operation == TokenKind::Minus)
    {
        if (isPointer(left.type) && isInteger(right.type) && left.type->elementType()->kind() != TypeKind::Void &&
            left.type->elementType()->isComplete())
        {
            recordOperator(node, operation, left.type, right.type, left.type, left.type, false);
            return record(node, makeRValue(left.type));
        }
        if (operation == TokenKind::Plus && isInteger(left.type) && isPointer(right.type) &&
            right.type->elementType()->kind() != TypeKind::Void && right.type->elementType()->isComplete())
        {
            recordOperator(node, operation, left.type, right.type, right.type, right.type, false);
            return record(node, makeRValue(right.type));
        }
        if (operation == TokenKind::Minus && isPointer(left.type) && isPointer(right.type) && left.type == right.type &&
            left.type->elementType()->kind() != TypeKind::Void)
        {
            Type* resultType = types_.builtin(TypeKind::NInt);
            recordOperator(node, operation, left.type, right.type, left.type, resultType, false);
            return record(node, makeRValue(resultType));
        }
    }

    if (operation == TokenKind::Percent)
    {
        Type* common = commonIntegralType(left.type, right.type);
        if (common == nullptr)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "operator % requires compatible integer operands");
            return makeError(node);
        }
        result = makeRValue(common);
        recordImplicitConversion(node, node->first(), left, common, 0);
        recordImplicitConversion(node, node->second(), right, common, 1);
        recordOperator(node, operation, left.type, right.type, common, result.type, false);
        return record(node, evaluateConstantBinary(node, left, right, result));
    }

    if (operation == TokenKind::Plus || operation == TokenKind::Minus || operation == TokenKind::Star ||
        operation == TokenKind::Slash)
    {
        Type* common = commonNumericType(left.type, right.type);
        if (common == nullptr)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidOperand,
                "arithmetic operands do not have a common numeric type");
            return makeError(node);
        }
        result = makeRValue(common);
        recordImplicitConversion(node, node->first(), left, common, 0);
        recordImplicitConversion(node, node->second(), right, common, 1);
        recordOperator(node, operation, left.type, right.type, common, result.type, false);
        return record(node, evaluateConstantBinary(node, left, right, result));
    }
    return std::nullopt;
}

ExpressionInfo SemanticAnalyzer::analyzeConditional(ast::AstNode* node)
{
    ExpressionInfo condition = analyzeExpression(node->first());
    ExpressionInfo whenTrue = analyzeExpression(node->second());
    ExpressionInfo whenFalse = analyzeExpression(node->third());
    if (condition.type->kind() == TypeKind::Error ||
        whenTrue.type->kind() == TypeKind::Error || whenFalse.type->kind() == TypeKind::Error)
    {
        return makeError(node);
    }
    if (condition.type->kind() != TypeKind::Bool)
    {
        report(
            node->first()->range(),
            diagnostics::DiagnosticId::InvalidCondition,
            "conditional expression condition must have type bool");
        return makeError(node);
    }

    Type* resultType = nullptr;
    if (whenTrue.type == whenFalse.type)
    {
        resultType = whenTrue.type;
    }
    else if (isNumeric(whenTrue.type) && isNumeric(whenFalse.type))
    {
        resultType = commonNumericType(whenTrue.type, whenFalse.type);
    }
    else if (canImplicitlyConvert(whenTrue, whenFalse.type))
    {
        resultType = whenFalse.type;
    }
    else if (canImplicitlyConvert(whenFalse, whenTrue.type))
    {
        resultType = whenTrue.type;
    }

    if (resultType == nullptr)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidOperand,
            "conditional expression branches do not have a common type");
        return makeError(node);
    }

    recordImplicitConversion(node, node->second(), whenTrue, resultType, 1);
    recordImplicitConversion(node, node->third(), whenFalse, resultType, 2);
    ExpressionInfo result = makeRValue(resultType);
    if (condition.isConstant && condition.constantValue.kind() == ConstantKind::Boolean)
    {
        const ExpressionInfo& selected = condition.constantValue.boolean() ? whenTrue : whenFalse;
        if (selected.isConstant)
        {
            result.isConstant = true;
            result.constantValue = convertConstant(selected.constantValue, selected.type, resultType);
        }
    }
    return record(node, result);
}

ExpressionInfo SemanticAnalyzer::analyzeAssignment(ast::AstNode* node)
{
    ExpressionInfo left = analyzeExpression(node->first());
    ExpressionInfo right = analyzeExpression(node->second());
    if (!left.modifiable)
    {
        report(
            node->first()->range(),
            diagnostics::DiagnosticId::InvalidAssignmentTarget,
            "left side of assignment must be a modifiable lvalue");
        return makeError(node);
    }

    if (node->tokenKind() == TokenKind::Equal)
    {
        if (!canImplicitlyConvert(right, left.type))
        {
            report(
                node->second()->range(),
                diagnostics::DiagnosticId::InvalidConversion,
                "cannot implicitly convert '" + right.type->displayName() + "' to '" + left.type->displayName() + "'");
            return makeError(node);
        }
        recordImplicitConversion(node, node->second(), right, left.type, 1);
        recordOperator(node, node->tokenKind(), left.type, right.type, left.type, left.type, false);
        return record(node, makeRValue(left.type));
    }

    const TokenKind binaryOperation = underlyingBinaryOperator(node->tokenKind());
    bool valid = false;

    if (isPointer(left.type) && (binaryOperation == TokenKind::Plus || binaryOperation == TokenKind::Minus))
    {
        valid = isInteger(right.type) && left.type->elementType()->kind() != TypeKind::Void &&
            left.type->elementType()->isComplete();
    }
    else if (binaryOperation == TokenKind::Percent || binaryOperation == TokenKind::Amp ||
             binaryOperation == TokenKind::Pipe || binaryOperation == TokenKind::Caret)
    {
        valid = commonIntegralType(left.type, right.type) != nullptr;
    }
    else if (binaryOperation == TokenKind::LessLess || binaryOperation == TokenKind::GreaterGreater)
    {
        valid = isInteger(left.type) && isInteger(right.type);
    }
    else if (binaryOperation == TokenKind::Plus || binaryOperation == TokenKind::Minus ||
             binaryOperation == TokenKind::Star || binaryOperation == TokenKind::Slash)
    {
        valid = commonNumericType(left.type, right.type) != nullptr;
    }

    if (!valid)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidOperand,
            "invalid operand types for compound assignment");
        return makeError(node);
    }

    Type* operationType = left.type;
    if (!isPointer(left.type))
    {
        operationType = (binaryOperation == TokenKind::LessLess || binaryOperation == TokenKind::GreaterGreater)
            ? promoteInteger(left.type)
            : (binaryOperation == TokenKind::Percent || binaryOperation == TokenKind::Amp ||
                       binaryOperation == TokenKind::Pipe || binaryOperation == TokenKind::Caret
                   ? commonIntegralType(left.type, right.type)
                   : commonNumericType(left.type, right.type));
        recordImplicitConversion(node, node->first(), left, operationType, 0);
        if (binaryOperation != TokenKind::LessLess && binaryOperation != TokenKind::GreaterGreater)
        {
            recordImplicitConversion(node, node->second(), right, operationType, 1);
        }
        if (operationType != left.type)
        {
            model_.addConversion(
                node,
                ConversionInfo{
                    classifyExplicitConversion(operationType, left.type),
                    ConversionRole::Result,
                    nullptr,
                    operationType,
                    left.type,
                    -1});
        }
    }
    recordOperator(node, binaryOperation, left.type, right.type, operationType, left.type, false);
    return record(node, makeRValue(left.type));
}

ExpressionInfo SemanticAnalyzer::analyzeCall(ast::AstNode* node)
{
    ExpressionInfo callee = analyzeExpression(node->first(), true);
    if (callee.symbol == nullptr ||
        callee.symbol->kind() != SymbolKind::Function ||
        callee.type->kind() != TypeKind::Function)
    {
        report(node->range(), diagnostics::DiagnosticId::InvalidCall, "call target is not a NativeSharp function");
        return makeError(node);
    }

    const std::vector<Type*>& parameters = callee.type->parameterTypes();
    if (parameters.size() != node->children().size())
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidArgumentCount,
            "function '" + callee.symbol->name() + "' expects " + std::to_string(parameters.size()) +
                " arguments but received " + std::to_string(node->children().size()));
    }

    const std::size_t count = std::min(parameters.size(), node->children().size());
    for (std::size_t index = 0; index < count; ++index)
    {
        ExpressionInfo argument = analyzeExpression(node->children()[index]);
        if (!canImplicitlyConvert(argument, parameters[index]))
        {
            report(
                node->children()[index]->range(),
                diagnostics::DiagnosticId::InvalidConversion,
                "argument " + std::to_string(index + 1) + " cannot convert from '" + argument.type->displayName() +
                    "' to '" + parameters[index]->displayName() + "'");
        }
        else
        {
            recordImplicitConversion(
                node,
                node->children()[index],
                argument,
                parameters[index],
                static_cast<int>(index));
        }
    }

    for (std::size_t index = count; index < node->children().size(); ++index)
    {
        analyzeExpression(node->children()[index]);
    }

    return record(node, makeRValue(callee.type->returnType()));
}

ExpressionInfo SemanticAnalyzer::analyzeIndex(ast::AstNode* node)
{
    ExpressionInfo base = analyzeExpression(node->first());
    std::vector<std::pair<ast::AstNode*, ExpressionInfo>> indices;
    if (!node->children().empty())
    {
        indices.reserve(node->children().size());
        for (ast::AstNode* syntax : node->children())
        {
            indices.emplace_back(syntax, analyzeExpression(syntax));
        }
    }
    else if (node->second() != nullptr)
    {
        indices.emplace_back(node->second(), analyzeExpression(node->second()));
    }
    if (!validateIndexOperands(node, base, indices)) return makeError(node);
    bool fixedArray = false;
    Type* element = resolveIndexedElement(node, base, indices, fixedArray);
    if (element == nullptr) return makeError(node);

    ExpressionInfo result;
    result.type = element;
    result.category = ValueCategory::LValue;
    result.modifiable = base.type->kind() != TypeKind::StringLiteral && element->kind() != TypeKind::Array;
    result.addressable = true;
    result.requiresBoundsCheck = fixedArray;

    return record(node, result);
}

bool SemanticAnalyzer::validateIndexOperands(
    ast::AstNode* node,
    const ExpressionInfo& base,
    const std::vector<std::pair<ast::AstNode*, ExpressionInfo>>& indices)
{
    if (indices.empty())
    {
        report(node->range(), diagnostics::DiagnosticId::InvalidIndex,
            "index expression requires at least one index");
        return false;
    }
    if (base.type->kind() == TypeKind::Error)
    {
        return false;
    }
    for (const auto& [syntax, index] : indices)
    {
        (void)syntax;
        if (index.type->kind() == TypeKind::Error)
        {
            return false;
        }
    }
    for (const auto& [syntax, index] : indices)
    {
        if (isInteger(index.type)) continue;
        report(
            syntax->range(),
            diagnostics::DiagnosticId::InvalidIndex,
            "index expression must have a non-bool integer type");
        return false;
    }
    return true;
}

Type* SemanticAnalyzer::resolveIndexedElement(
    ast::AstNode* node,
    const ExpressionInfo& base,
    const std::vector<std::pair<ast::AstNode*, ExpressionInfo>>& indices,
    bool& fixedArray)
{
    Type* element = nullptr;
    fixedArray = false;
    if (base.type->kind() == TypeKind::Array)
    {
        fixedArray = true;
        Type* dimension = base.type;
        for (const auto& [syntax, index] : indices)
        {
            if (dimension->kind() != TypeKind::Array)
            {
                report(
                    syntax->range(),
                    diagnostics::DiagnosticId::InvalidIndex,
                    "fixed array index count exceeds its rank");
                return nullptr;
            }
            const std::uint64_t count = dimension->count();
            if (index.isConstant)
            {
                const std::optional<std::int64_t> signedValue = constantAsSigned(index);
                const std::optional<std::uint64_t> unsignedValue = constantAsUnsigned(index);
                const bool negative = signedValue.has_value() && *signedValue < 0;
                const bool tooLarge = unsignedValue.has_value() && *unsignedValue >= count;
                if (negative || tooLarge)
                {
                    report(
                        syntax->range(),
                        diagnostics::DiagnosticId::ConstantIndexOutOfBounds,
                        "constant fixed-array index is outside [0, " + std::to_string(count) + ")");
                }
            }
            dimension = dimension->elementType();
        }
        element = dimension;
    }
    else if (base.type->kind() == TypeKind::StringLiteral)
    {
        if (indices.size() != 1)
        {
            report(node->range(), diagnostics::DiagnosticId::InvalidIndex,
                "string indexing requires exactly one integral index");
            return nullptr;
        }
        element = types_.builtin(TypeKind::Char);
        fixedArray = true;
        const ExpressionInfo& index = indices.front().second;
        const std::uint64_t count = base.type->count();
        if (index.isConstant)
        {
            const std::optional<std::int64_t> signedValue = constantAsSigned(index);
            const std::optional<std::uint64_t> unsignedValue = constantAsUnsigned(index);
            const bool negative = signedValue.has_value() && *signedValue < 0;
            const bool tooLarge = unsignedValue.has_value() && *unsignedValue >= count;
            if (negative || tooLarge)
            {
                report(
                    indices.front().first->range(),
                    diagnostics::DiagnosticId::ConstantIndexOutOfBounds,
                    "constant fixed-array index is outside [0, " + std::to_string(count) + ")");
            }
        }
    }
    else if (isPointer(base.type) &&
             base.type->elementType()->kind() != TypeKind::Void &&
             base.type->elementType()->isComplete())
    {
        if (indices.size() != 1)
        {
            report(node->range(), diagnostics::DiagnosticId::InvalidIndex,
                "pointer indexing requires exactly one integral index");
            return nullptr;
        }
        element = base.type->elementType();
    }
    else
    {
        report(
            node->first()->range(),
            diagnostics::DiagnosticId::InvalidIndex,
            "index base must be a fixed array or non-void pointer");
        return nullptr;
    }
    return element;
}

ExpressionInfo SemanticAnalyzer::analyzeMember(ast::AstNode* node, bool pointerForm)
{
    if (!pointerForm && node->first()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        Symbol* qualifier = lookup(node->first()->text());
        if (qualifier != nullptr && qualifier->kind() == SymbolKind::Enum)
        {
            Symbol* member = qualifier->findMember(node->text());
            if (member == nullptr)
            {
                report(
                    node->range(),
                    diagnostics::DiagnosticId::UnknownMember,
                    "enum '" + qualifier->name() + "' has no member '" + node->text() + "'");
                return makeError(node);
            }

            ExpressionInfo result = makeRValue(qualifier->type());
            result.isConstant = true;
            result.constantValue = ConstantValue::signedInteger(member->enumValue());
            result.symbol = member;
            return record(node, result);
        }
    }

    ExpressionInfo base = analyzeExpression(node->first());
    if (base.type->kind() == TypeKind::Error)
    {
        return makeError(node);
    }
    Type* structType = base.type;
    if (pointerForm)
    {
        if (!isPointer(base.type) || base.type->elementType()->kind() != TypeKind::Struct ||
            !base.type->elementType()->isComplete())
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::InvalidMemberAccess,
                "operator -> requires pointer to complete struct");
            return makeError(node);
        }
        structType = base.type->elementType();
    }
    else if (structType->kind() != TypeKind::Struct)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidMemberAccess,
            "operator . requires a struct value or enum type qualifier");
        return makeError(node);
    }

    Symbol* structSymbol = globalScope_->lookupLocal(structType->name());
    Symbol* field = structSymbol == nullptr ? nullptr : structSymbol->findMember(node->text());
    if (field == nullptr)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::UnknownMember,
            "struct '" + structType->name() + "' has no field '" + node->text() + "'");
        return makeError(node);
    }

    ExpressionInfo result;
    result.type = field->type();
    if (pointerForm || base.category == ValueCategory::LValue)
    {
        result.category = ValueCategory::LValue;
        result.modifiable = pointerForm
            ? field->type()->kind() != TypeKind::Array
            : base.modifiable && field->type()->kind() != TypeKind::Array;
        result.addressable = pointerForm ? true : base.addressable;
    }
    else
    {
        result.category = ValueCategory::RValue;
    }
    result.symbol = field;
    return record(node, result);
}

ExpressionInfo SemanticAnalyzer::analyzeCast(ast::AstNode* node)
{
    Type* destination = resolveType(node->typeSyntax());
    ExpressionInfo source = analyzeExpression(node->first());
    if (destination == nullptr)
    {
        return makeError(node);
    }
    if (source.type->kind() == TypeKind::Error)
    {
        return makeError(node);
    }

    if (destination->kind() == TypeKind::Void)
    {
        recordExplicitConversion(node, node->first(), source.type, destination);
        return record(node, makeRValue(destination));
    }

    if (!canExplicitlyConvert(source.type, destination))
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidConversion,
            "explicit conversion from '" + source.type->displayName() + "' to '" + destination->displayName() +
                "' is not allowed");
        return makeError(node);
    }

    recordExplicitConversion(node, node->first(), source.type, destination);
    ExpressionInfo result = makeRValue(destination);
    if (source.isConstant && isFloating(source.type) && isInteger(destination))
    {
        const double value = source.constantValue.floating();
        const long double truncated = std::trunc(static_cast<long double>(value));
        bool outOfRange = !std::isfinite(value);

        if (!outOfRange && isSignedInteger(destination))
        {
            const std::uint32_t width = integerWidth(destination);
            const long double minimum = width == 64
                ? static_cast<long double>(std::numeric_limits<std::int64_t>::min())
                : -static_cast<long double>(std::uint64_t{1} << (width - 1));
            const long double maximum = width == 64
                ? static_cast<long double>(std::numeric_limits<std::int64_t>::max())
                : static_cast<long double>((std::uint64_t{1} << (width - 1)) - 1);
            outOfRange = truncated < minimum || truncated > maximum;
        }
        else if (!outOfRange)
        {
            const long double maximum = static_cast<long double>(maskForWidth(integerWidth(destination)));
            outOfRange = truncated < 0.0L || truncated > maximum;
        }

        if (outOfRange)
        {
            if (constantTrapSuppressionDepth_ == 0)
            {
                report(
                    node->range(),
                    diagnostics::DiagnosticId::ConstantEvaluationTrap,
                    "constant floating-to-integer conversion would trap at runtime");
                return makeError(node);
            }
            return record(node, result);
        }
    }

    if (source.isConstant &&
        (isNumeric(source.type) || isInteger(source.type) || source.type->kind() == TypeKind::Enum) &&
        (isNumeric(destination) || destination->kind() == TypeKind::Enum))
    {
        result.isConstant = true;
        result.constantValue = convertConstant(source.constantValue, source.type, destination);
    }
    else if (source.isConstant && source.type->kind() == TypeKind::NullLiteral && isPointer(destination))
    {
        result.isConstant = true;
        result.constantValue = ConstantValue::nullValue();
    }
    return record(node, result);
}

ExpressionInfo SemanticAnalyzer::analyzeSizeof(ast::AstNode* node, bool typeForm)
{
    Type* targetType = nullptr;
    if (typeForm)
    {
        targetType = resolveType(node->typeSyntax());
    }
    else
    {
        ++constantTrapSuppressionDepth_;
        targetType = analyzeExpression(node->first(), true).type;
        --constantTrapSuppressionDepth_;
    }

    const std::optional<std::uint64_t> size = sizeOf(targetType);
    if (!size.has_value())
    {
        report(node->range(), diagnostics::DiagnosticId::InvalidSizeof, "sizeof requires a complete sized object type");
        return makeError(node);
    }

    ExpressionInfo result = makeRValue(types_.builtin(TypeKind::NUInt));
    result.isConstant = true;
    result.constantValue = ConstantValue::unsignedInteger(*size);
    return record(node, result);
}

ExpressionInfo SemanticAnalyzer::analyzeAlignof(ast::AstNode* node, bool typeForm)
{
    Type* targetType = nullptr;
    if (typeForm)
    {
        targetType = resolveType(node->typeSyntax());
    }
    else
    {
        ++constantTrapSuppressionDepth_;
        targetType = analyzeExpression(node->first(), true).type;
        --constantTrapSuppressionDepth_;
    }

    const std::optional<target::TypeLayout> layout = layoutOfType(targetType);
    if (!layout.has_value())
    {
        report(node->range(), diagnostics::DiagnosticId::InvalidSizeof,
               "alignof requires a complete aligned object type");
        return makeError(node);
    }

    ExpressionInfo result = makeRValue(types_.builtin(TypeKind::NUInt));
    result.isConstant = true;
    result.constantValue = ConstantValue::unsignedInteger(layout->alignment);
    return record(node, result);
}

ExpressionInfo SemanticAnalyzer::analyzeOffsetof(ast::AstNode* node)
{
    Type* targetType = resolveType(node->typeSyntax());
    if (targetType == nullptr || targetType->kind() == TypeKind::Error)
    {
        return makeError(node);
    }
    if (targetType->kind() != TypeKind::Struct || !targetType->isComplete())
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidOffsetof,
            "offsetof requires a complete struct or union type");
        return makeError(node);
    }

    Symbol* typeSymbol = globalScope_->lookupLocal(targetType->name());
    Symbol* field = typeSymbol == nullptr ? nullptr : typeSymbol->findMember(node->text());
    if (field == nullptr || field->kind() != SymbolKind::Field)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidOffsetof,
            "struct '" + targetType->name() + "' has no field '" + node->text() + "'");
        return makeError(node);
    }
    if (!field->hasFieldOffset())
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidOffsetof,
            "field '" + node->text() + "' has no resolved physical offset");
        return makeError(node);
    }

    ExpressionInfo result = makeRValue(types_.builtin(TypeKind::NUInt));
    result.isConstant = true;
    result.constantValue = ConstantValue::unsignedInteger(field->fieldOffset());
    return record(node, result);
}

ExpressionInfo SemanticAnalyzer::analyzeNew(ast::AstNode* node)
{
    Type* elementType = resolveType(node->typeSyntax());
    if (!validateObjectType(elementType, node->range(), false) || elementType->kind() == TypeKind::Array)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidAllocationType,
            "new requires complete sized non-void non-array element type");
        return makeError(node);
    }

    if (node->flag())
    {
        ExpressionInfo count = analyzeExpression(node->first());
        if (!isInteger(count.type))
        {
            report(
                node->first()->range(),
                diagnostics::DiagnosticId::InvalidAllocationType,
                "new[] count must have non-bool integer type");
            return makeError(node);
        }
        if (isNegativeConstant(count) && constantTrapSuppressionDepth_ == 0)
        {
            report(
                node->first()->range(),
                diagnostics::DiagnosticId::ConstantEvaluationTrap,
                "negative new[] count is a defined runtime trap and is rejected when constant");
        }
        else if (count.isConstant)
        {
            const std::optional<std::uint64_t> value = constantAsUnsigned(count);
            const std::optional<target::TypeLayout> elementLayout = layoutOfType(elementType);
            if (value.has_value() && *value != 0 && elementLayout.has_value() &&
                !dataLayout_.layoutArray(*elementLayout, *value).has_value() && constantTrapSuppressionDepth_ == 0)
            {
                report(
                    node->first()->range(),
                    diagnostics::DiagnosticId::ConstantEvaluationTrap,
                    "constant new[] byte-size calculation overflows nuint");
            }
        }
    }

    return record(node, makeRValue(types_.pointerTo(elementType)));
}

ExpressionInfo SemanticAnalyzer::analyzeDelete(ast::AstNode* node)
{
    ExpressionInfo operand = analyzeExpression(node->first());
    if (!isPointer(operand.type) && operand.type->kind() != TypeKind::NullLiteral)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidDeleteOperand,
            "delete/delete[] operand must be pointer or null");
        return makeError(node);
    }
    return record(node, makeRValue(types_.builtin(TypeKind::Void)));
}

ExpressionInfo SemanticAnalyzer::analyzeStackalloc(ast::AstNode* node)
{
    Type* elementType = resolveType(node->typeSyntax());
    if (!validateObjectType(elementType, node->range(), false) || elementType->kind() == TypeKind::Array)
    {
        report(
            node->range(),
            diagnostics::DiagnosticId::InvalidStackalloc,
            "stackalloc requires complete sized non-void non-array element type");
        return makeError(node);
    }

    ExpressionInfo count = analyzeExpression(node->first());
    if (!isInteger(count.type))
    {
        report(
            node->first()->range(),
            diagnostics::DiagnosticId::InvalidStackalloc,
            "stackalloc count must have non-bool integer type");
        return makeError(node);
    }
    if (isNegativeConstant(count) && constantTrapSuppressionDepth_ == 0)
    {
        report(
            node->first()->range(),
            diagnostics::DiagnosticId::ConstantEvaluationTrap,
            "negative stackalloc count is a defined runtime trap and is rejected when constant");
    }
    else if (count.isConstant)
    {
        const std::optional<std::uint64_t> value = constantAsUnsigned(count);
        const std::optional<target::TypeLayout> elementLayout = layoutOfType(elementType);
        if (value.has_value() && *value != 0 && elementLayout.has_value() &&
            !dataLayout_.layoutArray(*elementLayout, *value).has_value() && constantTrapSuppressionDepth_ == 0)
        {
            report(
                node->first()->range(),
                diagnostics::DiagnosticId::ConstantEvaluationTrap,
                "constant stackalloc byte-size calculation overflows nuint");
        }
    }
    return record(node, makeRValue(types_.pointerTo(elementType)));
}


}
