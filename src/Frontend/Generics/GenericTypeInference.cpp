#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../Internal/CloneDepthGuard.h"
#include "../Internal/ByteReader.h"
#include "../Internal/EnumConstResolver.h"
#include "../Internal/SpecializationWaveTimer.h"
#include "../Internal/InterfaceFingerprint.h"
#include "../Internal/ImplementationFingerprint.h"
#include "../Internal/LocatedNode.h"
#include "../Internal/MirFunctionKey.h"
#include "../Internal/MirCacheRecord.h"
#include "../Internal/ProjectGenericDependencyFrame.h"
#include "../Internal/ProjectGenericBodyFrame.h"
#include "../Internal/ProjectGenericAliasFrame.h"
#include "../Internal/ProjectGenericChildWork.h"
#include "../Internal/ProjectGenericCollectFrame.h"
#include "../Internal/ProjectGenericProcessFrame.h"
#include "../Internal/ProjectGenericReservedDemand.h"
#include "../Internal/TypeWork.h"
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
#include "GenericModelSupport.h"
#include "Support/TemplateClassification.h"
#include "Support/TemplateSpelling.h"
#include "Support/ConstGenericArguments.h"
#include "Support/GenericPatterns.h"
#include "Cache/TemplateCatalogCodec.h"
namespace nativesharp::frontend
{
using internal::appendOwner;
using internal::computeTemplateSourceFingerprint;
using internal::constArgumentExpression;
using internal::constGenericParameterTypeKey;
using internal::decodeConstGenericArgument;
using internal::deserializeAstNodeTree;
using internal::enclosingTypeSpellings;
using internal::genericArityAccepts;
using internal::genericAritySpelling;
using internal::genericTemplateAccessibility;
using internal::genericTypeTemplateKind;
using internal::isGenericMethodTemplateNode;
using internal::isGenericTypeTemplateNode;
using internal::isNamedTypeDeclaration;
using internal::isStructuralGenericSurface;
using internal::methodSpecializationFamilyKey;
using internal::normalizeConstExpressionArgument;
using internal::qualifiedName;
using internal::receiverSourceSpelling;
using internal::restoreCatalogEntry;
using internal::serializeAstNodeTree;
using internal::serializeCatalogEntry;
using internal::serializeMirCacheRecord;
using internal::splitDotted;
using internal::stableHashSuffix;
using internal::templateCacheKey;
using internal::templateSourceSpelling;
using internal::trailingPackIndex;
using internal::valueParameterCount;
using internal::ByteReader;
using internal::EnumConstResolver;
using internal::MirCacheRecord;
using internal::SpecializationWaveTimer;
#if 0 // INSTRUMENTATION CODE
using internal::genericNodeDetail;
using internal::genericTypeDetail;
using internal::genericUnitDetail;
using internal::GenericWorkProgress;
#endif // INSTRUMENTATION CODE
ast::TypeSyntax* ProjectGenericModel::makeInferredBuiltinType(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* expression,
    lexer::TokenKind kind,
    std::string name)
{
    return unit.arena().make<ast::TypeSyntax>(
        expression->range(), kind, std::move(name));
}

ast::TypeSyntax* ProjectGenericModel::cloneInferredType(
    ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* source)
{
    bool conflict = false;
    return cloneAliasType(unit.arena(), source, {}, conflict);
}

ast::TypeSyntax* ProjectGenericModel::inferNamedExpressionType(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* expression,
    const GenericLookupScope& scope)
{
    if (expression->kind() == ast::AstNodeKind::MatchValueExpression)
        return cloneInferredType(unit, scope.matchValueType);
    if (expression->kind() == ast::AstNodeKind::MemberExpression &&
        (expression->first() == nullptr ||
         expression->first()->kind() != ast::AstNodeKind::ThisExpression))
        return nullptr;
    const ast::TypeSyntax* found = scope.findValueType(expression->text());
    return found == nullptr ? nullptr : cloneInferredType(unit, found);
}

ast::TypeSyntax* ProjectGenericModel::inferIntegerLiteralType(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* expression)
{
    const std::string key(builtins::integerLiteralTypeKey(
        expression->text(), expression->integerValue(), false));
    if (key == "builtin:uint")
        return makeInferredBuiltinType(unit, expression, lexer::TokenKind::KwUInt, "uint");
    if (key == "builtin:long")
        return makeInferredBuiltinType(unit, expression, lexer::TokenKind::KwLong, "long");
    if (key == "builtin:ulong")
        return makeInferredBuiltinType(unit, expression, lexer::TokenKind::KwULong, "ulong");
    return makeInferredBuiltinType(unit, expression, lexer::TokenKind::KwInt, "int");
}

ast::TypeSyntax* ProjectGenericModel::inferConstructionExpressionType(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* expression)
{
    ast::TypeSyntax* result = cloneInferredType(unit, expression->typeSyntax());
    if (expression->kind() == ast::AstNodeKind::CastExpression ||
        expression->kind() == ast::AstNodeKind::SafeCastExpression ||
        expression->kind() == ast::AstNodeKind::DefaultValueExpression ||
        expression->kind() == ast::AstNodeKind::ObjectCreationExpression)
        return result;
    if (result == nullptr) return nullptr;
    ast::TypeModifierSyntaxKind modifier = ast::TypeModifierSyntaxKind::Pointer;
    if (expression->kind() == ast::AstNodeKind::NewExpression && expression->flag())
        modifier = ast::TypeModifierSyntaxKind::DynamicArray;
    result->addModifier(ast::TypeModifierSyntax{
        modifier, nullptr, expression->range()});
    return result;
}

ast::TypeSyntax* ProjectGenericModel::inferUnaryExpressionType(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* expression,
    const GenericLookupScope& scope)
{
    ast::TypeSyntax* operand = inferExpressionType(unit, expression->first(), scope);
    if (operand == nullptr) return nullptr;
    if (expression->tokenKind() == lexer::TokenKind::Bang)
        return makeInferredBuiltinType(
            unit, expression, lexer::TokenKind::KwBool, "bool");
    if (expression->tokenKind() == lexer::TokenKind::Amp)
    {
        operand->addModifier(ast::TypeModifierSyntax{
            ast::TypeModifierSyntaxKind::Pointer, nullptr, expression->range()});
        return operand;
    }
    if (expression->tokenKind() != lexer::TokenKind::Star ||
        operand->modifiers().empty() ||
        operand->modifiers().back().kind != ast::TypeModifierSyntaxKind::Pointer)
        return operand;
    ast::TypeSyntax* result = unit.arena().make<ast::TypeSyntax>(
        operand->range(), operand->baseTokenKind(), operand->baseName());
    result->setOwnershipQualifier(operand->ownershipQualifier());
    for (ast::TypeSyntax* argument : operand->typeArguments())
        result->addTypeArgument(cloneInferredType(unit, argument));
    for (std::size_t index = 0; index + 1 < operand->modifiers().size(); ++index)
        result->addModifier(operand->modifiers()[index]);
    return result;
}

ast::TypeSyntax* ProjectGenericModel::inferConditionalExpressionType(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* expression,
    const GenericLookupScope& scope)
{
    ast::TypeSyntax* whenTrue = inferExpressionType(unit, expression->second(), scope);
    ast::TypeSyntax* whenFalse = inferExpressionType(unit, expression->third(), scope);
    // A recursive generic lambda has no callable return entry until its first closed
    // body is inferred. One arm can still provide the fixed point (`n == 0 ? 0 : self`).
    // Keep the ordinary exact-equality rule when both arms are known.
    if (whenTrue == nullptr) return whenFalse;
    if (whenFalse == nullptr) return whenTrue;
    if (typeSpelling(whenTrue) != typeSpelling(whenFalse)) return nullptr;
    return whenTrue;
}

bool ProjectGenericModel::binaryExpressionReturnsBoolean(lexer::TokenKind tokenKind)
{
    switch (tokenKind)
    {
        case lexer::TokenKind::EqualEqual:
        case lexer::TokenKind::BangEqual:
        case lexer::TokenKind::Less:
        case lexer::TokenKind::LessEqual:
        case lexer::TokenKind::Greater:
        case lexer::TokenKind::GreaterEqual:
        case lexer::TokenKind::AmpAmp:
        case lexer::TokenKind::PipePipe:
            return true;
        default:
            return false;
    }
}

ast::TypeSyntax* ProjectGenericModel::inferBinaryExpressionType(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* expression,
    const GenericLookupScope& scope)
{
    if (binaryExpressionReturnsBoolean(expression->tokenKind()))
        return makeInferredBuiltinType(
            unit, expression, lexer::TokenKind::KwBool, "bool");
    ast::TypeSyntax* left = inferExpressionType(unit, expression->first(), scope);
    ast::TypeSyntax* right = inferExpressionType(unit, expression->second(), scope);
    if (left == nullptr) return right;
    if (right == nullptr) return left;
    if (typeSpelling(left) != typeSpelling(right)) return nullptr;
    return left;
}

ast::TypeSyntax* ProjectGenericModel::inferTupleExpressionType(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* expression,
    const GenericLookupScope& scope)
{
    ast::TypeSyntax* tuple = unit.arena().make<ast::TypeSyntax>(
        expression->range(), lexer::TokenKind::Invalid, "$tuple");
    tuple->setTuple();
    for (const ast::AstNode* element : expression->children())
    {
        if (element == nullptr ||
            element->kind() == ast::AstNodeKind::SpreadElementExpression)
            return nullptr;
        ast::TypeSyntax* elementType = inferExpressionType(unit, element, scope);
        if (elementType == nullptr) return nullptr;
        tuple->addTupleElement(elementType);
    }
    return tuple;
}

ast::TypeSyntax* ProjectGenericModel::inferCallExpressionType(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* expression)
{
    const auto found = inferredCallReturnTypes_.find(expression);
    if (found != inferredCallReturnTypes_.end())
        return cloneInferredType(unit, found->second);
    const auto ordinary = ordinaryCallableReturnTypes_.find(methodCalleeName(expression));
    return ordinary == ordinaryCallableReturnTypes_.end()
        ? nullptr : cloneInferredType(unit, ordinary->second);
}

ast::TypeSyntax* ProjectGenericModel::inferExpressionType(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* expression,
    const GenericLookupScope& scope)
{
    if (expression == nullptr) return nullptr;
#if 0 // INSTRUMENTATION CODE
    const GenericWorkProgress progress(
        telemetry_, "generics.infer-expression-type", genericNodeDetail(unit, *expression));
#endif // INSTRUMENTATION CODE
    switch (expression->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MatchValueExpression:
        case ast::AstNodeKind::MemberExpression:
            return inferNamedExpressionType(unit, expression, scope);
        case ast::AstNodeKind::IntegerLiteralExpression:
            return inferIntegerLiteralType(unit, expression);
        case ast::AstNodeKind::FloatingLiteralExpression:
            return !expression->text().empty() &&
                    (expression->text().back() == 'f' || expression->text().back() == 'F')
                ? makeInferredBuiltinType(
                    unit, expression, lexer::TokenKind::KwFloat, "float")
                : makeInferredBuiltinType(
                    unit, expression, lexer::TokenKind::KwDouble, "double");
        case ast::AstNodeKind::CharacterLiteralExpression:
            return makeInferredBuiltinType(
                unit, expression, lexer::TokenKind::KwChar, "char");
        case ast::AstNodeKind::StringLiteralExpression:
            return makeInferredBuiltinType(
                unit, expression, lexer::TokenKind::KwString, "string");
        case ast::AstNodeKind::BooleanLiteralExpression:
            return makeInferredBuiltinType(
                unit, expression, lexer::TokenKind::KwBool, "bool");
        case ast::AstNodeKind::CastExpression:
        case ast::AstNodeKind::SafeCastExpression:
        case ast::AstNodeKind::DefaultValueExpression:
        case ast::AstNodeKind::ObjectCreationExpression:
        case ast::AstNodeKind::PlacementConstructionExpression:
        case ast::AstNodeKind::NewExpression:
        case ast::AstNodeKind::StackallocExpression:
            return inferConstructionExpressionType(unit, expression);
        case ast::AstNodeKind::MoveExpression:
            return inferExpressionType(unit, expression->first(), scope);
        case ast::AstNodeKind::UnaryExpression:
            return inferUnaryExpressionType(unit, expression, scope);
        case ast::AstNodeKind::AssignmentExpression:
            return inferExpressionType(unit, expression->second(), scope);
        case ast::AstNodeKind::ConditionalExpression:
            return inferConditionalExpressionType(unit, expression, scope);
        case ast::AstNodeKind::BinaryExpression:
            return inferBinaryExpressionType(unit, expression, scope);
        case ast::AstNodeKind::TupleExpression:
            return inferTupleExpressionType(unit, expression, scope);
        case ast::AstNodeKind::CallExpression:
            return inferCallExpressionType(unit, expression);
        default:
            return nullptr;
    }
}

bool ProjectGenericModel::inferArrayExtentTypeArguments(
    ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* pattern,
    const ast::TypeSyntax* actual,
    const GenericTemplateRecord& record,
    std::size_t modifierPrefix,
    std::unordered_map<std::string, ast::TypeSyntax*>& inferred)
{
    for (std::size_t index = 0; index < pattern->modifiers().size(); ++index)
    {
        const ast::TypeModifierSyntax& patternModifier = pattern->modifiers()[index];
        const ast::TypeModifierSyntax& actualModifier =
            actual->modifiers()[modifierPrefix + index];
        if (patternModifier.kind != actualModifier.kind)
            return false;
        if (patternModifier.kind != ast::TypeModifierSyntaxKind::Array ||
            patternModifier.arrayCount == nullptr ||
            patternModifier.arrayCount->kind() != ast::AstNodeKind::IdentifierExpression)
            continue;

        const std::string& parameterName = patternModifier.arrayCount->text();
        const auto parameter = std::ranges::find_if(
            record.parameterSyntaxes,
            [&](const ast::GenericParameterSyntax& candidate)
            {
                return candidate.name == parameterName &&
                    candidate.kind == ast::GenericParameterSyntaxKind::Constant;
            });
        if (parameter == record.parameterSyntaxes.end() || actualModifier.arrayCount == nullptr)
            continue;

        const ProjectIntegerEvaluator evaluator(nullptr);
        const std::optional<ProjectIntegerValue> extent =
            evaluator.evaluate(actualModifier.arrayCount);
        if (!extent.has_value()) return false;
        ast::TypeSyntax* value = unit.arena().make<ast::TypeSyntax>(
            actualModifier.arrayCount->range(), lexer::TokenKind::Invalid,
            language::constGenericArgumentMarker(
                extent->typeKey, std::to_string(extent->bits)));
        if (!recordInferredTypeArgument(parameterName, value, inferred)) return false;
    }
    return true;
}

bool ProjectGenericModel::recordInferredTypeArgument(
    const std::string& parameterName,
    ast::TypeSyntax* value,
    std::unordered_map<std::string, ast::TypeSyntax*>& inferred) const
{
    const auto prior = inferred.find(parameterName);
    if (prior == inferred.end())
    {
        inferred.emplace(parameterName, value);
        return true;
    }
    return typeSpelling(prior->second) == typeSpelling(value);
}

std::optional<bool> ProjectGenericModel::inferConstantTypeArgument(
    ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* pattern,
    const ast::TypeSyntax* actual,
    const GenericTemplateRecord& record,
    const std::unordered_set<std::string>& parameters,
    std::unordered_map<std::string, ast::TypeSyntax*>& inferred)
{
    const auto constantParameter = std::ranges::find_if(
        record.parameterSyntaxes,
        [&](const ast::GenericParameterSyntax& candidate)
        {
            return candidate.name == pattern->baseName() &&
                candidate.kind == ast::GenericParameterSyntaxKind::Constant;
        });
    if (parameters.contains(pattern->baseName()) &&
        constantParameter != record.parameterSyntaxes.end() &&
        pattern->typeArguments().empty() && pattern->modifiers().empty())
    {
        std::string actualType;
        std::string actualValue;
        if (!decodeConstGenericArgument(actual, actualType, actualValue)) return false;
        ast::TypeSyntax* value = unit.arena().make<ast::TypeSyntax>(
            actual->range(), lexer::TokenKind::Invalid,
            language::constGenericArgumentMarker(actualType, actualValue));
        return recordInferredTypeArgument(pattern->baseName(), value, inferred);
    }
    return std::nullopt;
}

ast::TypeSyntax* ProjectGenericModel::cloneInferredTypeArgument(
    ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* pattern,
    const ast::TypeSyntax* actual,
    std::size_t modifierPrefix)
{
    ast::TypeSyntax* value = unit.arena().make<ast::TypeSyntax>(
        actual->range(), actual->baseTokenKind(), actual->baseName());
    if (pattern->ownershipQualifier() == ast::OwnershipQualifierSyntaxKind::None)
        value->setOwnershipQualifier(actual->ownershipQualifier());
    value->setTuple(actual->isTuple());
    value->setDynamicTrait(actual->isDynamicTrait());
    value->setOpaqueResult(actual->isOpaqueResult());
    value->setOpaqueResultTraitName(actual->opaqueResultTraitName());
    value->setPointeeConst(actual->isPointeeConst());
    if (actual->originProjectOrdinal().has_value())
        value->setOriginProjectOrdinal(*actual->originProjectOrdinal());
    if (actual->originNamespace().has_value())
        value->setOriginNamespace(*actual->originNamespace());
    bool conflict = false;
    for (std::size_t index = 0; index < actual->typeArguments().size(); ++index)
    {
        ast::TypeSyntax* argument = cloneAliasType(
            unit.arena(), actual->typeArguments()[index], {}, conflict);
        if (actual->isTuple())
        {
            const std::string name = index < actual->tupleElementNames().size()
                ? actual->tupleElementNames()[index] : std::string{};
            value->addTupleElement(argument, name);
        }
        else
        {
            value->addTypeArgument(argument);
        }
    }
    for (std::size_t index = 0; index < modifierPrefix; ++index)
        value->addModifier(actual->modifiers()[index]);
    return value;
}

std::optional<bool> ProjectGenericModel::inferNamedTypeArgument(
    ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* pattern,
    const ast::TypeSyntax* actual,
    const GenericTemplateRecord& record,
    const std::unordered_set<std::string>& parameters,
    std::size_t modifierPrefix,
    std::unordered_map<std::string, ast::TypeSyntax*>& inferred)
{
    const auto typeParameter = std::ranges::find_if(
        record.parameterSyntaxes,
        [&](const ast::GenericParameterSyntax& candidate)
        {
            return candidate.name == pattern->baseName() &&
                (candidate.kind == ast::GenericParameterSyntaxKind::Type ||
                 candidate.kind == ast::GenericParameterSyntaxKind::TypePack);
        });
    if (!parameters.contains(pattern->baseName()) ||
        typeParameter == record.parameterSyntaxes.end() || !pattern->typeArguments().empty())
        return std::nullopt;
    ast::TypeSyntax* value =
        cloneInferredTypeArgument(unit, pattern, actual, modifierPrefix);
    return recordInferredTypeArgument(pattern->baseName(), value, inferred);
}

bool ProjectGenericModel::inferTypeArguments(
    ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* pattern,
    const ast::TypeSyntax* actual,
    const GenericTemplateRecord& record,
    const std::unordered_set<std::string>& parameters,
    std::unordered_map<std::string, ast::TypeSyntax*>& inferred)
{
    if (pattern == nullptr || actual == nullptr) return false;
#if 0 // INSTRUMENTATION CODE
    const GenericWorkProgress progress(
        telemetry_, "generics.infer-type-arguments",
        "pattern-" + genericTypeDetail(*pattern) + " actual-" + genericTypeDetail(*actual));
#endif // INSTRUMENTATION CODE
    if (actual->modifiers().size() < pattern->modifiers().size()) return false;
    const std::size_t modifierPrefix = actual->modifiers().size() - pattern->modifiers().size();
    if (!inferArrayExtentTypeArguments(
            unit, pattern, actual, record, modifierPrefix, inferred))
        return false;
    const std::optional<bool> constant =
        inferConstantTypeArgument(unit, pattern, actual, record, parameters, inferred);
    if (constant.has_value()) return *constant;
    const std::optional<bool> named = inferNamedTypeArgument(
        unit, pattern, actual, record, parameters, modifierPrefix, inferred);
    if (named.has_value()) return *named;
    if (modifierPrefix != 0 || pattern->baseTokenKind() != actual->baseTokenKind() ||
        pattern->baseName() != actual->baseName() ||
        pattern->typeArguments().size() != actual->typeArguments().size())
        return false;
    for (std::size_t index = 0; index < pattern->typeArguments().size(); ++index)
    {
        if (!inferTypeArguments(
                unit, pattern->typeArguments()[index], actual->typeArguments()[index],
                record, parameters, inferred)) return false;
    }
    return true;
}

bool ProjectGenericModel::validateGenericPackArgumentCount(
    std::size_t argumentCount,
    const std::optional<std::size_t>& packIndex,
    SourceRange range,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!packIndex.has_value()) return true;
    const std::array<std::size_t, 1> lengths{argumentCount - *packIndex};
    std::string packError;
    if (language::validatePackExpansion(
            lengths, DefaultMaximumPackExpansionCount, packError))
        return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidArgumentCount,
        diagnostics::DiagnosticSeverity::Error,
        range,
        packError + " (limit " +
            std::to_string(DefaultMaximumPackExpansionCount) + ")");
    return false;
}

