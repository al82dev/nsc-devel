#include "OwnershipArgumentClassification.h"
#include "OperatorClassification.h"
#include "IntegralConstantArithmetic.h"
#include "OwnershipFlowSnapshotQueries.h"

#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "Bodies/MethodBodyInternals.h"
#include "Internal/Stage37ConstantValue.h"

#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Frontend/ProjectCallableResultMerger.h"
#include "nativesharp/Frontend/ProjectCallableWaveReducer.h"
#include "nativesharp/Frontend/ProjectCallableWaveScheduler.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/SumTypeSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
bool isErrorType(std::string_view key) noexcept
{
    return key.empty() || key == "<error>";
}

// La REGLA vive en include/nativesharp/Frontend/IntegerLiteralTyping.h, compartida con el
// evaluador de constantes; aqui solo queda sacarle el texto y el valor al nodo. `negated` es true
// cuando este literal es el operando directo de un menos unario -- ver analyzeUnary.
std::string integerLiteralType(const ast::AstNode* node, bool negated)
{
    return std::string(builtins::integerLiteralTypeKey(node->text(), node->integerValue(), negated));
}

bool isUnsignedIntegerLiteral(const ast::AstNode* node) noexcept
{
    for (const char value : node->text())
    {
        if (value == 'u' || value == 'U')
        {
            return true;
        }
    }
    return false;
}

bool isSignedIntegralType(std::string_view typeKey) noexcept
{
    // ID 89: `sbyte` entra aqui y `byte` no. Es toda la diferencia entre los dos tipos.
    return typeKey == "builtin:sbyte" || typeKey == "builtin:short" || typeKey == "builtin:int" ||
        typeKey == "builtin:long" || typeKey == "builtin:nint" || typeKey == "builtin:i128";
}

unsigned integralBitWidth(std::string_view typeKey) noexcept
{
    if (typeKey == "builtin:sbyte" || typeKey == "builtin:byte" ||
        typeKey == "builtin:char") return 8;
    if (typeKey == "builtin:short" || typeKey == "builtin:ushort") return 16;
    if (typeKey == "builtin:int" || typeKey == "builtin:uint") return 32;
    if (typeKey == "builtin:i128" || typeKey == "builtin:u128") return 128;
    return 64;
}

std::uint64_t integralMask(unsigned width) noexcept
{
    return width >= 64 ? std::numeric_limits<std::uint64_t>::max()
                       : ((std::uint64_t{1} << width) - 1U);
}

std::uint64_t normalizeIntegral(std::uint64_t value, std::string_view typeKey) noexcept
{
    return value & integralMask(integralBitWidth(typeKey));
}

std::optional<std::pair<std::string, std::size_t>> decodeResultCarrierMarker(
    std::string_view marker,
    std::string_view prefix)
{
    if (!marker.starts_with(prefix)) return std::nullopt;
    marker.remove_prefix(prefix.size());
    const std::size_t separator = marker.rfind(':');
    if (separator == std::string_view::npos || separator == 0U ||
        separator + 1U >= marker.size()) return std::nullopt;
    std::size_t tag = 0;
    const char* begin = marker.data() + separator + 1U;
    const char* end = marker.data() + marker.size();
    const auto parsed = std::from_chars(begin, end, tag);
    if (parsed.ec != std::errc{} || parsed.ptr != end) return std::nullopt;
    return std::pair<std::string, std::size_t>{
        std::string(marker.substr(0, separator)), tag};
}

std::int64_t signedIntegral(std::uint64_t bits, std::string_view typeKey) noexcept
{
    const unsigned width = integralBitWidth(typeKey);
    bits &= integralMask(width);
    if (width >= 64) return static_cast<std::int64_t>(bits);
    const std::uint64_t sign = std::uint64_t{1} << (width - 1U);
    if ((bits & sign) == 0) return static_cast<std::int64_t>(bits);
    return static_cast<std::int64_t>(bits | ~integralMask(width));
}

double normalizeFloatingConstant(double value, std::string_view typeKey) noexcept
{
    return typeKey == "builtin:float"
        ? static_cast<double>(static_cast<float>(value))
        : value;
}

