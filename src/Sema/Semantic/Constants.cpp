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

ExpressionInfo SemanticAnalyzer::evaluateConstantUnary(
    ast::AstNode* node,
    const ExpressionInfo& operand,
    ExpressionInfo result)
{
    if (!operand.isConstant)
    {
        return result;
    }

    const TokenKind operation = node->tokenKind();
    if (operation == TokenKind::Bang && operand.constantValue.kind() == ConstantKind::Boolean)
    {
        result.isConstant = true;
        result.constantValue = ConstantValue::boolean(!operand.constantValue.boolean());
        return result;
    }

    if (isInteger(result.type))
    {
        const ConstantValue converted = convertConstant(operand.constantValue, operand.type, result.type);
        const std::uint32_t width = integerWidth(result.type);
        std::uint64_t bits = converted.kind() == ConstantKind::SignedInteger
            ? bitsFromSigned(converted.signedInteger(), width)
            : converted.unsignedInteger();

        if (operation == TokenKind::Minus)
        {
            bits = (~bits + 1) & maskForWidth(width);
        }
        else if (operation == TokenKind::Tilde)
        {
            bits = (~bits) & maskForWidth(width);
        }
        else if (operation != TokenKind::Plus)
        {
            return result;
        }

        result.isConstant = true;
        result.constantValue = isSignedInteger(result.type)
            ? ConstantValue::signedInteger(signedFromBits(bits, width))
            : ConstantValue::unsignedInteger(bits);
        return result;
    }

    if (isFloating(result.type) && operand.constantValue.kind() == ConstantKind::Floating)
    {
        result.isConstant = true;
        result.constantValue = ConstantValue::floating(
            operation == TokenKind::Minus ? -operand.constantValue.floating() : operand.constantValue.floating());
    }
    return result;
}

ExpressionInfo SemanticAnalyzer::evaluateConstantBinary(
    ast::AstNode* node,
    const ExpressionInfo& left,
    const ExpressionInfo& right,
    ExpressionInfo result)
{
    if (!left.isConstant || !right.isConstant)
    {
        return result;
    }

    const TokenKind operation = node->tokenKind();
    if (operation == TokenKind::AmpAmp || operation == TokenKind::PipePipe)
        return evaluateConstantLogical(node, left, right, result);
    if (operation == TokenKind::EqualEqual || operation == TokenKind::BangEqual)
        return evaluateConstantEquality(node, left, right, result);
    if (operation == TokenKind::Less || operation == TokenKind::LessEqual ||
        operation == TokenKind::Greater || operation == TokenKind::GreaterEqual)
        return evaluateConstantRelational(node, left, right, result);
    if (isFloating(result.type))
        return evaluateConstantFloating(node, left, right, result);
    return evaluateConstantInteger(node, left, right, result);
}

ExpressionInfo SemanticAnalyzer::evaluateConstantLogical(
    ast::AstNode* node,
    const ExpressionInfo& left,
    const ExpressionInfo& right,
    ExpressionInfo result)
{
    const TokenKind operation = node->tokenKind();
    if ((operation == TokenKind::AmpAmp || operation == TokenKind::PipePipe) &&
        left.constantValue.kind() == ConstantKind::Boolean && right.constantValue.kind() == ConstantKind::Boolean)
    {
        result.isConstant = true;
        result.constantValue = ConstantValue::boolean(operation == TokenKind::AmpAmp
            ? left.constantValue.boolean() && right.constantValue.boolean()
            : left.constantValue.boolean() || right.constantValue.boolean());
        return result;
    }
    return result;
}