std::string ProjectGenericModel::structuralConstGenericTypeKey(
    const ast::TypeSyntax* type) const
{
    if (type == nullptr || !type->modifiers().empty()) return {};
    if (lexer::isBuiltinTypeKeyword(type->baseTokenKind()))
        return "builtin:" + type->baseName();
    if (type->isTuple())
    {
        std::string key = "tuple<";
        for (std::size_t item = 0; item < type->typeArguments().size(); ++item)
        {
            const std::string element =
                structuralConstGenericTypeKey(type->typeArguments()[item]);
            if (element.empty()) return {};
            if (item != 0) key += ',';
            key += element;
        }
        return key + '>';
    }
    const auto enumType = constEnumTypeBySpelling_.find(type->baseName());
    return enumType != constEnumTypeBySpelling_.end()
        ? enumType->second : std::string{};
}

std::string ProjectGenericModel::expectedConstGenericTypeKey(
    const ast::GenericParameterSyntax& parameter) const
{
    std::string expectedType = constGenericParameterTypeKey(parameter);
    if (expectedType.empty() && parameter.constantType != nullptr)
        expectedType = structuralConstGenericTypeKey(parameter.constantType);
    return expectedType;
}

bool ProjectGenericModel::normalizeEnumConstGenericArgument(
    ast::TypeSyntax* argument,
    const std::string& expectedType,
    std::string& argumentType,
    std::string& argumentValue) const
{
    if (!expectedType.starts_with("enum:") || argument == nullptr) return false;
    const auto enumValue = constEnumValueBySpelling_.find(argument->baseName());
    if (enumValue == constEnumValueBySpelling_.end() || enumValue->second.first.empty())
        return false;
    argumentType = enumValue->second.first;
    argumentValue = enumValue->second.second;
    argument->setBaseName(language::constGenericArgumentMarker(
        argumentType, argumentValue));
    return true;
}

