#include "HirLoweringSupport.h"
#include "Internal/StaticAssertConstant.h"
#include "Internal/StaticAssertEvaluationFrame.h"

#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectMetaprogrammingModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
using internal::StaticAssertConstant;
using internal::StaticAssertConstantKind;
using internal::StaticAssertEvaluationFrame;

std::optional<unsigned> staticAssertIntegerWidth(std::string_view typeKey) noexcept
{
    if (typeKey == "builtin:sbyte" || typeKey == "builtin:byte") return 8U;
    if (typeKey == "builtin:char" || typeKey == "builtin:short" || typeKey == "builtin:ushort") return 16U;
    if (typeKey == "builtin:int" || typeKey == "builtin:uint") return 32U;
    if (typeKey == "builtin:long" || typeKey == "builtin:ulong" ||
        typeKey == "builtin:nint" || typeKey == "builtin:nuint") return 64U;
    if (typeKey == "builtin:i128" || typeKey == "builtin:u128") return 128U;
    return std::nullopt;
}

bool staticAssertSignedInteger(std::string_view typeKey) noexcept
{
    return typeKey == "builtin:sbyte" || typeKey == "builtin:short" ||
        typeKey == "builtin:int" || typeKey == "builtin:long" || typeKey == "builtin:nint" ||
        typeKey == "builtin:i128";
}

std::uint64_t staticAssertMask(unsigned width) noexcept
{
    return width == 64U ? ~std::uint64_t{0} : ((std::uint64_t{1} << width) - 1U);
}

std::uint64_t staticAssertNormalize(std::uint64_t value, std::string_view typeKey) noexcept
{
    const std::optional<unsigned> width = staticAssertIntegerWidth(typeKey);
    return width.has_value() ? value & staticAssertMask(*width) : value;
}

std::int64_t staticAssertSignedValue(std::uint64_t value, std::string_view typeKey) noexcept
{
    const unsigned width = staticAssertIntegerWidth(typeKey).value_or(64U);
    value &= staticAssertMask(width);
    if (width == 64U) return std::bit_cast<std::int64_t>(value);
    const std::uint64_t sign = std::uint64_t{1} << (width - 1U);
    if ((value & sign) != 0U) value |= ~staticAssertMask(width);
    return std::bit_cast<std::int64_t>(value);
}

std::optional<StaticAssertConstant> makeStaticAssertInteger(
    std::uint64_t value,
    std::string typeKey)
{
    StaticAssertConstant result;
    result.kind = StaticAssertConstantKind::Integer;
    result.integerValue = staticAssertNormalize(value, typeKey);
    result.typeKey = std::move(typeKey);
    return std::optional<StaticAssertConstant>(std::move(result));
}

std::optional<StaticAssertConstant> makeStaticAssertFloating(
    double value,
    std::string typeKey)
{
    StaticAssertConstant result;
    result.kind = StaticAssertConstantKind::Floating;
    result.floatingValue = value;
    result.typeKey = std::move(typeKey);
    return std::optional<StaticAssertConstant>(std::move(result));
}

std::optional<StaticAssertConstant> makeStaticAssertBoolean(bool value)
{
    StaticAssertConstant result;
    result.kind = StaticAssertConstantKind::Boolean;
    result.booleanValue = value;
    result.typeKey = "builtin:bool";
    return std::optional<StaticAssertConstant>(std::move(result));
}