ExpressionInfo SemanticAnalyzer::evaluateConstantEquality(
    ast::AstNode* node,
    const ExpressionInfo& left,
    const ExpressionInfo& right,
    ExpressionInfo result)
{
    const TokenKind operation = node->tokenKind();
    if (operation == TokenKind::EqualEqual || operation == TokenKind::BangEqual)
    {
        bool equal = false;
        bool known = true;
        if (left.constantValue.kind() == ConstantKind::Boolean && right.constantValue.kind() == ConstantKind::Boolean)
        {
            equal = left.constantValue.boolean() == right.constantValue.boolean();
        }
        else if (left.constantValue.kind() == ConstantKind::Null && right.constantValue.kind() == ConstantKind::Null)
        {
            equal = true;
        }
        else if (isNumeric(left.type) && isNumeric(right.type))
        {
            Type* common = commonNumericType(left.type, right.type);
            if (common == nullptr)
            {
                known = false;
            }
            else if (isFloating(common))
            {
                const ConstantValue leftValue = convertConstant(left.constantValue, left.type, common);
                const ConstantValue rightValue = convertConstant(right.constantValue, right.type, common);
                equal = leftValue.floating() == rightValue.floating();
            }
            else
            {
                const ConstantValue leftValue = convertConstant(left.constantValue, left.type, common);
                const ConstantValue rightValue = convertConstant(right.constantValue, right.type, common);
                equal = isSignedInteger(common)
                    ? leftValue.signedInteger() == rightValue.signedInteger()
                    : leftValue.unsignedInteger() == rightValue.unsignedInteger();
            }
        }
        else if (left.type->kind() == TypeKind::Enum && left.type == right.type)
        {
            equal = left.constantValue.signedInteger() == right.constantValue.signedInteger();
        }
        else
        {
            known = false;
        }

        if (known)
        {
            result.isConstant = true;
            result.constantValue = ConstantValue::boolean(operation == TokenKind::EqualEqual ? equal : !equal);
        }
        return result;
    }
    return result;
}

ExpressionInfo SemanticAnalyzer::evaluateConstantRelational(
    ast::AstNode* node,
    const ExpressionInfo& left,
    const ExpressionInfo& right,
    ExpressionInfo result)
{
    const TokenKind operation = node->tokenKind();
    if (operation == TokenKind::Less || operation == TokenKind::LessEqual ||
        operation == TokenKind::Greater || operation == TokenKind::GreaterEqual)
    {
        Type* common = commonNumericType(left.type, right.type);
        if (common == nullptr)
        {
            return result;
        }

        bool comparison = false;
        if (isFloating(common))
        {
            const double a = convertConstant(left.constantValue, left.type, common).floating();
            const double b = convertConstant(right.constantValue, right.type, common).floating();
            if (operation == TokenKind::Less) comparison = a < b;
            if (operation == TokenKind::LessEqual) comparison = a <= b;
            if (operation == TokenKind::Greater) comparison = a > b;
            if (operation == TokenKind::GreaterEqual) comparison = a >= b;
        }
        else if (isSignedInteger(common))
        {
            const std::int64_t a = convertConstant(left.constantValue, left.type, common).signedInteger();
            const std::int64_t b = convertConstant(right.constantValue, right.type, common).signedInteger();
            if (operation == TokenKind::Less) comparison = a < b;
            if (operation == TokenKind::LessEqual) comparison = a <= b;
            if (operation == TokenKind::Greater) comparison = a > b;
            if (operation == TokenKind::GreaterEqual) comparison = a >= b;
        }
        else
        {
            const std::uint64_t a = convertConstant(left.constantValue, left.type, common).unsignedInteger();
            const std::uint64_t b = convertConstant(right.constantValue, right.type, common).unsignedInteger();
            if (operation == TokenKind::Less) comparison = a < b;
            if (operation == TokenKind::LessEqual) comparison = a <= b;
            if (operation == TokenKind::Greater) comparison = a > b;
            if (operation == TokenKind::GreaterEqual) comparison = a >= b;
        }

        result.isConstant = true;
        result.constantValue = ConstantValue::boolean(comparison);
        return result;
    }
    return result;
}

ExpressionInfo SemanticAnalyzer::evaluateConstantFloating(
    ast::AstNode* node,
    const ExpressionInfo& left,
    const ExpressionInfo& right,
    ExpressionInfo result)
{
    const TokenKind operation = node->tokenKind();
    if (isFloating(result.type))
    {
        Type* common = commonNumericType(left.type, right.type);
        if (common == nullptr || !isFloating(common))
        {
            return result;
        }
        const double a = convertConstant(left.constantValue, left.type, common).floating();
        const double b = convertConstant(right.constantValue, right.type, common).floating();
        double value = 0.0;
        if (operation == TokenKind::Plus) value = a + b;
        else if (operation == TokenKind::Minus) value = a - b;
        else if (operation == TokenKind::Star) value = a * b;
        else if (operation == TokenKind::Slash) value = a / b;
        else return result;
        result.isConstant = true;
        result.constantValue = ConstantValue::floating(
            result.type->kind() == TypeKind::Float
                ? static_cast<double>(static_cast<float>(value))
                : value);
        return result;
    }
    return result;
}