bool ProjectGenericModel::validateConstGenericArgumentKind(
    const GenericTemplateRecord& record,
    const ast::GenericParameterSyntax& parameter,
    ast::GenericParameterSyntaxKind kind,
    std::size_t parameterIndex,
    ast::TypeSyntax* argument,
    bool constant,
    const std::string& expectedType,
    const std::string& constSubject,
    SourceRange range,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (kind == ast::GenericParameterSyntaxKind::Type && constant)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            argument->range(),
            "generic type parameter '" + record.parameters[parameterIndex] +
                "' requires a type argument, not a compile-time value");
        return false;
    }
    if (kind == ast::GenericParameterSyntaxKind::TypePack && constant)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            argument->range(),
            "generic type pack '" + record.parameters[parameterIndex] +
                "' cannot contain a compile-time value argument");
        return false;
    }
    if (!constant)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            argument != nullptr ? argument->range() : range,
            constSubject + record.parameters[parameterIndex] +
                "' requires a compile-time value argument");
        return false;
    }
    if (!expectedType.empty()) return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        parameter.constantType != nullptr ? parameter.constantType->range() : range,
        constSubject + record.parameters[parameterIndex] +
            "' has a type that is not an enum or approved structural const type");
    return false;
}

std::optional<bool> ProjectGenericModel::validateStructuralConstGenericArgument(
    const GenericTemplateRecord& record,
    const ast::GenericParameterSyntax& parameter,
    std::size_t parameterIndex,
    ast::TypeSyntax* argument,
    const std::string& expectedType,
    const std::string& argumentType,
    const std::string& argumentValue,
    const std::string& constSubject,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (expectedType.starts_with("enum:"))
    {
        if (argumentType == expectedType) return true;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            argument->range(),
            constSubject + record.parameters[parameterIndex] +
                "' requires a value of enum type '" +
                parameter.constantType->baseName() + "'");
        return false;
    }
    if (expectedType.starts_with("tuple<"))
    {
        if (argumentType == expectedType) return true;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            argument->range(),
            constSubject + record.parameters[parameterIndex] +
                "' requires structural value type '" + expectedType + "'");
        return false;
    }
    if (expectedType != "builtin:bool") return std::nullopt;
    if (argumentType == "builtin:bool" &&
        (argumentValue == "true" || argumentValue == "false"))
        return true;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        argument->range(),
        constSubject + record.parameters[parameterIndex] +
            "' requires a boolean value");
    return false;
}