std::optional<StaticAssertConstant> convertStaticAssertConstant(
    const ProjectOopHirNode& expression,
    const StaticAssertConstant& value)
{
    if (expression.typeKey == "builtin:bool")
        return value.kind == StaticAssertConstantKind::Boolean
            ? makeStaticAssertBoolean(value.booleanValue) : std::nullopt;
    if (expression.typeKey == "builtin:float" || expression.typeKey == "builtin:double")
    {
        if (value.kind == StaticAssertConstantKind::Floating)
            return makeStaticAssertFloating(
                expression.typeKey == "builtin:float"
                    ? static_cast<double>(static_cast<float>(value.floatingValue))
                    : value.floatingValue,
                expression.typeKey);
        if (value.kind != StaticAssertConstantKind::Integer) return std::nullopt;
        const double converted = staticAssertSignedInteger(value.typeKey)
            ? static_cast<double>(staticAssertSignedValue(value.integerValue, value.typeKey))
            : static_cast<double>(value.integerValue);
        return makeStaticAssertFloating(
            expression.typeKey == "builtin:float"
                ? static_cast<double>(static_cast<float>(converted)) : converted,
            expression.typeKey);
    }
    if (!staticAssertIntegerWidth(expression.typeKey).has_value()) return std::nullopt;
    if (value.kind == StaticAssertConstantKind::Integer)
        return makeStaticAssertInteger(value.integerValue, expression.typeKey);
    if (value.kind != StaticAssertConstantKind::Floating ||
        !std::isfinite(value.floatingValue)) return std::nullopt;
    const long double converted = std::trunc(static_cast<long double>(value.floatingValue));
    if (staticAssertSignedInteger(expression.typeKey))
    {
        const unsigned width = *staticAssertIntegerWidth(expression.typeKey);
        const long double minimum = width == 64U
            ? static_cast<long double>(std::numeric_limits<std::int64_t>::min())
            : -static_cast<long double>(std::uint64_t{1} << (width - 1U));
        const long double maximum = width == 64U
            ? static_cast<long double>(std::numeric_limits<std::int64_t>::max())
            : static_cast<long double>((std::uint64_t{1} << (width - 1U)) - 1U);
        if (converted < minimum || converted > maximum) return std::nullopt;
        return makeStaticAssertInteger(
            std::bit_cast<std::uint64_t>(static_cast<std::int64_t>(converted)),
            expression.typeKey);
    }
    const unsigned width = *staticAssertIntegerWidth(expression.typeKey);
    const long double maximum = width == 64U
        ? static_cast<long double>(std::numeric_limits<std::uint64_t>::max())
        : static_cast<long double>(staticAssertMask(width));
    if (converted < 0.0L || converted > maximum) return std::nullopt;
    return makeStaticAssertInteger(static_cast<std::uint64_t>(converted), expression.typeKey);
}

std::optional<StaticAssertConstant> applyStaticAssertUnary(
    const ProjectOopHirNode& expression,
    const StaticAssertConstant& value)
{
    switch (expression.operation)
    {
        case lexer::TokenKind::Plus:
            return value;
        case lexer::TokenKind::Minus:
            if (value.kind == StaticAssertConstantKind::Floating)
                return makeStaticAssertFloating(-value.floatingValue, expression.typeKey);
            if (value.kind == StaticAssertConstantKind::Integer)
                return makeStaticAssertInteger(
                    std::uint64_t{0} - value.integerValue,
                    expression.typeKey);
            return std::nullopt;
        case lexer::TokenKind::Tilde:
            return value.kind == StaticAssertConstantKind::Integer
                ? makeStaticAssertInteger(~value.integerValue, expression.typeKey) : std::nullopt;
        case lexer::TokenKind::Bang:
            return value.kind == StaticAssertConstantKind::Boolean
                ? makeStaticAssertBoolean(!value.booleanValue) : std::nullopt;
        default:
            return std::nullopt;
    }
}