std::optional<Stage37ConstantValue> convertStage37Constant(
    Stage37ConstantValue value,
    std::string_view target)
{
    if (target == "builtin:bool")
    {
        if (value.kind != Stage37ConstantKind::Boolean) return std::nullopt;
        value.typeKey = std::string(target);
        return value;
    }
    if (target == "builtin:string")
    {
        if (value.kind != Stage37ConstantKind::String) return std::nullopt;
        value.typeKey = std::string(target);
        return value;
    }
    if (ProjectTypeConversion::isIntegral(target))
    {
        if (value.kind != Stage37ConstantKind::Integer) return std::nullopt;
        value.integer = normalizeIntegral(value.integer, target);
        value.typeKey = std::string(target);
        return value;
    }
    if (target != "builtin:float" && target != "builtin:double") return std::nullopt;
    double converted = 0.0;
    if (value.kind == Stage37ConstantKind::Floating)
    {
        converted = value.floating;
    }
    else if (value.kind == Stage37ConstantKind::Integer)
    {
        converted = isSignedIntegralType(value.typeKey)
            ? static_cast<double>(signedIntegral(value.integer, value.typeKey))
            : static_cast<double>(normalizeIntegral(value.integer, value.typeKey));
    }
    else
    {
        return std::nullopt;
    }
    value.kind = Stage37ConstantKind::Floating;
    value.floating = normalizeFloatingConstant(converted, target);
    value.typeKey = std::string(target);
    return value;
}

std::string stage37ExpressionType(
    const ProjectMethodBodyModel& model,
    const ast::AstNode* syntax,
    std::size_t context)
{
    const MethodExpressionTypeBinding* binding = model.expressionTypeBinding(syntax, context);
    return binding != nullptr ? binding->typeKey : std::string{};
}

std::optional<Stage37ConstantValue> evaluateStage37Constant(
    const ProjectMethodBodyModel& model,
    const ProjectMemberModel* memberModel,
    const ast::AstNode* node,
    std::size_t context,
    std::unordered_set<std::size_t>& evaluatingFields);

std::optional<Stage37ConstantValue> evaluateStage37Identifier(
    const ProjectMethodBodyModel& model,
    const ProjectMemberModel* memberModel,
    const ast::AstNode* node,
    std::size_t context,
    std::unordered_set<std::size_t>& evaluatingFields)
{
    const MethodNameBinding* binding = model.nameBinding(node, context);
    if (binding == nullptr || binding->candidateMemberOrdinals.empty()) return std::nullopt;
    const std::size_t ordinal = binding->candidateMemberOrdinals.front();
    if (ordinal >= memberModel->members().size()) return std::nullopt;
    const ClassMemberSymbol& field = memberModel->members()[ordinal];
    if (field.kind != ClassMemberKind::Field || !field.isConst ||
        field.syntax == nullptr || field.syntax->first() == nullptr)
    {
        return std::nullopt;
    }
    if (!evaluatingFields.insert(ordinal).second) return std::nullopt;
    std::optional<Stage37ConstantValue> value = evaluateStage37Constant(
        model, memberModel, field.syntax->first(), ordinal, evaluatingFields);
    evaluatingFields.erase(ordinal);
    return value.has_value()
        ? convertStage37Constant(*value, field.declaredTypeKey)
        : std::nullopt;
}

std::optional<Stage37ConstantValue> evaluateStage37Unary(
    const ProjectMethodBodyModel& model,
    const ProjectMemberModel* memberModel,
    const ast::AstNode* node,
    std::size_t context,
    std::string resultType,
    std::unordered_set<std::size_t>& evaluatingFields)
{
    const std::optional<Stage37ConstantValue> operand = evaluateStage37Constant(
        model, memberModel, node->first(), context, evaluatingFields);
    if (!operand.has_value()) return std::nullopt;
    if (resultType.empty()) resultType = operand->typeKey;
    if (node->tokenKind() == TokenKind::Bang)
    {
        if (operand->kind != Stage37ConstantKind::Boolean) return std::nullopt;
        return Stage37ConstantValue{
            Stage37ConstantKind::Boolean, "builtin:bool", 0, 0.0, !operand->boolean};
    }
    if (operand->kind == Stage37ConstantKind::Floating)
    {
        if (node->tokenKind() == TokenKind::Plus)
        {
            return convertStage37Constant(*operand, resultType);
        }
        if (node->tokenKind() == TokenKind::Minus)
        {
            Stage37ConstantValue value = *operand;
            value.floating = normalizeFloatingConstant(-value.floating, resultType);
            value.typeKey = resultType;
            return value;
        }
        return std::nullopt;
    }
    if (operand->kind != Stage37ConstantKind::Integer) return std::nullopt;
    std::optional<Stage37ConstantValue> converted =
        convertStage37Constant(*operand, resultType);
    if (!converted.has_value()) return std::nullopt;
    switch (node->tokenKind())
    {
        case TokenKind::Plus: break;
        case TokenKind::Minus:
            converted->integer = normalizeIntegral(
                std::uint64_t{0} - converted->integer, resultType);
            break;
        case TokenKind::Tilde:
            converted->integer = normalizeIntegral(~converted->integer, resultType);
            break;
        default: return std::nullopt;
    }
    return converted;
}