bool ProjectGenericModel::integralConstGenericValueFits(
    std::uint64_t value,
    unsigned expectedWidth,
    bool expectedSigned,
    bool argumentSigned)
{
    bool fits = expectedWidth != 0;
    if (fits && expectedSigned)
    {
        const std::uint64_t positiveMaximum = expectedWidth == 64
            ? 0x7fffffffffffffffULL
            : ((std::uint64_t{1} << (expectedWidth - 1U)) - 1U);
        if (argumentSigned && static_cast<std::int64_t>(value) < 0)
        {
            const std::int64_t minimum = expectedWidth == 64
                ? (std::numeric_limits<std::int64_t>::min)()
                : -(std::int64_t{1} << (expectedWidth - 1U));
            fits = static_cast<std::int64_t>(value) >= minimum;
        }
        else
            fits = value <= positiveMaximum;
    }
    else if (fits)
    {
        const std::uint64_t maximum = expectedWidth == 64
            ? (std::numeric_limits<std::uint64_t>::max)()
            : ((std::uint64_t{1} << expectedWidth) - 1U);
        fits = (!argumentSigned || static_cast<std::int64_t>(value) >= 0) &&
            value <= maximum;
    }
    return fits;
}

bool ProjectGenericModel::normalizeIntegralConstGenericArgument(
    const GenericTemplateRecord& record,
    const ast::GenericParameterSyntax& parameter,
    std::size_t parameterIndex,
    ast::TypeSyntax* argument,
    const std::string& expectedType,
    const std::string& argumentType,
    const std::string& argumentValue,
    const std::string& constSubject,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (argumentType == "builtin:bool")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            argument->range(),
            constSubject + record.parameters[parameterIndex] +
                "' requires an integral value");
        return false;
    }
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(
        argumentValue.data(), argumentValue.data() + argumentValue.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != argumentValue.data() + argumentValue.size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            argument->range(),
            "const generic argument is not a normalized integral value");
        return false;
    }
    const unsigned expectedWidth = ProjectIntegerEvaluator::integerWidth(expectedType);
    const bool expectedSigned = ProjectIntegerEvaluator::isSignedInteger(expectedType);
    const bool argumentSigned = ProjectIntegerEvaluator::isSignedInteger(argumentType);
    const bool fits = integralConstGenericValueFits(
        value, expectedWidth, expectedSigned, argumentSigned);
    if (!fits)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            argument->range(),
            "const generic argument '" + argumentValue + "' does not fit parameter type '" +
                (parameter.constantType != nullptr
                    ? parameter.constantType->baseName() : "<missing>") + "'");
        return false;
    }
    // The declaration type, not the literal suffix, participates in identity. Therefore
    // `Vector<int, 4>` and `Vector<int, 4u>` share the same specialization when N is uint.
    const std::uint64_t normalized = ProjectIntegerEvaluator::normalize(value, expectedType);
    const std::string normalizedText = expectedSigned &&
            static_cast<std::int64_t>(normalized) < 0
        ? std::to_string(static_cast<std::int64_t>(normalized))
        : std::to_string(normalized);
    argument->setBaseName(language::constGenericArgumentMarker(
        expectedType, normalizedText));
    return true;
}