std::optional<StaticAssertConstant> applyStaticAssertFloatingBinary(
    const ProjectOopHirNode& expression,
    const StaticAssertConstant& left,
    const StaticAssertConstant& right)
{
    switch (expression.operation)
    {
        case lexer::TokenKind::Less:
            return makeStaticAssertBoolean(left.floatingValue < right.floatingValue);
        case lexer::TokenKind::LessEqual:
            return makeStaticAssertBoolean(left.floatingValue <= right.floatingValue);
        case lexer::TokenKind::Greater:
            return makeStaticAssertBoolean(left.floatingValue > right.floatingValue);
        case lexer::TokenKind::GreaterEqual:
            return makeStaticAssertBoolean(left.floatingValue >= right.floatingValue);
        case lexer::TokenKind::Plus:
            return makeStaticAssertFloating(
                left.floatingValue + right.floatingValue,
                expression.typeKey);
        case lexer::TokenKind::Minus:
            return makeStaticAssertFloating(
                left.floatingValue - right.floatingValue,
                expression.typeKey);
        case lexer::TokenKind::Star:
            return makeStaticAssertFloating(
                left.floatingValue * right.floatingValue,
                expression.typeKey);
        case lexer::TokenKind::Slash:
            return makeStaticAssertFloating(
                left.floatingValue / right.floatingValue,
                expression.typeKey);
        case lexer::TokenKind::Percent:
            return makeStaticAssertFloating(
                std::fmod(left.floatingValue, right.floatingValue),
                expression.typeKey);
        default:
            return std::nullopt;
    }
}

std::optional<StaticAssertConstant> applyStaticAssertIntegerComparison(
    const ProjectOopHirNode& expression,
    const StaticAssertConstant& left,
    const StaticAssertConstant& right,
    bool signedOperands,
    std::int64_t signedLeft,
    std::int64_t signedRight)
{
    switch (expression.operation)
    {
        case lexer::TokenKind::Less:
            return makeStaticAssertBoolean(signedOperands ? signedLeft < signedRight
                                                          : left.integerValue < right.integerValue);
        case lexer::TokenKind::LessEqual:
            return makeStaticAssertBoolean(signedOperands ? signedLeft <= signedRight
                                                          : left.integerValue <= right.integerValue);
        case lexer::TokenKind::Greater:
            return makeStaticAssertBoolean(signedOperands ? signedLeft > signedRight
                                                          : left.integerValue > right.integerValue);
        case lexer::TokenKind::GreaterEqual:
            return makeStaticAssertBoolean(signedOperands ? signedLeft >= signedRight
                                                          : left.integerValue >= right.integerValue);
        default:
            return std::nullopt;
    }
}

std::optional<StaticAssertConstant> applyStaticAssertIntegerDivision(
    const ProjectOopHirNode& expression,
    const StaticAssertConstant& left,
    const StaticAssertConstant& right,
    bool signedOperands,
    std::int64_t signedLeft,
    std::int64_t signedRight)
{
    if (right.integerValue == 0U) return std::nullopt;
    if (!signedOperands)
        return makeStaticAssertInteger(
            expression.operation == lexer::TokenKind::Slash
                ? left.integerValue / right.integerValue
                : left.integerValue % right.integerValue,
            expression.typeKey);
    if (signedLeft == std::numeric_limits<std::int64_t>::min() && signedRight == -1)
        return makeStaticAssertInteger(
            expression.operation == lexer::TokenKind::Slash
                ? left.integerValue : std::uint64_t{0},
            expression.typeKey);
    return makeStaticAssertInteger(
        std::bit_cast<std::uint64_t>(
            expression.operation == lexer::TokenKind::Slash
                ? signedLeft / signedRight : signedLeft % signedRight),
        expression.typeKey);
}

