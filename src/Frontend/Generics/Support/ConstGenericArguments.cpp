#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../../Internal/CloneDepthGuard.h"
#include "../../Cache/CachePayloadReader.h"
#include "../../Internal/EnumConstResolver.h"
#include "../../Internal/SpecializationWaveTimer.h"
#include "../../Internal/InterfaceFingerprint.h"
#include "../../Internal/ImplementationFingerprint.h"
#include "../../Internal/LocatedNode.h"
#include "../../Internal/MirFunctionKey.h"
#include "../../Internal/MirCacheRecord.h"
#include "../../Internal/ProjectGenericDependencyFrame.h"
#include "../../Internal/ProjectGenericBodyFrame.h"
#include "../../Internal/ProjectGenericAliasFrame.h"
#include "../../Internal/ProjectGenericChildWork.h"
#include "../../Internal/ProjectGenericCollectFrame.h"
#include "../../Internal/ProjectGenericProcessFrame.h"
#include "../../Internal/ProjectGenericReservedDemand.h"
#include "../../Internal/TypeWork.h"
#include "nativesharp/Frontend/ProjectOpaqueResultModel.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "ConstGenericArguments.h"
namespace nativesharp::frontend::internal
{
bool decodeConstGenericArgument(
    const ast::TypeSyntax* syntax,
    std::string& typeKey,
    std::string& value) noexcept
{
    return syntax != nullptr && syntax->typeArguments().empty() && syntax->modifiers().empty() &&
        language::parseConstGenericArgumentMarker(syntax->baseName(), typeKey, value);
}

ast::AstNode* constArgumentExpression(
    ast::AstArena& arena,
    std::string_view typeKey,
    std::string_view value,
    SourceRange range);

static ast::AstNode* tupleConstArgumentExpression(
    ast::AstArena& arena,
    std::string_view value,
    SourceRange range)
{
    ast::AstNode* tuple = arena.make<ast::AstNode>(
        ast::AstNodeKind::TupleExpression, range);
    std::size_t cursor = 0;
    while (cursor < value.size())
    {
        const std::size_t colon = value.find(':', cursor);
        if (colon == std::string_view::npos || colon == cursor) return nullptr;
        std::size_t length = 0;
        const auto lengthResult = std::from_chars(
            value.data() + cursor, value.data() + colon, length);
        if (lengthResult.ec != std::errc{} || lengthResult.ptr != value.data() + colon)
            return nullptr;
        const std::size_t markerBegin = colon + 1U;
        if (length > value.size() - markerBegin) return nullptr;
        const std::size_t markerEnd = markerBegin + length;
        if (markerEnd >= value.size() || value[markerEnd] != ';') return nullptr;
        std::string elementType;
        std::string elementValue;
        if (!language::parseConstGenericArgumentMarker(
                value.substr(markerBegin, length), elementType, elementValue))
            return nullptr;
        ast::AstNode* element = constArgumentExpression(
            arena, elementType, elementValue, range);
        if (element == nullptr) return nullptr;
        tuple->addChild(element);
        cursor = markerEnd + 1U;
    }
    return tuple;
}

ast::AstNode* constArgumentExpression(
    ast::AstArena& arena,
    std::string_view typeKey,
    std::string_view value,
    SourceRange range)
{
    if (typeKey.starts_with("tuple<"))
        return tupleConstArgumentExpression(arena, value, range);

    const bool boolean = typeKey == "builtin:bool";
    if (!boolean && !value.empty() && value.front() == '-')
    {
        std::uint64_t magnitude = 0;
        const auto result = std::from_chars(
            value.data() + 1, value.data() + value.size(), magnitude);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) return nullptr;
        ast::AstNode* operand = arena.make<ast::AstNode>(
            ast::AstNodeKind::IntegerLiteralExpression, range);
        operand->setText(std::string(value.substr(1)));
        operand->setTokenKind(lexer::TokenKind::IntegerLiteral);
        operand->setIntegerValue(magnitude);
        ast::AstNode* negation = arena.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression, range);
        negation->setText(std::string(value));
        negation->setTokenKind(lexer::TokenKind::Minus);
        negation->setFirst(operand);
        return negation;
    }

    ast::AstNode* literal = arena.make<ast::AstNode>(
        boolean ? ast::AstNodeKind::BooleanLiteralExpression
                : ast::AstNodeKind::IntegerLiteralExpression,
        range);
    literal->setText(std::string(value));
    literal->setTokenKind(boolean
        ? (value == "true" ? lexer::TokenKind::KwTrue : lexer::TokenKind::KwFalse)
        : lexer::TokenKind::IntegerLiteral);
    if (!boolean)
    {
        std::uint64_t integer = 0;
        const auto result = std::from_chars(
            value.data(), value.data() + value.size(), integer);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) return nullptr;
        literal->setIntegerValue(integer);
    }
    return literal;
}