ExpressionInfo SemanticAnalyzer::evaluateConstantInteger(
    ast::AstNode* node,
    const ExpressionInfo& left,
    const ExpressionInfo& right,
    ExpressionInfo result)
{
    const TokenKind operation = node->tokenKind();
    Type* common = result.type;
    if (!isInteger(common))
    {
        common = commonIntegralType(left.type, right.type);
    }
    if (common == nullptr || !isInteger(common))
    {
        return result;
    }

    const std::uint32_t width = integerWidth(common);
    const ConstantValue aValue = convertConstant(left.constantValue, left.type, common);
    const ConstantValue bValue = convertConstant(right.constantValue, right.type, common);
    std::uint64_t a = isSignedInteger(common)
        ? bitsFromSigned(aValue.signedInteger(), width)
        : aValue.unsignedInteger();
    std::uint64_t b = isSignedInteger(common)
        ? bitsFromSigned(bValue.signedInteger(), width)
        : bValue.unsignedInteger();
    a &= maskForWidth(width);
    b &= maskForWidth(width);

    std::uint64_t bits = 0;
    if (operation == TokenKind::Plus) bits = a + b;
    else if (operation == TokenKind::Minus) bits = a - b;
    else if (operation == TokenKind::Star) bits = a * b;
    else if (operation == TokenKind::Amp) bits = a & b;
    else if (operation == TokenKind::Pipe) bits = a | b;
    else if (operation == TokenKind::Caret) bits = a ^ b;
    else if (operation == TokenKind::LessLess || operation == TokenKind::GreaterGreater)
    {
        return evaluateConstantShift(node, right, common, width, a, result);
    }
    else if (operation == TokenKind::Slash || operation == TokenKind::Percent)
    {
        return evaluateConstantDivision(node, common, width, a, b, result);
    }
    else
    {
        return result;
    }

    return finishConstantInteger(common, width, bits, result);
}

ExpressionInfo SemanticAnalyzer::evaluateConstantShift(
    ast::AstNode* node,
    const ExpressionInfo& right,
    Type* common,
    std::uint32_t width,
    std::uint64_t bits,
    ExpressionInfo result)
{
    const std::optional<std::int64_t> shift = constantAsSigned(right);
    if (!shift.has_value() || *shift < 0 || static_cast<std::uint64_t>(*shift) >= width)
    {
        if (constantTrapSuppressionDepth_ == 0)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::ConstantEvaluationTrap,
                "constant shift count is outside the valid range for the promoted left operand");
        }
        return result;
    }
    if (node->tokenKind() == TokenKind::LessLess)
    {
        bits <<= static_cast<unsigned>(*shift);
    }
    else if (isSignedInteger(common))
    {
        const std::int64_t signedBits = signedFromBits(bits, width);
        bits = bitsFromSigned(signedBits >> static_cast<unsigned>(*shift), width);
    }
    else
    {
        bits >>= static_cast<unsigned>(*shift);
    }
    return finishConstantInteger(common, width, bits, result);
}

ExpressionInfo SemanticAnalyzer::evaluateConstantDivision(
    ast::AstNode* node,
    Type* common,
    std::uint32_t width,
    std::uint64_t left,
    std::uint64_t right,
    ExpressionInfo result)
{
    if (right == 0)
    {
        if (constantTrapSuppressionDepth_ == 0)
        {
            report(
                node->range(),
                diagnostics::DiagnosticId::ConstantEvaluationTrap,
                "division or remainder by zero is a defined runtime trap and is rejected in a constant expression");
        }
        return result;
    }

    std::uint64_t bits = 0;
    if (isSignedInteger(common))
    {
        const std::int64_t signedLeft = signedFromBits(left, width);
        const std::int64_t signedRight = signedFromBits(right, width);
        const std::int64_t minimum = width == 64
            ? std::numeric_limits<std::int64_t>::min()
            : -(std::int64_t{1} << (width - 1));
        if (signedLeft == minimum && signedRight == -1)
        {
            if (constantTrapSuppressionDepth_ == 0)
            {
                report(
                    node->range(),
                    diagnostics::DiagnosticId::ConstantEvaluationTrap,
                    "signed MIN / -1 is a defined runtime trap and is rejected in a constant expression");
            }
            return result;
        }
        const std::int64_t computed = node->tokenKind() == TokenKind::Slash
            ? signedLeft / signedRight
            : signedLeft % signedRight;
        bits = bitsFromSigned(computed, width);
    }
    else
    {
        bits = node->tokenKind() == TokenKind::Slash ? left / right : left % right;
    }
    return finishConstantInteger(common, width, bits, result);
}