bool ProjectGenericModel::normalizeGenericArgument(
    const GenericTemplateRecord& record,
    std::span<ast::TypeSyntax* const> arguments,
    std::size_t index,
    const std::optional<std::size_t>& packIndex,
    SourceRange range,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::size_t parameterIndex =
        packIndex.has_value() && index >= *packIndex ? *packIndex : index;
    const ast::GenericParameterSyntaxKind kind =
        parameterIndex < record.parameterSyntaxes.size()
        ? record.parameterSyntaxes[parameterIndex].kind
        : ast::GenericParameterSyntaxKind::Type;
    const ast::GenericParameterSyntax& parameter = record.parameterSyntaxes[parameterIndex];
    const std::string constSubject = kind == ast::GenericParameterSyntaxKind::ConstantPack
        ? "const generic pack '" : "const generic parameter '";
    const std::string expectedType = expectedConstGenericTypeKey(parameter);
    std::string argumentType;
    std::string argumentValue;
    bool constant = decodeConstGenericArgument(
            arguments[index], argumentType, argumentValue) ||
        normalizeConstExpressionArgument(arguments[index], argumentType, argumentValue);
    if (!constant)
        constant = normalizeEnumConstGenericArgument(
            arguments[index], expectedType, argumentType, argumentValue);
    if (kind == ast::GenericParameterSyntaxKind::Type ||
        kind == ast::GenericParameterSyntaxKind::TypePack)
        return !constant || validateConstGenericArgumentKind(
            record, parameter, kind, parameterIndex, arguments[index], constant,
            expectedType, constSubject, range, diagnostics);
    if (!validateConstGenericArgumentKind(
            record, parameter, kind, parameterIndex, arguments[index], constant,
            expectedType, constSubject, range, diagnostics))
        return false;
    const std::optional<bool> structural = validateStructuralConstGenericArgument(
        record, parameter, parameterIndex, arguments[index], expectedType,
        argumentType, argumentValue, constSubject, diagnostics);
    if (structural.has_value()) return *structural;
    return normalizeIntegralConstGenericArgument(
        record, parameter, parameterIndex, arguments[index], expectedType,
        argumentType, argumentValue, constSubject, diagnostics);
}