bool normalizeConstExpressionArgument(
    ast::TypeSyntax* syntax,
    std::string& typeKey,
    std::string& value)
{
    if (syntax == nullptr || syntax->constExpression() == nullptr) return false;
    const ast::AstNode* expression = syntax->constExpression();
    if (expression->kind() == ast::AstNodeKind::TupleExpression)
    {
        std::string tupleType = "tuple<";
        std::string tupleValue;
        for (std::size_t index = 0; index < expression->children().size(); ++index)
        {
            ast::TypeSyntax elementCarrier(
                expression->children()[index]->range(), lexer::TokenKind::Invalid,
                "$nsc_const_expression$");
            elementCarrier.setConstExpression(expression->children()[index]);
            std::string elementType;
            std::string elementValue;
            if (!normalizeConstExpressionArgument(
                    &elementCarrier, elementType, elementValue)) return false;
            if (index != 0) tupleType += ',';
            tupleType += elementType;
            const std::string marker =
                language::constGenericArgumentMarker(elementType, elementValue);
            tupleValue += std::to_string(marker.size()) + ":" + marker + ";";
        }
        tupleType += '>';
        typeKey = std::move(tupleType);
        value = std::move(tupleValue);
        syntax->setBaseName(language::constGenericArgumentMarker(typeKey, value));
        syntax->setConstExpression(nullptr);
        return true;
    }
    if (expression->kind() == ast::AstNodeKind::BooleanLiteralExpression)
    {
        typeKey = "builtin:bool";
        value = expression->tokenKind() == lexer::TokenKind::KwTrue ? "true" : "false";
    }
    else
    {
        const ProjectIntegerEvaluator evaluator(nullptr);
        const std::optional<ProjectIntegerValue> evaluated = evaluator.evaluate(expression);
        if (!evaluated.has_value()) return false;
        typeKey = evaluated->typeKey;
        value = std::to_string(evaluated->bits);
    }
    syntax->setBaseName(language::constGenericArgumentMarker(typeKey, value));
    syntax->setConstExpression(nullptr);
    return true;
}

std::string constGenericParameterTypeKey(const ast::GenericParameterSyntax& parameter)
{
    if (parameter.constantType == nullptr) return {};
    return lexer::isBuiltinTypeKeyword(parameter.constantType->baseTokenKind())
        ? "builtin:" + parameter.constantType->baseName()
        : std::string{};
}

std::optional<std::size_t> trailingPackIndex(const GenericTemplateRecord& record) noexcept
{
    if (record.parameterSyntaxes.empty()) return std::nullopt;
    const std::size_t index = record.parameterSyntaxes.size() - 1U;
    return record.parameterSyntaxes[index].kind == ast::GenericParameterSyntaxKind::TypePack ||
        record.parameterSyntaxes[index].kind == ast::GenericParameterSyntaxKind::ConstantPack
        ? std::optional<std::size_t>(index) : std::nullopt;
}
}