std::optional<Stage37ConstantValue> evaluateStage37Comparison(
    const Stage37ConstantValue& leftRaw,
    const Stage37ConstantValue& rightRaw,
    TokenKind operation)
{
    if (leftRaw.kind == Stage37ConstantKind::Boolean ||
        rightRaw.kind == Stage37ConstantKind::Boolean)
    {
        if (leftRaw.kind != Stage37ConstantKind::Boolean ||
            rightRaw.kind != Stage37ConstantKind::Boolean ||
            (operation != TokenKind::EqualEqual && operation != TokenKind::BangEqual))
        {
            return std::nullopt;
        }
        const bool equal = leftRaw.boolean == rightRaw.boolean;
        return Stage37ConstantValue{
            Stage37ConstantKind::Boolean, "builtin:bool", 0, 0.0,
            operation == TokenKind::EqualEqual ? equal : !equal};
    }
    const std::optional<std::string> common = ProjectTypeConversion::commonNumericType(
        leftRaw.typeKey, rightRaw.typeKey);
    if (!common.has_value()) return std::nullopt;
    const std::optional<Stage37ConstantValue> left =
        convertStage37Constant(leftRaw, *common);
    const std::optional<Stage37ConstantValue> right =
        convertStage37Constant(rightRaw, *common);
    if (!left.has_value() || !right.has_value()) return std::nullopt;
    bool value = false;
    if (left->kind == Stage37ConstantKind::Floating)
    {
        switch (operation)
        {
            case TokenKind::EqualEqual: value = left->floating == right->floating; break;
            case TokenKind::BangEqual: value = left->floating != right->floating; break;
            case TokenKind::Less: value = left->floating < right->floating; break;
            case TokenKind::LessEqual: value = left->floating <= right->floating; break;
            case TokenKind::Greater: value = left->floating > right->floating; break;
            case TokenKind::GreaterEqual: value = left->floating >= right->floating; break;
            default: return std::nullopt;
        }
    }
    else if (isSignedIntegralType(*common))
    {
        const std::int64_t lhs = signedIntegral(left->integer, *common);
        const std::int64_t rhs = signedIntegral(right->integer, *common);
        switch (operation)
        {
            case TokenKind::EqualEqual: value = lhs == rhs; break;
            case TokenKind::BangEqual: value = lhs != rhs; break;
            case TokenKind::Less: value = lhs < rhs; break;
            case TokenKind::LessEqual: value = lhs <= rhs; break;
            case TokenKind::Greater: value = lhs > rhs; break;
            case TokenKind::GreaterEqual: value = lhs >= rhs; break;
            default: return std::nullopt;
        }
    }
    else
    {
        switch (operation)
        {
            case TokenKind::EqualEqual: value = left->integer == right->integer; break;
            case TokenKind::BangEqual: value = left->integer != right->integer; break;
            case TokenKind::Less: value = left->integer < right->integer; break;
            case TokenKind::LessEqual: value = left->integer <= right->integer; break;
            case TokenKind::Greater: value = left->integer > right->integer; break;
            case TokenKind::GreaterEqual: value = left->integer >= right->integer; break;
            default: return std::nullopt;
        }
    }
    return Stage37ConstantValue{
        Stage37ConstantKind::Boolean, "builtin:bool", 0, 0.0, value};
}