std::optional<StaticAssertConstant> applyStaticAssertIntegerBinary(
    const ProjectOopHirNode& expression,
    const StaticAssertConstant& left,
    const StaticAssertConstant& right)
{
    const unsigned width = staticAssertIntegerWidth(left.typeKey).value_or(64U);
    const bool signedOperands = staticAssertSignedInteger(left.typeKey);
    const std::int64_t signedLeft = staticAssertSignedValue(left.integerValue, left.typeKey);
    const std::int64_t signedRight = staticAssertSignedValue(right.integerValue, right.typeKey);
    if (expression.operation == lexer::TokenKind::Less ||
        expression.operation == lexer::TokenKind::LessEqual ||
        expression.operation == lexer::TokenKind::Greater ||
        expression.operation == lexer::TokenKind::GreaterEqual)
        return applyStaticAssertIntegerComparison(
            expression, left, right, signedOperands, signedLeft, signedRight);
    switch (expression.operation)
    {
        case lexer::TokenKind::Plus:
            return makeStaticAssertInteger(left.integerValue + right.integerValue, expression.typeKey);
        case lexer::TokenKind::Minus:
            return makeStaticAssertInteger(left.integerValue - right.integerValue, expression.typeKey);
        case lexer::TokenKind::Star:
            return makeStaticAssertInteger(left.integerValue * right.integerValue, expression.typeKey);
        case lexer::TokenKind::Amp:
            return makeStaticAssertInteger(left.integerValue & right.integerValue, expression.typeKey);
        case lexer::TokenKind::Pipe:
            return makeStaticAssertInteger(left.integerValue | right.integerValue, expression.typeKey);
        case lexer::TokenKind::Caret:
            return makeStaticAssertInteger(left.integerValue ^ right.integerValue, expression.typeKey);
        case lexer::TokenKind::LessLess:
            if (right.integerValue >= width) return std::nullopt;
            return makeStaticAssertInteger(left.integerValue << right.integerValue, expression.typeKey);
        case lexer::TokenKind::GreaterGreater:
            if (right.integerValue >= width) return std::nullopt;
            return signedOperands
                ? makeStaticAssertInteger(
                    std::bit_cast<std::uint64_t>(signedLeft >> right.integerValue),
                    expression.typeKey)
                : makeStaticAssertInteger(
                    left.integerValue >> right.integerValue,
                    expression.typeKey);
        case lexer::TokenKind::Slash:
        case lexer::TokenKind::Percent:
            return applyStaticAssertIntegerDivision(
                expression, left, right, signedOperands, signedLeft, signedRight);
        default:
            return std::nullopt;
    }
}

std::optional<StaticAssertConstant> applyStaticAssertBinary(
    const ProjectOopHirNode& expression,
    const StaticAssertConstant& left,
    const StaticAssertConstant& right)
{
    if (expression.operation == lexer::TokenKind::AmpAmp ||
        expression.operation == lexer::TokenKind::PipePipe)
        return right.kind == StaticAssertConstantKind::Boolean
            ? makeStaticAssertBoolean(right.booleanValue) : std::nullopt;
    const bool floatingOperands = left.kind == StaticAssertConstantKind::Floating &&
        right.kind == StaticAssertConstantKind::Floating;
    const bool integerOperands = left.kind == StaticAssertConstantKind::Integer &&
        right.kind == StaticAssertConstantKind::Integer;
    const bool booleanOperands = left.kind == StaticAssertConstantKind::Boolean &&
        right.kind == StaticAssertConstantKind::Boolean;
    if (expression.operation == lexer::TokenKind::EqualEqual ||
        expression.operation == lexer::TokenKind::BangEqual)
    {
        bool equal = false;
        if (floatingOperands) equal = left.floatingValue == right.floatingValue;
        else if (integerOperands) equal = left.integerValue == right.integerValue;
        else if (booleanOperands) equal = left.booleanValue == right.booleanValue;
        else return std::nullopt;
        return makeStaticAssertBoolean(
            expression.operation == lexer::TokenKind::EqualEqual ? equal : !equal);
    }
    if (floatingOperands) return applyStaticAssertFloatingBinary(expression, left, right);
    return integerOperands
        ? applyStaticAssertIntegerBinary(expression, left, right)
        : std::nullopt;
}