ExpressionInfo SemanticAnalyzer::finishConstantInteger(
    Type* common,
    std::uint32_t width,
    std::uint64_t bits,
    ExpressionInfo result) const
{
    bits &= maskForWidth(width);
    result.isConstant = true;
    result.constantValue = isSignedInteger(common)
        ? ConstantValue::signedInteger(signedFromBits(bits, width))
        : ConstantValue::unsignedInteger(bits);
    return result;
}

bool SemanticAnalyzer::statementPreventsFallthrough(ast::AstNode* node) const
{
    if (node == nullptr)
    {
        return false;
    }

    switch (node->kind())
    {
        case ast::AstNodeKind::ReturnStatement:
        case ast::AstNodeKind::BreakStatement:
        case ast::AstNodeKind::ContinueStatement:
            return true;

        case ast::AstNodeKind::BlockStatement:
            for (ast::AstNode* child : node->children())
            {
                if (statementPreventsFallthrough(child))
                {
                    return true;
                }
            }
            return false;

        case ast::AstNodeKind::IfStatement:
        {
            const std::optional<bool> condition = constantBoolean(node->first());
            if (condition.has_value())
            {
                if (*condition)
                {
                    return statementPreventsFallthrough(node->second());
                }
                return node->third() != nullptr && statementPreventsFallthrough(node->third());
            }

            return node->second() != nullptr && node->third() != nullptr &&
                statementPreventsFallthrough(node->second()) && statementPreventsFallthrough(node->third());
        }

        case ast::AstNodeKind::WhileStatement:
        {
            const std::optional<bool> condition = constantBoolean(node->first());
            return condition.has_value() && *condition && !containsBreakTargetingLoop(node->second());
        }

        case ast::AstNodeKind::ForStatement:
        {
            bool alwaysTrue = node->second() == nullptr;
            if (node->second() != nullptr)
            {
                const std::optional<bool> condition = constantBoolean(node->second());
                alwaysTrue = condition.has_value() && *condition;
            }

            ast::AstNode* body = node->children().empty() ? nullptr : node->children().front();
            return alwaysTrue && !containsBreakTargetingLoop(body);
        }

        default:
            return false;
    }
}

bool SemanticAnalyzer::containsBreakTargetingLoop(ast::AstNode* node) const
{
    if (node == nullptr)
    {
        return false;
    }

    if (node->kind() == ast::AstNodeKind::BreakStatement)
    {
        return true;
    }

    if (node->kind() == ast::AstNodeKind::WhileStatement || node->kind() == ast::AstNodeKind::ForStatement)
    {
        return false;
    }

    if (node->kind() == ast::AstNodeKind::BlockStatement)
    {
        for (ast::AstNode* child : node->children())
        {
            if (containsBreakTargetingLoop(child))
            {
                return true;
            }
            if (statementPreventsFallthrough(child))
            {
                return false;
            }
        }
        return false;
    }

    if (node->kind() == ast::AstNodeKind::IfStatement)
    {
        const std::optional<bool> condition = constantBoolean(node->first());
        if (condition.has_value())
        {
            return *condition
                ? containsBreakTargetingLoop(node->second())
                : containsBreakTargetingLoop(node->third());
        }

        return containsBreakTargetingLoop(node->second()) || containsBreakTargetingLoop(node->third());
    }

    return false;
}

std::optional<bool> SemanticAnalyzer::constantBoolean(ast::AstNode* node) const noexcept
{
    const ExpressionInfo* info = model_.get(node);
    if (info == nullptr || !info->isConstant || info->constantValue.kind() != ConstantKind::Boolean)
    {
        return std::nullopt;
    }
    return info->constantValue.boolean();
}

void SemanticAnalyzer::report(
    SourceRange range,
    diagnostics::DiagnosticId id,
    std::string message)
{
    diagnostics_.report(id, diagnostics::DiagnosticSeverity::Error, range, std::move(message));
}

void SemanticAnalyzer::reportPreviousDeclaration(const Symbol* symbol)
{
    if (symbol == nullptr || symbol->declaration() == nullptr)
    {
        return;
    }

    diagnostics_.report(
        diagnostics::DiagnosticId::PreviousDeclaration,
        diagnostics::DiagnosticSeverity::Note,
        symbol->declaration()->range(),
        "previous declaration is here");
}

}