std::optional<Stage37ConstantValue> evaluateStage37FloatingArithmetic(
    const Stage37ConstantValue& left,
    const Stage37ConstantValue& right,
    TokenKind operation,
    std::string_view resultType)
{
    double value = 0.0;
    switch (operation)
    {
        case TokenKind::Plus: value = left.floating + right.floating; break;
        case TokenKind::Minus: value = left.floating - right.floating; break;
        case TokenKind::Star: value = left.floating * right.floating; break;
        case TokenKind::Slash: value = left.floating / right.floating; break;
        case TokenKind::Percent: value = std::fmod(left.floating, right.floating); break;
        default: return std::nullopt;
    }
    return Stage37ConstantValue{
        Stage37ConstantKind::Floating,
        std::string(resultType),
        0,
        normalizeFloatingConstant(value, resultType),
        false};
}

std::optional<Stage37ConstantValue> evaluateStage37IntegerArithmetic(
    const Stage37ConstantValue& left,
    const Stage37ConstantValue& right,
    TokenKind operation,
    std::string_view resultType)
{
    const unsigned width = integralBitWidth(resultType);
    const std::uint64_t lhs = normalizeIntegral(left.integer, resultType);
    const std::uint64_t rhs = normalizeIntegral(right.integer, resultType);
    std::uint64_t value = 0;
    switch (operation)
    {
        case TokenKind::Plus: value = lhs + rhs; break;
        case TokenKind::Minus: value = lhs - rhs; break;
        case TokenKind::Star: value = lhs * rhs; break;
        case TokenKind::Amp: value = lhs & rhs; break;
        case TokenKind::Pipe: value = lhs | rhs; break;
        case TokenKind::Caret: value = lhs ^ rhs; break;
        case TokenKind::LessLess:
            if (rhs >= width) return std::nullopt;
            value = lhs << static_cast<unsigned>(rhs);
            break;
        case TokenKind::GreaterGreater:
            if (rhs >= width) return std::nullopt;
            value = isSignedIntegralType(resultType)
                ? static_cast<std::uint64_t>(
                    signedIntegral(lhs, resultType) >> static_cast<unsigned>(rhs))
                : (lhs >> static_cast<unsigned>(rhs));
            break;
        case TokenKind::Slash:
        case TokenKind::Percent:
            if (rhs == 0) return std::nullopt;
            if (isSignedIntegralType(resultType))
            {
                const std::int64_t signedLeft = signedIntegral(lhs, resultType);
                const std::int64_t signedRight = signedIntegral(rhs, resultType);
                const std::int64_t minimum = width == 64
                    ? std::numeric_limits<std::int64_t>::min()
                    : -(std::int64_t{1} << (width - 1U));
                if (signedLeft == minimum && signedRight == -1)
                {
                    value = operation == TokenKind::Slash
                        ? static_cast<std::uint64_t>(minimum)
                        : 0U;
                }
                else
                {
                    value = static_cast<std::uint64_t>(operation == TokenKind::Slash
                        ? signedLeft / signedRight
                        : signedLeft % signedRight);
                }
            }
            else
            {
                value = operation == TokenKind::Slash ? lhs / rhs : lhs % rhs;
            }
            break;
        default: return std::nullopt;
    }
    return Stage37ConstantValue{
        Stage37ConstantKind::Integer,
        std::string(resultType),
        normalizeIntegral(value, resultType),
        0.0,
        false};
}