std::optional<bool> beginStaticAssertEvaluation(
    const ProjectOopHirNode*& current,
    std::vector<StaticAssertEvaluationFrame>& frames,
    std::optional<StaticAssertConstant>& value)
{
    switch (current->kind)
    {
        case ProjectOopHirNodeKind::IntegerConstant:
            value = makeStaticAssertInteger(current->integerValue, current->typeKey);
            return true;
        case ProjectOopHirNodeKind::FloatingConstant:
            value = makeStaticAssertFloating(current->floatingValue, current->typeKey);
            return true;
        case ProjectOopHirNodeKind::BooleanConstant:
            value = makeStaticAssertBoolean(current->booleanValue);
            return true;
        case ProjectOopHirNodeKind::Conversion:
        case ProjectOopHirNodeKind::Unary:
            if (current->children.size() != 1U) return std::nullopt;
            frames.push_back(StaticAssertEvaluationFrame{current, 0U, std::nullopt});
            current = current->children[0].get();
            return false;
        case ProjectOopHirNodeKind::Binary:
            if (current->children.size() != 2U) return std::nullopt;
            frames.push_back(StaticAssertEvaluationFrame{current, 0U, std::nullopt});
            current = current->children[0].get();
            return false;
        case ProjectOopHirNodeKind::Conditional:
            if (current->children.size() != 3U) return std::nullopt;
            frames.push_back(StaticAssertEvaluationFrame{current, 0U, std::nullopt});
            current = current->children[0].get();
            return false;
        default:
            return std::nullopt;
    }
}

std::optional<bool> resumeStaticAssertEvaluation(
    const ProjectOopHirNode*& current,
    std::vector<StaticAssertEvaluationFrame>& frames,
    std::optional<StaticAssertConstant>& value)
{
    while (true)
    {
        if (!value.has_value()) return std::nullopt;
        if (frames.empty()) return true;
        StaticAssertEvaluationFrame& frame = frames.back();
        if (frame.expression->kind == ProjectOopHirNodeKind::Conversion)
        {
            value = convertStaticAssertConstant(*frame.expression, *value);
            frames.pop_back();
            continue;
        }
        if (frame.expression->kind == ProjectOopHirNodeKind::Unary)
        {
            value = applyStaticAssertUnary(*frame.expression, *value);
            frames.pop_back();
            continue;
        }
        if (frame.expression->kind == ProjectOopHirNodeKind::Conditional)
        {
            if (frame.stage == 0U)
            {
                if (value->kind != StaticAssertConstantKind::Boolean) return std::nullopt;
                const std::size_t selected = value->booleanValue ? 1U : 2U;
                frame.stage = 1U;
                current = frame.expression->children[selected].get();
                return false;
            }
            frames.pop_back();
            continue;
        }
        if (frame.stage == 0U)
        {
            const bool logical = frame.expression->operation == lexer::TokenKind::AmpAmp ||
                frame.expression->operation == lexer::TokenKind::PipePipe;
            if (logical)
            {
                if (value->kind != StaticAssertConstantKind::Boolean) return std::nullopt;
                const bool shortCircuits =
                    (frame.expression->operation == lexer::TokenKind::AmpAmp &&
                     !value->booleanValue) ||
                    (frame.expression->operation == lexer::TokenKind::PipePipe &&
                     value->booleanValue);
                if (shortCircuits)
                {
                    value = makeStaticAssertBoolean(value->booleanValue);
                    frames.pop_back();
                    continue;
                }
            }
            frame.first = std::move(value);
            frame.stage = 1U;
            current = frame.expression->children[1].get();
            return false;
        }
        value = applyStaticAssertBinary(*frame.expression, *frame.first, *value);
        frames.pop_back();
    }
}

std::optional<StaticAssertConstant> evaluateStaticAssertConstant(const ProjectOopHirNode& node)
{
    std::vector<StaticAssertEvaluationFrame> frames;
    const ProjectOopHirNode* current = &node;
    std::optional<StaticAssertConstant> value;
    while (true)
    {
        const std::optional<bool> began =
            beginStaticAssertEvaluation(current, frames, value);
        if (!began.has_value()) return std::nullopt;
        if (!*began) continue;
        const std::optional<bool> resumed =
            resumeStaticAssertEvaluation(current, frames, value);
        if (!resumed.has_value()) return std::nullopt;
        if (*resumed) return value;
    }
}


}

