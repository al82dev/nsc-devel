#include "MirConstantEvaluator.h"

#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <cmath>
#include <limits>
#include <string_view>
#include <tuple>

namespace nativesharp::frontend
{
namespace
{
const ProjectOopMirStaticField* knownStaticField(
    std::size_t memberOrdinal,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept
{
    for (const ProjectOopMirStaticField& field : knownFields)
    {
        if (field.memberOrdinal == memberOrdinal && field.isConst && field.hasConstantInitializer)
        {
            return &field;
        }
    }
    return nullptr;
}

bool isSignedIntegralType(std::string_view typeKey) noexcept
{
    // ID 89. Dice lo mismo que la tabla del modelo semantico y que la del backend, a proposito:
    // cada capa contesta sin depender de las otras. Las tres se fijan entre si en la fixture c19.
    return typeKey == "builtin:sbyte" || typeKey == "builtin:short" ||
        typeKey == "builtin:int" || typeKey == "builtin:long" || typeKey == "builtin:nint" ||
        typeKey == "builtin:i128";
}

unsigned integerBitWidth(std::string_view typeKey) noexcept
{
    if (typeKey == "builtin:sbyte" || typeKey == "builtin:byte" ||
        typeKey == "builtin:char") return 8;
    if (typeKey == "builtin:short" || typeKey == "builtin:ushort") return 16;
    if (typeKey == "builtin:int" || typeKey == "builtin:uint") return 32;
    if (typeKey == "builtin:i128" || typeKey == "builtin:u128") return 128;
    if (typeKey == "builtin:long" || typeKey == "builtin:ulong" ||
        typeKey == "builtin:nint" || typeKey == "builtin:nuint") return 64;
    return 64;
}

std::uint64_t integerMask(unsigned width) noexcept
{
    return width >= 64 ? std::numeric_limits<std::uint64_t>::max()
                       : ((std::uint64_t{1} << width) - 1U);
}

// ID 127. The MIR evaluator delegates the central truncate-and-sign-extend operation to the
// semantic evaluator. Keeping this delegation prevents the historical third width/sign table
// from diverging again when a negative narrow value is widened before constant arithmetic.
std::uint64_t normalizeInteger(std::uint64_t value, std::string_view typeKey) noexcept
{
    return ProjectIntegerEvaluator::normalize(value, typeKey);
}

std::int64_t signedInteger(std::uint64_t bits, std::string_view typeKey) noexcept
{
    const unsigned width = integerBitWidth(typeKey);
    bits &= integerMask(width);
    if (width >= 64) return static_cast<std::int64_t>(bits);
    const std::uint64_t sign = std::uint64_t{1} << (width - 1U);
    if ((bits & sign) == 0) return static_cast<std::int64_t>(bits);
    return static_cast<std::int64_t>(bits | ~integerMask(width));
}

double normalizeFloating(double value, std::string_view typeKey) noexcept
{
    return typeKey == "builtin:float"
        ? static_cast<double>(static_cast<float>(value))
        : value;
}

std::optional<std::uint64_t> integerConversionValue(
    const ProjectOopHirNode* node,
    std::optional<std::uint64_t> value) noexcept
{
    if (!ProjectTypeConversion::isIntegral(node->typeKey)) return std::nullopt;
    const ProjectOopHirNode* child = node->children.front().get();
    if (!ProjectTypeConversion::isIntegral(child->typeKey)) return std::nullopt;
    return value.has_value()
        ? std::optional<std::uint64_t>(normalizeInteger(*value, node->typeKey))
        : std::nullopt;
}

std::optional<std::uint64_t> integerUnaryValue(
    const ProjectOopHirNode* node,
    std::optional<std::uint64_t> value) noexcept
{
    if (!value.has_value()) return std::nullopt;
    switch (node->operation)
    {
        case lexer::TokenKind::Plus: return normalizeInteger(*value, node->typeKey);
        case lexer::TokenKind::Minus:
            return normalizeInteger(std::uint64_t{0} - *value, node->typeKey);
        case lexer::TokenKind::Tilde: return normalizeInteger(~*value, node->typeKey);
        default: return std::nullopt;
    }
}

std::optional<std::uint64_t> signedDivisionOrRemainder(
    const ProjectOopHirNode* node,
    std::uint64_t left,
    std::uint64_t right,
    unsigned width) noexcept
{
    const std::int64_t lhs = signedInteger(left, node->typeKey);
    const std::int64_t rhs = signedInteger(right, node->typeKey);
    const std::int64_t minimum = width == 64
        ? std::numeric_limits<std::int64_t>::min()
        : -(std::int64_t{1} << (width - 1U));
    if (lhs == minimum && rhs == -1)
    {
        return node->operation == lexer::TokenKind::Slash
            ? static_cast<std::uint64_t>(minimum)
            : std::uint64_t{0};
    }
    const std::int64_t computed = node->operation == lexer::TokenKind::Slash
        ? lhs / rhs
        : lhs % rhs;
    return static_cast<std::uint64_t>(computed);
}

std::optional<std::uint64_t> integerBinaryValue(
    const ProjectOopHirNode* node,
    std::optional<std::uint64_t> leftValue,
    std::optional<std::uint64_t> rightValue) noexcept
{
    if (!leftValue.has_value() || !rightValue.has_value()) return std::nullopt;
    const unsigned width = integerBitWidth(node->typeKey);
    const std::uint64_t left = normalizeInteger(*leftValue, node->typeKey);
    const std::uint64_t right = normalizeInteger(*rightValue, node->typeKey);
    std::uint64_t result = 0;
    switch (node->operation)
    {
        case lexer::TokenKind::Plus: result = left + right; break;
        case lexer::TokenKind::Minus: result = left - right; break;
        case lexer::TokenKind::Star: result = left * right; break;
        case lexer::TokenKind::Amp: result = left & right; break;
        case lexer::TokenKind::Pipe: result = left | right; break;
        case lexer::TokenKind::Caret: result = left ^ right; break;
        case lexer::TokenKind::LessLess:
            if (right >= width) return std::nullopt;
            result = left << static_cast<unsigned>(right);
            break;
        case lexer::TokenKind::GreaterGreater:
            if (right >= width) return std::nullopt;
            result = isSignedIntegralType(node->typeKey)
                ? static_cast<std::uint64_t>(signedInteger(left, node->typeKey) >>
                    static_cast<unsigned>(right))
                : left >> static_cast<unsigned>(right);
            break;
        case lexer::TokenKind::Slash:
        case lexer::TokenKind::Percent:
            if (right == 0) return std::nullopt;
            if (isSignedIntegralType(node->typeKey))
            {
                const std::optional<std::uint64_t> signedResult =
                    signedDivisionOrRemainder(node, left, right, width);
                result = *signedResult;
            }
            else
            {
                result = node->operation == lexer::TokenKind::Slash ? left / right : left % right;
            }
            break;
        default: return std::nullopt;
    }
    return normalizeInteger(result, node->typeKey);
}

std::optional<double> floatingStaticFieldValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept
{
    const ProjectOopMirStaticField* field = knownStaticField(*node->memberOrdinal, knownFields);
    if (field == nullptr) return std::nullopt;
    if (field->typeKey == "builtin:float" || field->typeKey == "builtin:double")
        return field->constantFloatingValue;
    if (!ProjectTypeConversion::isIntegral(field->typeKey)) return std::nullopt;
    const std::uint64_t bits = normalizeInteger(field->constantIntegerValue, field->typeKey);
    return isSignedIntegralType(field->typeKey)
        ? static_cast<double>(signedInteger(bits, field->typeKey))
        : static_cast<double>(bits);
}

std::optional<double> floatingBinaryValue(
    const ProjectOopHirNode* node,
    std::optional<double> left,
    std::optional<double> right) noexcept
{
    if (!left.has_value() || !right.has_value()) return std::nullopt;
    double result = 0.0;
    switch (node->operation)
    {
        case lexer::TokenKind::Plus: result = *left + *right; break;
        case lexer::TokenKind::Minus: result = *left - *right; break;
        case lexer::TokenKind::Star: result = *left * *right; break;
        case lexer::TokenKind::Slash: result = *left / *right; break;
        case lexer::TokenKind::Percent: result = std::fmod(*left, *right); break;
        default: return std::nullopt;
    }
    return normalizeFloating(result, node->typeKey);
}

std::optional<double> floatingConversionValue(
    const ProjectOopHirNode* node,
    std::optional<double> value) noexcept
{
    return value.has_value()
        ? std::optional<double>(normalizeFloating(*value, node->typeKey))
        : std::nullopt;
}

std::optional<double> floatingUnaryValue(
    const ProjectOopHirNode* node,
    std::optional<double> value) noexcept
{
    if (!value.has_value()) return std::nullopt;
    if (node->operation == lexer::TokenKind::Plus)
        return normalizeFloating(*value, node->typeKey);
    if (node->operation == lexer::TokenKind::Minus)
        return normalizeFloating(-*value, node->typeKey);
    return std::nullopt;
}

std::optional<bool> floatingComparison(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept
{
    const std::optional<double> left =
        MirConstantEvaluator::floatingValue(node->children[0].get(), knownFields);
    const std::optional<double> right =
        MirConstantEvaluator::floatingValue(node->children[1].get(), knownFields);
    if (!left.has_value() || !right.has_value()) return std::nullopt;
    switch (node->operation)
    {
        case lexer::TokenKind::EqualEqual: return *left == *right;
        case lexer::TokenKind::BangEqual: return *left != *right;
        case lexer::TokenKind::Less: return *left < *right;
        case lexer::TokenKind::LessEqual: return *left <= *right;
        case lexer::TokenKind::Greater: return *left > *right;
        case lexer::TokenKind::GreaterEqual: return *left >= *right;
        default: return std::nullopt;
    }
}

std::optional<bool> integerComparison(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept
{
    const std::optional<std::uint64_t> left =
        MirConstantEvaluator::integerValue(node->children[0].get(), knownFields);
    const std::optional<std::uint64_t> right =
        MirConstantEvaluator::integerValue(node->children[1].get(), knownFields);
    if (!left.has_value() || !right.has_value()) return std::nullopt;
    if (isSignedIntegralType(node->children[0]->typeKey))
    {
        const auto lhs = static_cast<std::int64_t>(*left);
        const auto rhs = static_cast<std::int64_t>(*right);
        switch (node->operation)
        {
            case lexer::TokenKind::EqualEqual: return lhs == rhs;
            case lexer::TokenKind::BangEqual: return lhs != rhs;
            case lexer::TokenKind::Less: return lhs < rhs;
            case lexer::TokenKind::LessEqual: return lhs <= rhs;
            case lexer::TokenKind::Greater: return lhs > rhs;
            case lexer::TokenKind::GreaterEqual: return lhs >= rhs;
            default: return std::nullopt;
        }
    }
    switch (node->operation)
    {
        case lexer::TokenKind::EqualEqual: return *left == *right;
        case lexer::TokenKind::BangEqual: return *left != *right;
        case lexer::TokenKind::Less: return *left < *right;
        case lexer::TokenKind::LessEqual: return *left <= *right;
        case lexer::TokenKind::Greater: return *left > *right;
        case lexer::TokenKind::GreaterEqual: return *left >= *right;
        default: return std::nullopt;
    }
}

std::optional<bool> booleanBinaryValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept
{
    if (node->operation == lexer::TokenKind::AmpAmp || node->operation == lexer::TokenKind::PipePipe)
    {
        const std::optional<bool> left =
            MirConstantEvaluator::booleanValue(node->children[0].get(), knownFields);
        const std::optional<bool> right =
            MirConstantEvaluator::booleanValue(node->children[1].get(), knownFields);
        if (!left.has_value() || !right.has_value()) return std::nullopt;
        return node->operation == lexer::TokenKind::AmpAmp ? (*left && *right) : (*left || *right);
    }
    if (node->operation == lexer::TokenKind::EqualEqual ||
        node->operation == lexer::TokenKind::BangEqual)
    {
        const std::optional<bool> left =
            MirConstantEvaluator::booleanValue(node->children[0].get(), knownFields);
        const std::optional<bool> right =
            MirConstantEvaluator::booleanValue(node->children[1].get(), knownFields);
        if (left.has_value() && right.has_value())
            return node->operation == lexer::TokenKind::EqualEqual ? *left == *right : *left != *right;
    }
    const bool floating = node->children[0]->typeKey == "builtin:float" ||
        node->children[0]->typeKey == "builtin:double";
    return floating
        ? floatingComparison(node, knownFields)
        : integerComparison(node, knownFields);
}
}

std::optional<std::string> MirConstantEvaluator::stringValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields)
{
    while (node != nullptr)
    {
        if (node->kind == ProjectOopHirNodeKind::StringLiteral) return node->stringValue;
        if (node->kind == ProjectOopHirNodeKind::StaticFieldValue && node->memberOrdinal.has_value())
        {
            const ProjectOopMirStaticField* field =
                knownStaticField(*node->memberOrdinal, knownFields);
            return field != nullptr && field->typeKey == "builtin:string"
                ? std::optional<std::string>(field->constantStringValue)
                : std::nullopt;
        }
        if (node->kind != ProjectOopHirNodeKind::Conversion ||
            node->typeKey != "builtin:string" || node->children.empty())
            return std::nullopt;
        node = node->children.front().get();
    }
    return std::nullopt;
}

std::optional<std::uint64_t> MirConstantEvaluator::integerValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept
{
    using Result = std::optional<std::uint64_t>;
    using Frame = std::tuple<const ProjectOopHirNode*, unsigned, Result>;
    std::vector<Frame> work{{node, 0U, std::nullopt}};
    std::vector<Result> values;
    const auto finish = [&](Result value)
    {
        work.pop_back();
        values.push_back(std::move(value));
    };
    while (!work.empty())
    {
        auto& [current, state, left] = work.back();
        if (state == 0U)
        {
            if (current == nullptr) { finish(std::nullopt); continue; }
            if (current->kind == ProjectOopHirNodeKind::IntegerConstant)
            {
                finish(normalizeInteger(current->integerValue, current->typeKey));
                continue;
            }
            if (current->kind == ProjectOopHirNodeKind::StaticFieldValue &&
                current->memberOrdinal.has_value())
            {
                const ProjectOopMirStaticField* field =
                    knownStaticField(*current->memberOrdinal, knownFields);
                finish(field != nullptr && ProjectTypeConversion::isIntegral(field->typeKey)
                    ? Result(normalizeInteger(field->constantIntegerValue, field->typeKey))
                    : std::nullopt);
                continue;
            }
            const bool conversion = current->kind == ProjectOopHirNodeKind::Conversion &&
                !current->children.empty();
            const bool unary = current->kind == ProjectOopHirNodeKind::Unary &&
                !current->children.empty();
            const bool binary = current->kind == ProjectOopHirNodeKind::Binary &&
                current->children.size() == 2U;
            if (!conversion && !unary && !binary) { finish(std::nullopt); continue; }
            if (conversion && (!ProjectTypeConversion::isIntegral(current->typeKey) ||
                !ProjectTypeConversion::isIntegral(current->children.front()->typeKey)))
            {
                finish(std::nullopt);
                continue;
            }
            state = 1U;
            work.emplace_back(current->children.front().get(), 0U, std::nullopt);
            continue;
        }
        Result child = std::move(values.back());
        values.pop_back();
        if (state == 1U && current->kind == ProjectOopHirNodeKind::Binary)
        {
            left = std::move(child);
            state = 2U;
            work.emplace_back(current->children[1].get(), 0U, std::nullopt);
            continue;
        }
        if (current->kind == ProjectOopHirNodeKind::Conversion)
            finish(integerConversionValue(current, std::move(child)));
        else if (current->kind == ProjectOopHirNodeKind::Unary)
            finish(integerUnaryValue(current, std::move(child)));
        else
            finish(integerBinaryValue(current, std::move(left), std::move(child)));
    }
    return values.empty() ? std::nullopt : std::move(values.back());
}

std::optional<double> MirConstantEvaluator::floatingValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept
{
    using Result = std::optional<double>;
    using Frame = std::tuple<const ProjectOopHirNode*, unsigned, Result>;
    std::vector<Frame> work{{node, 0U, std::nullopt}};
    std::vector<Result> values;
    const auto finish = [&](Result value)
    {
        work.pop_back();
        values.push_back(std::move(value));
    };
    while (!work.empty())
    {
        auto& [current, state, left] = work.back();
        if (state == 0U)
        {
            if (current == nullptr) { finish(std::nullopt); continue; }
            if (current->kind == ProjectOopHirNodeKind::FloatingConstant)
            {
                finish(normalizeFloating(current->floatingValue, current->typeKey));
                continue;
            }
            if (current->kind == ProjectOopHirNodeKind::IntegerConstant)
            {
                const std::uint64_t bits =
                    normalizeInteger(current->integerValue, current->typeKey);
                finish(isSignedIntegralType(current->typeKey)
                    ? Result(static_cast<double>(signedInteger(bits, current->typeKey)))
                    : Result(static_cast<double>(bits)));
                continue;
            }
            if (current->kind == ProjectOopHirNodeKind::StaticFieldValue &&
                current->memberOrdinal.has_value())
            {
                finish(floatingStaticFieldValue(current, knownFields));
                continue;
            }
            const bool conversion = current->kind == ProjectOopHirNodeKind::Conversion &&
                !current->children.empty();
            const bool unary = current->kind == ProjectOopHirNodeKind::Unary &&
                !current->children.empty();
            const bool binary = current->kind == ProjectOopHirNodeKind::Binary &&
                current->children.size() == 2U;
            if (!conversion && !unary && !binary) { finish(std::nullopt); continue; }
            state = 1U;
            work.emplace_back(current->children.front().get(), 0U, std::nullopt);
            continue;
        }
        Result child = std::move(values.back());
        values.pop_back();
        if (state == 1U && current->kind == ProjectOopHirNodeKind::Binary)
        {
            left = std::move(child);
            state = 2U;
            work.emplace_back(current->children[1].get(), 0U, std::nullopt);
            continue;
        }
        if (current->kind == ProjectOopHirNodeKind::Conversion)
            finish(floatingConversionValue(current, std::move(child)));
        else if (current->kind == ProjectOopHirNodeKind::Unary)
            finish(floatingUnaryValue(current, std::move(child)));
        else
            finish(floatingBinaryValue(current, std::move(left), std::move(child)));
    }
    return values.empty() ? std::nullopt : std::move(values.back());
}

std::optional<bool> MirConstantEvaluator::booleanValue(
    const ProjectOopHirNode* node,
    const std::vector<ProjectOopMirStaticField>& knownFields) noexcept
{
    if (node == nullptr) return std::nullopt;
    if (node->kind == ProjectOopHirNodeKind::BooleanConstant) return node->booleanValue;
    if (node->kind == ProjectOopHirNodeKind::StaticFieldValue && node->memberOrdinal.has_value())
    {
        const ProjectOopMirStaticField* field = knownStaticField(*node->memberOrdinal, knownFields);
        return field != nullptr && field->typeKey == "builtin:bool"
            ? std::optional<bool>(field->constantBooleanValue)
            : std::nullopt;
    }
    if (node->kind == ProjectOopHirNodeKind::Conversion && !node->children.empty())
        return booleanValue(node->children.front().get(), knownFields);
    if (node->kind == ProjectOopHirNodeKind::Unary && node->operation == lexer::TokenKind::Bang &&
        !node->children.empty())
    {
        const std::optional<bool> value = booleanValue(node->children.front().get(), knownFields);
        return value.has_value() ? std::optional<bool>(!*value) : std::nullopt;
    }
    if (node->kind == ProjectOopHirNodeKind::Binary && node->children.size() == 2)
        return booleanBinaryValue(node, knownFields);
    return std::nullopt;
}
}