std::optional<Stage37ConstantValue> evaluateStage37Binary(
    const ProjectMethodBodyModel& model,
    const ProjectMemberModel* memberModel,
    const ast::AstNode* node,
    std::size_t context,
    std::string_view resultType,
    std::unordered_set<std::size_t>& evaluatingFields)
{
    const std::optional<Stage37ConstantValue> leftRaw = evaluateStage37Constant(
        model, memberModel, node->first(), context, evaluatingFields);
    const std::optional<Stage37ConstantValue> rightRaw = evaluateStage37Constant(
        model, memberModel, node->second(), context, evaluatingFields);
    if (!leftRaw.has_value() || !rightRaw.has_value()) return std::nullopt;
    const TokenKind operation = node->tokenKind();
    if (isLogical(operation))
    {
        if (leftRaw->kind != Stage37ConstantKind::Boolean ||
            rightRaw->kind != Stage37ConstantKind::Boolean)
        {
            return std::nullopt;
        }
        return Stage37ConstantValue{
            Stage37ConstantKind::Boolean,
            "builtin:bool",
            0,
            0.0,
            operation == TokenKind::AmpAmp
                ? (leftRaw->boolean && rightRaw->boolean)
                : (leftRaw->boolean || rightRaw->boolean)};
    }
    if (isComparison(operation))
    {
        return evaluateStage37Comparison(*leftRaw, *rightRaw, operation);
    }
    if (!isArithmetic(operation) || resultType.empty()) return std::nullopt;
    const std::optional<Stage37ConstantValue> left =
        convertStage37Constant(*leftRaw, resultType);
    const std::optional<Stage37ConstantValue> right =
        convertStage37Constant(*rightRaw, resultType);
    if (!left.has_value() || !right.has_value()) return std::nullopt;
    if (left->kind == Stage37ConstantKind::Floating)
    {
        return evaluateStage37FloatingArithmetic(*left, *right, operation, resultType);
    }
    if (left->kind != Stage37ConstantKind::Integer ||
        right->kind != Stage37ConstantKind::Integer)
    {
        return std::nullopt;
    }
    return evaluateStage37IntegerArithmetic(*left, *right, operation, resultType);
}

std::optional<Stage37ConstantValue> evaluateStage37Constant(
    const ProjectMethodBodyModel& model,
    const ProjectMemberModel* memberModel,
    const ast::AstNode* node,
    std::size_t context,
    std::unordered_set<std::size_t>& evaluatingFields)
{
    if (node == nullptr) return std::nullopt;
    std::string resultType = stage37ExpressionType(model, node, context);
    switch (node->kind())
    {
        case ast::AstNodeKind::IntegerLiteralExpression:
            if (resultType.empty()) resultType = integerLiteralType(node);
            return Stage37ConstantValue{
                Stage37ConstantKind::Integer, resultType,
                normalizeIntegral(node->integerValue(), resultType), 0.0, false};
        case ast::AstNodeKind::CharacterLiteralExpression:
            if (resultType.empty()) resultType = "builtin:char";
            return Stage37ConstantValue{
                Stage37ConstantKind::Integer, resultType,
                normalizeIntegral(node->integerValue(), resultType), 0.0, false};
        case ast::AstNodeKind::FloatingLiteralExpression:
            if (resultType.empty())
            {
                resultType = (!node->text().empty() &&
                    (node->text().back() == 'f' || node->text().back() == 'F'))
                    ? "builtin:float"
                    : "builtin:double";
            }
            return Stage37ConstantValue{
                Stage37ConstantKind::Floating, resultType, 0,
                normalizeFloatingConstant(node->floatingValue(), resultType), false};
        case ast::AstNodeKind::BooleanLiteralExpression:
            return Stage37ConstantValue{
                Stage37ConstantKind::Boolean, "builtin:bool", 0, 0.0,
                node->tokenKind() == TokenKind::KwTrue};
        case ast::AstNodeKind::NameOfExpression:
        case ast::AstNodeKind::StringLiteralExpression:
            return Stage37ConstantValue{
                Stage37ConstantKind::String, "builtin:string", 0, 0.0, false, node->text()};
        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
            return evaluateStage37Identifier(
                model, memberModel, node, context, evaluatingFields);
        case ast::AstNodeKind::CastExpression:
        {
            const std::optional<Stage37ConstantValue> operand = evaluateStage37Constant(
                model, memberModel, node->first(), context, evaluatingFields);
            if (!operand.has_value() || resultType.empty()) return std::nullopt;
            // Stage 37 deliberately excludes floating-to-integral native const casts. This is
            // the exact same boundary enforced by ProjectOopMirModel.
            return convertStage37Constant(*operand, resultType);
        }
        case ast::AstNodeKind::UnaryExpression:
            return evaluateStage37Unary(
                model, memberModel, node, context, resultType, evaluatingFields);
        case ast::AstNodeKind::BinaryExpression:
            return evaluateStage37Binary(
                model, memberModel, node, context, resultType, evaluatingFields);
        default:
            return std::nullopt;
    }
}
}