bool ProjectGenericModel::normalizeGenericArguments(
    const GenericTemplateRecord& record,
    std::span<ast::TypeSyntax* const> arguments,
    SourceRange range,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::optional<std::size_t> packIndex = trailingPackIndex(record);
    if (!validateGenericPackArgumentCount(
            arguments.size(), packIndex, range, diagnostics))
        return false;
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        if (!normalizeGenericArgument(
                record, arguments, index, packIndex, range, diagnostics))
            return false;
    }
    return true;
}

bool ProjectGenericModel::applyExpectedTypeToExpression(
    ProjectSyntaxUnit& unit,
    ast::AstNode* expression,
    const ast::TypeSyntax* expectedType,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics,
    std::size_t inferenceDepth)
{
    if (expression == nullptr || expectedType == nullptr ||
        inferenceDepth > DefaultMaximumInstantiationDepth)
        return false;
#if 0 // INSTRUMENTATION CODE
    const GenericWorkProgress progress(
        telemetry_, "generics.apply-expected-type",
        genericNodeDetail(unit, *expression) + " expected-" + genericTypeDetail(*expectedType) +
            " depth=" + std::to_string(inferenceDepth));
#endif // INSTRUMENTATION CODE

    if (expression->kind() == ast::AstNodeKind::CallExpression)
    {
        if (!expression->typeSyntaxes().empty() || expression->flag()) return true;
        if (!inferGenericCall(
                unit, expression, namespaceName, ownerName, scope, diagnostics, expectedType,
                true, inferenceDepth + 1U))
            return false;
        if (!expression->typeSyntaxes().empty() || expression->flag())
            return processGenericCall(
                unit, expression, namespaceName, ownerName, scope, diagnostics);
        return true;
    }

    if (expression->kind() == ast::AstNodeKind::ConditionalExpression)
    {
        const bool whenTrue = applyExpectedTypeToExpression(
            unit, expression->second(), expectedType, namespaceName, ownerName, scope,
            diagnostics, inferenceDepth + 1U);
        const bool whenFalse = applyExpectedTypeToExpression(
            unit, expression->third(), expectedType, namespaceName, ownerName, scope,
            diagnostics, inferenceDepth + 1U);
        return whenTrue && whenFalse;
    }

    if (expression->kind() == ast::AstNodeKind::TupleExpression && expectedType->isTuple() &&
        expression->children().size() == expectedType->typeArguments().size())
    {
        bool valid = true;
        for (std::size_t index = 0; index < expression->children().size(); ++index)
        {
            ast::AstNode* element = expression->children()[index];
            if (element == nullptr || element->kind() == ast::AstNodeKind::SpreadElementExpression)
            {
                valid = false;
                continue;
            }
            valid = applyExpectedTypeToExpression(
                unit, element, expectedType->typeArguments()[index], namespaceName, ownerName,
                scope, diagnostics, inferenceDepth + 1U) && valid;
        }
        return valid;
    }

    if (expression->kind() == ast::AstNodeKind::MoveExpression)
        return applyExpectedTypeToExpression(
            unit, expression->first(), expectedType, namespaceName, ownerName, scope,
            diagnostics, inferenceDepth + 1U);

    return true;
}