std::unique_ptr<ProjectOopHirNode>
ProjectOopHirModel::finishStaticAssertStatement(
    const ast::AstNode* statement,
    std::unique_ptr<ProjectOopHirNode> condition,
    std::string& error)
{
    const std::optional<StaticAssertConstant> value =
        evaluateStaticAssertConstant(*condition);
    destroyHirTreeIteratively(condition);
    if (!value.has_value() || value->kind != StaticAssertConstantKind::Boolean)
    {
        error = "static_assert condition is not a compile-time bool constant";
        return nullptr;
    }
    if (!value->booleanValue)
    {
        error = "static assertion failed";
        if (!statement->text().empty()) error += ": " + statement->text();
        return nullptr;
    }
    return makeNode(ProjectOopHirNodeKind::Block, statement, "builtin:void");
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    if (statement == nullptr)
    {
        error = "null statement reached Stage 34 OOP HIR";
        return nullptr;
    }

    switch (statement->kind())
    {
        case ast::AstNodeKind::BlockStatement:
            return lowerBlock(statement, memberOrdinal, error);

        case ast::AstNodeKind::VariableDeclarationStatement:
            return lowerVariableDeclarationStatement(
                statement, memberOrdinal, error);
        case ast::AstNodeKind::DeconstructionDeclarationStatement:
            return lowerDeconstructionDeclarationStatement(
                statement, memberOrdinal, error);

        case ast::AstNodeKind::ExpressionStatement:
            return lowerExpressionStatement(statement, memberOrdinal, error);

        case ast::AstNodeKind::InlineAssemblyStatement:
            return lowerInlineAssemblyStatement(statement, memberOrdinal, error);

        case ast::AstNodeKind::StaticAssertStatement:
            return lowerStaticAssertStatement(statement, memberOrdinal, error);

        case ast::AstNodeKind::IfStatement:
            return lowerIfStatement(statement, memberOrdinal, error);

        case ast::AstNodeKind::DoWhileStatement:
        case ast::AstNodeKind::WhileStatement:
            return lowerLoopStatement(statement, memberOrdinal, error);

        // `switch`. The label EXPRESSIONS are not lowered at all: the body model folded them to
        // integers, checked them against the governing type and rejected duplicates, and left the
        // answer in a MethodSwitchBinding. So this walk copies numbers and lowers statements --
        // there is exactly one place in the compiler that knows how to evaluate a case label, and
        // it is not here.
        case ast::AstNodeKind::SwitchStatement:
            return lowerSwitchStatement(statement, memberOrdinal, error);

        case ast::AstNodeKind::ForStatement:
            return lowerForStatement(statement, memberOrdinal, error);

        case ast::AstNodeKind::ForeachStatement:
            return lowerForeachStatement(statement, memberOrdinal, error);

        case ast::AstNodeKind::BreakStatement:
            if (statement->first() != nullptr)
                return lowerExitValueStatement(statement, memberOrdinal, error);
            return lowerExceptionOrExitStatement(statement, memberOrdinal, error);
        case ast::AstNodeKind::ContinueStatement:
            return lowerExceptionOrExitStatement(statement, memberOrdinal, error);
        case ast::AstNodeKind::SuspendStatement:
        case ast::AstNodeKind::YieldReturnStatement:
        case ast::AstNodeKind::YieldBreakStatement:
            return lowerCoroutineStatement(statement, memberOrdinal, error);
        case ast::AstNodeKind::ReturnStatement:
        case ast::AstNodeKind::ThrowStatement:
            if (statement->first() != nullptr)
                return lowerExitValueStatement(statement, memberOrdinal, error);
            return lowerExceptionOrExitStatement(statement, memberOrdinal, error);
        case ast::AstNodeKind::UsingStatement:
            return lowerUsingStatement(statement, memberOrdinal, error);
        case ast::AstNodeKind::TryStatement:
            return lowerTryStatement(statement, memberOrdinal, error);
        default:
            error = "unsupported Stage 34 OOP statement kind: " +
                std::string(ast::astNodeKindName(statement->kind()));
            return nullptr;
    }
}

// Ver el comentario de la declaracion en ProjectOopHirModel.h: este envoltorio existe para que
// los 125 sitios que ponen un error dentro de lowerExpressionImpl no tengan que adjuntar la
// ubicacion cada uno.

}
