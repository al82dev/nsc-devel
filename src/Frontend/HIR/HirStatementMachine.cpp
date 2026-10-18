#include "HirLoweringSupport.h"

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
enum class StaticAssertConstantKind
{
    Integer,
    Floating,
    Boolean
};

struct StaticAssertConstant final
{
    StaticAssertConstantKind kind = StaticAssertConstantKind::Integer;
    std::string typeKey;
    std::uint64_t integerValue = 0;
    double floatingValue = 0.0;
    bool booleanValue = false;
};

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

std::optional<StaticAssertConstant> evaluateStaticAssertConstant(const ProjectOopHirNode& node)
{
    const auto integer = [&](std::uint64_t value, std::string typeKey)
    {
        StaticAssertConstant result;
        result.kind = StaticAssertConstantKind::Integer;
        result.integerValue = staticAssertNormalize(value, typeKey);
        result.typeKey = std::move(typeKey);
        return std::optional<StaticAssertConstant>(std::move(result));
    };
    const auto floating = [&](double value, std::string typeKey)
    {
        StaticAssertConstant result;
        result.kind = StaticAssertConstantKind::Floating;
        result.floatingValue = value;
        result.typeKey = std::move(typeKey);
        return std::optional<StaticAssertConstant>(std::move(result));
    };
    const auto boolean = [&](bool value)
    {
        StaticAssertConstant result;
        result.kind = StaticAssertConstantKind::Boolean;
        result.booleanValue = value;
        result.typeKey = "builtin:bool";
        return std::optional<StaticAssertConstant>(std::move(result));
    };
    const auto convert = [&](const ProjectOopHirNode& expression,
                             const StaticAssertConstant& value)
        -> std::optional<StaticAssertConstant>
    {
        if (expression.typeKey == "builtin:bool")
            return value.kind == StaticAssertConstantKind::Boolean
                ? boolean(value.booleanValue) : std::nullopt;
        if (expression.typeKey == "builtin:float" || expression.typeKey == "builtin:double")
        {
            if (value.kind == StaticAssertConstantKind::Floating)
                return floating(
                    expression.typeKey == "builtin:float"
                        ? static_cast<double>(static_cast<float>(value.floatingValue))
                        : value.floatingValue,
                    expression.typeKey);
            if (value.kind != StaticAssertConstantKind::Integer) return std::nullopt;
            const double converted = staticAssertSignedInteger(value.typeKey)
                ? static_cast<double>(staticAssertSignedValue(value.integerValue, value.typeKey))
                : static_cast<double>(value.integerValue);
            return floating(
                expression.typeKey == "builtin:float"
                    ? static_cast<double>(static_cast<float>(converted)) : converted,
                expression.typeKey);
        }
        if (!staticAssertIntegerWidth(expression.typeKey).has_value()) return std::nullopt;
        if (value.kind == StaticAssertConstantKind::Integer)
            return integer(value.integerValue, expression.typeKey);
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
            return integer(
                std::bit_cast<std::uint64_t>(static_cast<std::int64_t>(converted)),
                expression.typeKey);
        }
        const unsigned width = *staticAssertIntegerWidth(expression.typeKey);
        const long double maximum = width == 64U
            ? static_cast<long double>(std::numeric_limits<std::uint64_t>::max())
            : static_cast<long double>(staticAssertMask(width));
        if (converted < 0.0L || converted > maximum) return std::nullopt;
        return integer(static_cast<std::uint64_t>(converted), expression.typeKey);
    };
    const auto applyUnary = [&](const ProjectOopHirNode& expression,
                                const StaticAssertConstant& value)
        -> std::optional<StaticAssertConstant>
    {
        switch (expression.operation)
        {
            case lexer::TokenKind::Plus:
                return value;
            case lexer::TokenKind::Minus:
                if (value.kind == StaticAssertConstantKind::Floating)
                    return floating(-value.floatingValue, expression.typeKey);
                if (value.kind == StaticAssertConstantKind::Integer)
                    return integer(std::uint64_t{0} - value.integerValue, expression.typeKey);
                return std::nullopt;
            case lexer::TokenKind::Tilde:
                return value.kind == StaticAssertConstantKind::Integer
                    ? integer(~value.integerValue, expression.typeKey) : std::nullopt;
            case lexer::TokenKind::Bang:
                return value.kind == StaticAssertConstantKind::Boolean
                    ? boolean(!value.booleanValue) : std::nullopt;
            default:
                return std::nullopt;
        }
    };
    const auto applyBinary = [&](const ProjectOopHirNode& expression,
                                 const StaticAssertConstant& left,
                                 const StaticAssertConstant& right)
        -> std::optional<StaticAssertConstant>
    {
        if (expression.operation == lexer::TokenKind::AmpAmp ||
            expression.operation == lexer::TokenKind::PipePipe)
        {
            return right.kind == StaticAssertConstantKind::Boolean
                ? boolean(right.booleanValue) : std::nullopt;
        }
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
            return boolean(expression.operation == lexer::TokenKind::EqualEqual ? equal : !equal);
        }
        if (floatingOperands)
        {
            switch (expression.operation)
            {
                case lexer::TokenKind::Less: return boolean(left.floatingValue < right.floatingValue);
                case lexer::TokenKind::LessEqual: return boolean(left.floatingValue <= right.floatingValue);
                case lexer::TokenKind::Greater: return boolean(left.floatingValue > right.floatingValue);
                case lexer::TokenKind::GreaterEqual: return boolean(left.floatingValue >= right.floatingValue);
                case lexer::TokenKind::Plus: return floating(left.floatingValue + right.floatingValue, expression.typeKey);
                case lexer::TokenKind::Minus: return floating(left.floatingValue - right.floatingValue, expression.typeKey);
                case lexer::TokenKind::Star: return floating(left.floatingValue * right.floatingValue, expression.typeKey);
                case lexer::TokenKind::Slash: return floating(left.floatingValue / right.floatingValue, expression.typeKey);
                case lexer::TokenKind::Percent: return floating(std::fmod(left.floatingValue, right.floatingValue), expression.typeKey);
                default: return std::nullopt;
            }
        }
        if (!integerOperands) return std::nullopt;
        const unsigned width = staticAssertIntegerWidth(left.typeKey).value_or(64U);
        const bool signedOperands = staticAssertSignedInteger(left.typeKey);
        const std::int64_t signedLeft = staticAssertSignedValue(left.integerValue, left.typeKey);
        const std::int64_t signedRight = staticAssertSignedValue(right.integerValue, right.typeKey);
        switch (expression.operation)
        {
            case lexer::TokenKind::Less:
                return boolean(signedOperands ? signedLeft < signedRight
                                              : left.integerValue < right.integerValue);
            case lexer::TokenKind::LessEqual:
                return boolean(signedOperands ? signedLeft <= signedRight
                                              : left.integerValue <= right.integerValue);
            case lexer::TokenKind::Greater:
                return boolean(signedOperands ? signedLeft > signedRight
                                              : left.integerValue > right.integerValue);
            case lexer::TokenKind::GreaterEqual:
                return boolean(signedOperands ? signedLeft >= signedRight
                                              : left.integerValue >= right.integerValue);
            case lexer::TokenKind::Plus:
                return integer(left.integerValue + right.integerValue, expression.typeKey);
            case lexer::TokenKind::Minus:
                return integer(left.integerValue - right.integerValue, expression.typeKey);
            case lexer::TokenKind::Star:
                return integer(left.integerValue * right.integerValue, expression.typeKey);
            case lexer::TokenKind::Amp:
                return integer(left.integerValue & right.integerValue, expression.typeKey);
            case lexer::TokenKind::Pipe:
                return integer(left.integerValue | right.integerValue, expression.typeKey);
            case lexer::TokenKind::Caret:
                return integer(left.integerValue ^ right.integerValue, expression.typeKey);
            case lexer::TokenKind::LessLess:
                if (right.integerValue >= width) return std::nullopt;
                return integer(left.integerValue << right.integerValue, expression.typeKey);
            case lexer::TokenKind::GreaterGreater:
                if (right.integerValue >= width) return std::nullopt;
                return signedOperands
                    ? integer(
                        std::bit_cast<std::uint64_t>(signedLeft >> right.integerValue),
                        expression.typeKey)
                    : integer(left.integerValue >> right.integerValue, expression.typeKey);
            case lexer::TokenKind::Slash:
            case lexer::TokenKind::Percent:
                if (right.integerValue == 0U) return std::nullopt;
                if (!signedOperands)
                    return integer(
                        expression.operation == lexer::TokenKind::Slash
                            ? left.integerValue / right.integerValue
                            : left.integerValue % right.integerValue,
                        expression.typeKey);
                if (signedLeft == std::numeric_limits<std::int64_t>::min() && signedRight == -1)
                    return integer(
                        expression.operation == lexer::TokenKind::Slash
                            ? left.integerValue : std::uint64_t{0},
                        expression.typeKey);
                return integer(
                    std::bit_cast<std::uint64_t>(
                        expression.operation == lexer::TokenKind::Slash
                            ? signedLeft / signedRight : signedLeft % signedRight),
                    expression.typeKey);
            default:
                return std::nullopt;
        }
    };

    struct EvaluationFrame final
    {
        const ProjectOopHirNode* expression = nullptr;
        std::uint8_t stage = 0;
        std::optional<StaticAssertConstant> first;
    };
    std::vector<EvaluationFrame> frames;
    const ProjectOopHirNode* current = &node;
    std::optional<StaticAssertConstant> value;
    while (true)
    {
        switch (current->kind)
        {
            case ProjectOopHirNodeKind::IntegerConstant:
                value = integer(current->integerValue, current->typeKey);
                break;
            case ProjectOopHirNodeKind::FloatingConstant:
                value = floating(current->floatingValue, current->typeKey);
                break;
            case ProjectOopHirNodeKind::BooleanConstant:
                value = boolean(current->booleanValue);
                break;
            case ProjectOopHirNodeKind::Conversion:
            case ProjectOopHirNodeKind::Unary:
                if (current->children.size() != 1U) return std::nullopt;
                frames.push_back(EvaluationFrame{current, 0U, std::nullopt});
                current = current->children[0].get();
                continue;
            case ProjectOopHirNodeKind::Binary:
                if (current->children.size() != 2U) return std::nullopt;
                frames.push_back(EvaluationFrame{current, 0U, std::nullopt});
                current = current->children[0].get();
                continue;
            case ProjectOopHirNodeKind::Conditional:
                if (current->children.size() != 3U) return std::nullopt;
                frames.push_back(EvaluationFrame{current, 0U, std::nullopt});
                current = current->children[0].get();
                continue;
            default:
                return std::nullopt;
        }

        while (true)
        {
            if (!value.has_value()) return std::nullopt;
            if (frames.empty()) return value;
            EvaluationFrame& frame = frames.back();
            if (frame.expression->kind == ProjectOopHirNodeKind::Conversion)
            {
                value = convert(*frame.expression, *value);
                frames.pop_back();
                continue;
            }
            if (frame.expression->kind == ProjectOopHirNodeKind::Unary)
            {
                value = applyUnary(*frame.expression, *value);
                frames.pop_back();
                continue;
            }
            if (frame.expression->kind == ProjectOopHirNodeKind::Conditional)
            {
                if (frame.stage == 0U)
                {
                    if (value->kind != StaticAssertConstantKind::Boolean)
                        return std::nullopt;
                    const std::size_t selected = value->booleanValue ? 1U : 2U;
                    frame.stage = 1U;
                    current = frame.expression->children[selected].get();
                    break;
                }
                frames.pop_back();
                continue;
            }
            if (frame.stage == 0U)
            {
                if (frame.expression->operation == lexer::TokenKind::AmpAmp ||
                    frame.expression->operation == lexer::TokenKind::PipePipe)
                {
                    if (value->kind != StaticAssertConstantKind::Boolean)
                        return std::nullopt;
                    const bool shortCircuits =
                        (frame.expression->operation == lexer::TokenKind::AmpAmp &&
                         !value->booleanValue) ||
                        (frame.expression->operation == lexer::TokenKind::PipePipe &&
                         value->booleanValue);
                    if (shortCircuits)
                    {
                        value = boolean(value->booleanValue);
                        frames.pop_back();
                        continue;
                    }
                }
                frame.first = std::move(value);
                frame.stage = 1U;
                current = frame.expression->children[1].get();
                break;
            }
            value = applyBinary(*frame.expression, *frame.first, *value);
            frames.pop_back();
        }
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
        case ast::AstNodeKind::TryStatement:
            return lowerExceptionOrExitStatement(statement, memberOrdinal, error);
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