ProjectGenericModel::TemplateState* ProjectGenericModel::resolveLambdaDelegate(
    const ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* delegatePattern)
{
    TemplateState* delegate = nullptr;
    for (TemplateState& state : templateStates_)
    {
        if (state.record.kind != GenericTemplateKind::Delegate ||
            state.record.parameters.size() != delegatePattern->typeArguments().size() ||
            !templateVisible(unit.projectOrdinal(), state.record))
            continue;
        const std::string qualified = qualifiedName(
            state.record.namespaceName, state.record.qualifiedTypeName);
        if (state.record.name != delegatePattern->baseName() &&
            qualified != delegatePattern->baseName())
            continue;
        if (delegate != nullptr) return nullptr;
        delegate = &state;
    }
    return delegate;
}

bool ProjectGenericModel::inferLambdaTypeArguments(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* lambda,
    const ast::TypeSyntax* delegatePattern,
    const GenericTemplateRecord& methodRecord,
    const std::unordered_set<std::string>& methodParameters,
    const GenericLookupScope& scope,
    std::unordered_map<std::string, ast::TypeSyntax*>& inferred)
{
    if (lambda == nullptr || lambda->kind() != ast::AstNodeKind::LambdaExpression ||
        delegatePattern == nullptr || delegatePattern->modifiers().size() != 0U)
        return true;

    TemplateState* delegate = resolveLambdaDelegate(unit, delegatePattern);
    if (delegate == nullptr || delegate->syntax == nullptr) return true;

    std::vector<const ast::AstNode*> delegateParameters;
    for (const ast::AstNode* child : delegate->syntax->children())
        if (child != nullptr && child->kind() == ast::AstNodeKind::ParameterDeclaration)
            delegateParameters.push_back(child);
    if (delegateParameters.size() != lambda->children().size()) return false;

    std::unordered_map<std::string, const ast::TypeSyntax*> delegateArguments;
    for (std::size_t index = 0; index < delegate->record.parameters.size(); ++index)
        delegateArguments.emplace(
            delegate->record.parameters[index], delegatePattern->typeArguments()[index]);

    GenericLookupScope lambdaScope = scope;
    for (std::size_t index = 0; index < lambda->children().size(); ++index)
    {
        const ast::AstNode* lambdaParameter = lambda->children()[index];
        bool conflict = false;
        ast::TypeSyntax* signaturePattern = cloneAliasType(
            unit.arena(), delegateParameters[index]->typeSyntax(), delegateArguments, conflict);
        if (conflict || signaturePattern == nullptr) return false;
        if (lambdaParameter != nullptr && lambdaParameter->typeSyntax() != nullptr)
        {
            if (!inferTypeArguments(
                    unit, signaturePattern, lambdaParameter->typeSyntax(), methodRecord,
                    methodParameters, inferred))
                return false;
            lambdaScope.addValueType(lambdaParameter->text(), lambdaParameter->typeSyntax());
        }
    }

    if (!lambda->flag() && lambda->first() != nullptr && delegate->syntax->typeSyntax() != nullptr)
    {
        std::unordered_map<std::string, const ast::TypeSyntax*> methodArguments;
        for (const auto& [name, type] : inferred) methodArguments.emplace(name, type);
        bool conflict = false;
        ast::TypeSyntax* returnPattern = cloneAliasType(
            unit.arena(), delegate->syntax->typeSyntax(), delegateArguments, conflict);
        if (conflict || returnPattern == nullptr) return false;
        returnPattern = cloneAliasType(
            unit.arena(), returnPattern, methodArguments, conflict);
        ast::TypeSyntax* bodyType = inferExpressionType(unit, lambda->first(), lambdaScope);
        if (!conflict && bodyType != nullptr &&
            !inferTypeArguments(
                unit, returnPattern, bodyType, methodRecord, methodParameters, inferred))
            return false;
    }
    return true;
}
}
