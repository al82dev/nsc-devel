#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "MethodBodyInternals.h"
#include "Internal/NamedArgumentMap.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Frontend/ReadonlyReferenceReturn.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
using lexer::TokenKind;
using method_body_detail::NamedArgumentMap;
using method_body_detail::pointerContract;

namespace
{
constexpr const char* kStructRootFacadeTypeKey = "<struct-root-facade>";
constexpr const char* kEnumRootFacadeTypeKey = "<enum-root-facade>";
constexpr const char* kIteratorFacadeTypeKey = "<iterator-facade>";

NamedArgumentMap mapPositionalArguments(
    std::span<ast::AstNode* const> arguments,
    const ClassMemberSymbol& candidate,
    std::size_t reservedLeadingParameters,
    NamedArgumentMap result)
{
    const std::size_t fixedEnd = result.parameterPackOrdinal.value_or(candidate.parameters.size());
    const std::size_t fixedAvailable = fixedEnd - reservedLeadingParameters;
    if (arguments.size() > fixedAvailable && !result.parameterPackOrdinal.has_value() &&
        !candidate.isVariadic) return result;

    const std::size_t fixedArguments = std::min(arguments.size(), fixedAvailable);
    result.explicitParameterOrdinals.reserve(arguments.size());
    result.parameterPackElements.reserve(arguments.size());
    for (std::size_t index = 0; index < fixedArguments; ++index)
    {
        if (arguments[index] == nullptr) return {};
        result.explicitParameterOrdinals.push_back(reservedLeadingParameters + index);
        result.parameterPackElements.push_back(false);
    }
    for (std::size_t index = fixedArguments; index < arguments.size(); ++index)
    {
        if (arguments[index] == nullptr) return {};
        if (result.parameterPackOrdinal.has_value())
        {
            result.explicitParameterOrdinals.push_back(*result.parameterPackOrdinal);
            result.parameterPackElements.push_back(true);
            result.usesExpandedParameterPack = true;
        }
        else
        {
            result.explicitParameterOrdinals.push_back(
                candidate.parameters.size() + index - fixedArguments);
            result.parameterPackElements.push_back(false);
        }
    }
    for (std::size_t index = reservedLeadingParameters + fixedArguments; index < fixedEnd; ++index)
    {
        if (candidate.parameters[index].defaultValueSyntax == nullptr) return {};
        result.defaultedParameterOrdinals.push_back(index);
    }
    result.synthesizesEmptyParameterPack =
        result.parameterPackOrdinal.has_value() && !result.usesExpandedParameterPack;
    result.valid = true;
    return result;
}

bool mapNamedArgument(
    const ast::AstNode* argument,
    const ClassMemberSymbol& candidate,
    NamedArgumentMap& result,
    std::vector<bool>& assigned,
    std::size_t reservedLeadingParameters,
    std::size_t& nextPositional,
    std::size_t& variadicOrdinal,
    bool& reachedVariadicArguments,
    bool& reachedExpandedParameterPack,
    bool& assignedParameterPackDirectly)
{
    if (argument == nullptr) return false;
    std::optional<std::size_t> parameterOrdinal;
    bool parameterPackElement = false;
    if (!argument->argumentName().empty())
    {
        // The ellipsis has neither a name nor a parameter symbol. Once a positional
        // argument has crossed the fixed prefix, a later named argument would otherwise
        // make the physical C argument order ambiguous.
        if (reachedVariadicArguments || reachedExpandedParameterPack) return false;
        for (std::size_t index = reservedLeadingParameters; index < candidate.parameters.size(); ++index)
        {
            if (candidate.parameters[index].name == argument->argumentName())
            {
                parameterOrdinal = index;
                break;
            }
        }
        if (parameterOrdinal == result.parameterPackOrdinal)
        {
            if (assignedParameterPackDirectly) return false;
            assignedParameterPackDirectly = true;
        }
    }
    else
    {
        while (nextPositional < assigned.size() && assigned[nextPositional]) ++nextPositional;
        if (nextPositional < assigned.size())
        {
            parameterOrdinal = nextPositional;
            if (parameterOrdinal == result.parameterPackOrdinal)
            {
                if (assignedParameterPackDirectly) return false;
                parameterPackElement = true;
                reachedExpandedParameterPack = true;
            }
        }
        else if (candidate.isVariadic)
        {
            parameterOrdinal = candidate.parameters.size() + variadicOrdinal++;
            reachedVariadicArguments = true;
        }
    }
    if (!parameterOrdinal.has_value()) return false;
    if (*parameterOrdinal < assigned.size())
    {
        if (assigned[*parameterOrdinal] && !parameterPackElement) return false;
        if (!parameterPackElement) assigned[*parameterOrdinal] = true;
    }
    result.explicitParameterOrdinals.push_back(*parameterOrdinal);
    result.parameterPackElements.push_back(parameterPackElement);
    result.usesExpandedParameterPack = result.usesExpandedParameterPack || parameterPackElement;
    while (nextPositional < assigned.size() && assigned[nextPositional]) ++nextPositional;
    return true;
}

bool appendDefaultedParameters(
    const ClassMemberSymbol& candidate,
    std::size_t reservedLeadingParameters,
    const std::vector<bool>& assigned,
    bool assignedParameterPackDirectly,
    NamedArgumentMap& result)
{
    for (std::size_t index = reservedLeadingParameters; index < candidate.parameters.size(); ++index)
    {
        if (index == result.parameterPackOrdinal)
        {
            result.synthesizesEmptyParameterPack =
                !result.usesExpandedParameterPack && !assignedParameterPackDirectly;
            continue;
        }
        if (assigned[index]) continue;
        if (candidate.parameters[index].defaultValueSyntax == nullptr) return false;
        result.defaultedParameterOrdinals.push_back(index);
    }
    return true;
}

NamedArgumentMap mapArgumentsToParameters(
    std::span<ast::AstNode* const> arguments,
    const ClassMemberSymbol& candidate,
    std::size_t reservedLeadingParameters = 0)
{
    NamedArgumentMap result;
    if (reservedLeadingParameters > candidate.parameters.size()) return result;
    if (!candidate.parameters.empty() && candidate.parameters.back().isParameterPack)
    {
        result.parameterPackOrdinal = candidate.parameters.size() - 1U;
        if (*result.parameterPackOrdinal < reservedLeadingParameters) return {};
    }

    // Positional calls are overwhelmingly the common case, especially in generated numeric
    // libraries. Reject impossible arities before allocating the per-candidate assignment map.
    // The general path below remains authoritative for named/reordered arguments.
    const bool hasNamedArguments = std::ranges::any_of(arguments, [](const ast::AstNode* argument)
    {
        return argument != nullptr && !argument->argumentName().empty();
    });
    if (!hasNamedArguments)
    {
        return mapPositionalArguments(arguments, candidate, reservedLeadingParameters, std::move(result));
    }

    std::vector<bool> assigned(candidate.parameters.size(), false);
    for (std::size_t index = 0; index < reservedLeadingParameters; ++index) assigned[index] = true;

    std::size_t nextPositional = reservedLeadingParameters;
    std::size_t variadicOrdinal = 0;
    bool reachedVariadicArguments = false;
    bool reachedExpandedParameterPack = false;
    bool assignedParameterPackDirectly = false;
    for (const ast::AstNode* argument : arguments)
    {
        if (!mapNamedArgument(
                argument,
                candidate,
                result,
                assigned,
                reservedLeadingParameters,
                nextPositional,
                variadicOrdinal,
                reachedVariadicArguments,
                reachedExpandedParameterPack,
                assignedParameterPackDirectly)) return result;
    }

    if (!appendDefaultedParameters(
            candidate, reservedLeadingParameters, assigned, assignedParameterPackDirectly, result)) return result;
    result.valid = true;
    return result;
}

bool isErrorType(std::string_view key) noexcept
{
    return key.empty() || key == "<error>";
}

std::optional<std::size_t> snapshotLocalIndex(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t globalLocalOrdinal) noexcept
{
    if (globalLocalOrdinal < snapshot.localBaseOrdinal ||
        globalLocalOrdinal >= snapshot.localCount)
    {
        return std::nullopt;
    }
    const std::size_t index = globalLocalOrdinal - snapshot.localBaseOrdinal;
    return index < snapshot.localFlags.size() ? std::optional<std::size_t>(index) : std::nullopt;
}

std::optional<std::size_t> snapshotStorageIndex(
    const OwnershipFlowSnapshot& snapshot,
    std::size_t storageKey) noexcept
{
    if ((storageKey & 1U) != 0U)
    {
        const std::size_t parameterOrdinal = storageKey / 2U;
        return parameterOrdinal < snapshot.parameterFlags.size()
            ? std::optional<std::size_t>(parameterOrdinal)
            : std::nullopt;
    }
    const std::optional<std::size_t> localIndex = snapshotLocalIndex(
        snapshot,
        storageKey / 2U);
    return localIndex.has_value()
        ? std::optional<std::size_t>(snapshot.parameterFlags.size() + *localIndex)
        : std::nullopt;
}

OwnershipFlowStorageFacts* mutableSnapshotStorageFacts(
    OwnershipFlowSnapshot& snapshot,
    std::size_t storageKey) noexcept
{
    const std::optional<std::size_t> index = snapshotStorageIndex(snapshot, storageKey);
    return index.has_value() && *index < snapshot.storageFacts.size()
        ? &snapshot.storageFacts[*index]
        : nullptr;
}
}


ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeNamespaceFunctionGroup(
    const ast::AstNode* expression,
    const NamespaceFunctionReferenceBinding& reference,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    bool allowMethodGroup)
{
    ExpressionResult result;
    result.methodGroup = true;
    result.valid = allowMethodGroup;
    result.typeKey = "<method-group>";
    result.methodCandidates.reserve(reference.candidateFunctionOrdinals.size());
    for (const std::size_t functionOrdinal : reference.candidateFunctionOrdinals)
    {
        if (functionOrdinal < namespaceCallableAdapters().size())
            result.methodCandidates.push_back(namespaceCallableBase_ + functionOrdinal);
    }
    recordNameBinding(MethodNameBinding{
        MethodNameBindingKind::StaticMethodGroup,
        expression,
        callerCallableOrdinal,
        scopeOrdinal,
        reference.writtenName,
        "<method-group>",
        std::nullopt,
        std::nullopt,
        result.methodCandidates,
        nullptr,
        "",
        std::nullopt,
        false});
    if (!allowMethodGroup)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression != nullptr ? expression->range() : SourceRange{},
            "namespace function group '" + reference.writtenName +
                "' can only be used as a call target or converted to a delegate");
    }
    return result;
}

void ProjectMethodBodyModel::storeNamespaceFunctionCall(
    const ast::AstNode* expression,
    const NamespaceFunctionCallBinding& call)
{
    namespaceFunctionCallBySyntax_[expression] = namespaceFunctionCalls_.size();
    namespaceFunctionCalls_.push_back(call);
}

std::vector<ast::AstNode*> ProjectMethodBodyModel::namespaceFunctionArgumentSyntaxes(
    const ast::AstNode* expression,
    const NamespaceFunctionReferenceBinding& reference,
    const ExpressionResult* forwardedReceiver,
    const ast::AstNode* extraArgumentSyntax,
    std::size_t explicitArgumentCount) const
{
    std::vector<ast::AstNode*> argumentSyntaxes;
    argumentSyntaxes.reserve(explicitArgumentCount + (forwardedReceiver != nullptr ? 1U : 0U));
    if (forwardedReceiver != nullptr)
    {
        argumentSyntaxes.push_back(const_cast<ast::AstNode*>(reference.receiverSyntax));
    }
    if (extraArgumentSyntax != nullptr)
    {
        argumentSyntaxes.push_back(const_cast<ast::AstNode*>(extraArgumentSyntax));
    }
    else
    {
        argumentSyntaxes.insert(
            argumentSyntaxes.end(), expression->children().begin(), expression->children().end());
    }
    return argumentSyntaxes;
}

ProjectMethodBodyModel::ExpressionResult
ProjectMethodBodyModel::bindNamespaceFunctionMethodGroupArgument(
    const ast::AstNode* argument,
    ExpressionResult analyzed,
    const NamespaceFunctionReferenceBinding& reference,
    std::span<ast::AstNode* const> argumentSyntaxes,
    std::size_t argumentOrdinal,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    bool hasForwardedReceiver,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<std::string> targets;
    for (const std::size_t functionOrdinal : reference.candidateFunctionOrdinals)
    {
        if (functionOrdinal >= namespaceCallableAdapters().size()) continue;
        const ClassMemberSymbol& candidate = namespaceCallableAdapters()[functionOrdinal];
        const NamedArgumentMap mapping = mapArgumentsToParameters(argumentSyntaxes, candidate);
        const std::size_t mappedArgumentOrdinal =
            argumentOrdinal + (hasForwardedReceiver ? 1U : 0U);
        if (!mapping.valid || mappedArgumentOrdinal >= mapping.explicitParameterOrdinals.size())
        {
            continue;
        }
        const std::size_t parameterOrdinal =
            mapping.explicitParameterOrdinals[mappedArgumentOrdinal];
        if (parameterOrdinal >= candidate.parameters.size()) continue;
        const std::string& target = candidate.parameters[parameterOrdinal].typeKey;
        if (methodGroupExactlyMatchesDelegate(analyzed, target) &&
            std::ranges::find(targets, target) == targets.end())
        {
            targets.push_back(target);
        }
    }
    if (targets.size() == 1)
    {
        return bindMethodGroupToDelegate(
            argument, analyzed, targets.front(), callerCallableOrdinal, scopeOrdinal, diagnostics);
    }
    diagnostics.report(
        targets.empty() ? diagnostics::DiagnosticId::InvalidConversion
                        : diagnostics::DiagnosticId::InvalidCall,
        diagnostics::DiagnosticSeverity::Error,
        argument->range(),
        targets.empty()
            ? "method group does not match a delegate parameter of namespace function '" +
                reference.writtenName + "'"
            : "method group argument has more than one compatible delegate target in namespace function '" +
                reference.writtenName + "'");
    return analyzed;
}

std::vector<ProjectMethodBodyModel::ExpressionResult>
ProjectMethodBodyModel::analyzeNamespaceFunctionArguments(
    const ast::AstNode* expression,
    const NamespaceFunctionReferenceBinding& reference,
    std::span<ast::AstNode* const> argumentSyntaxes,
    std::size_t explicitArgumentCount,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    const ExpressionResult* forwardedReceiver,
    const ast::AstNode* extraArgumentSyntax,
    bool& argumentsValid,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<ExpressionResult> arguments;
    arguments.reserve(argumentSyntaxes.size());
    if (forwardedReceiver != nullptr)
    {
        ExpressionResult receiverArgument = *forwardedReceiver;
        receiverArgument.typeKey = forwardedReceiver->receiverTypeKey;
        receiverArgument.methodGroup = false;
        receiverArgument.extensionMethodGroup = false;
        receiverArgument.valid = true;
        arguments.push_back(std::move(receiverArgument));
    }
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < explicitArgumentCount; ++argumentOrdinal)
    {
        const ast::AstNode* argument = extraArgumentSyntax != nullptr
            ? extraArgumentSyntax : expression->children()[argumentOrdinal];
        ExpressionResult analyzed = analyzeExpression(
            argument, callerCallableOrdinal, scopeOrdinal, diagnostics, true, false, true);
        if (analyzed.methodGroup)
        {
            analyzed = bindNamespaceFunctionMethodGroupArgument(
                argument, std::move(analyzed), reference, argumentSyntaxes, argumentOrdinal,
                callerCallableOrdinal, scopeOrdinal, forwardedReceiver != nullptr, diagnostics);
        }
        argumentsValid = argumentsValid && analyzed.valid && !analyzed.methodGroup;
        arguments.push_back(std::move(analyzed));
    }
    return arguments;
}

void ProjectMethodBodyModel::normalizeNamespaceFunctionParameterPack(
    const ClassMemberSymbol& candidate,
    std::span<const ExpressionResult> arguments,
    MethodCallCandidate& evaluated) const
{
    if (!evaluated.usesExpandedParameterPack ||
        std::ranges::count(evaluated.parameterPackElements, true) != 1 ||
        !evaluated.parameterPackOrdinal.has_value())
    {
        return;
    }
    const auto packElement = std::ranges::find(evaluated.parameterPackElements, true);
    const std::size_t argumentOrdinal = static_cast<std::size_t>(
        std::distance(evaluated.parameterPackElements.begin(), packElement));
    const MethodParameterSymbol& pack = candidate.parameters[*evaluated.parameterPackOrdinal];
    if (classifyImplicit(
            arguments[argumentOrdinal].typeKey,
            pack.typeKey,
            arguments[argumentOrdinal].integerConstant).exists())
    {
        evaluated.parameterPackElements[argumentOrdinal] = false;
        evaluated.usesExpandedParameterPack = false;
    }
}

std::optional<ProjectImplicitConversion>
ProjectMethodBodyModel::evaluateNamespaceVariadicCandidateArgument(
    std::span<const ExpressionResult> arguments,
    std::size_t argumentOrdinal,
    std::size_t parameterOrdinal,
    const ClassMemberSymbol& candidate,
    MethodCallCandidate& evaluated) const
{
    if (parameterOrdinal < candidate.parameters.size()) return std::nullopt;
    ProjectImplicitConversion conversion =
        classifyCVariadicPromotion(arguments[argumentOrdinal].typeKey);
    if (!conversion.exists() ||
        arguments[argumentOrdinal].argumentPassKind != ParameterPassKind::Value ||
        arguments[argumentOrdinal].ownership != sema::OwnershipKind::None)
    {
        evaluated.viable = false;
        evaluated.invalidVariadicArgumentOrdinal = argumentOrdinal;
    }
    return conversion;
}

void ProjectMethodBodyModel::validateNamespaceFunctionCandidateArgumentOwnership(
    const MethodParameterSymbol& parameter,
    std::span<const ExpressionResult> arguments,
    std::size_t argumentOrdinal,
    bool packElement,
    MethodCallCandidate& evaluated) const
{
    const bool freshStringBorrow = !packElement && parameter.typeKey == "builtin:string" &&
        (parameter.ownership == sema::OwnershipKind::None ||
         parameter.ownership == sema::OwnershipKind::Borrowed) &&
        arguments[argumentOrdinal].ownership == sema::OwnershipKind::Owned &&
        arguments[argumentOrdinal].ownershipTransfer &&
        arguments[argumentOrdinal].ownershipFreshOwner;
    if (freshStringBorrow)
    {
        evaluated.fullExpressionBorrowArgumentOrdinals.push_back(argumentOrdinal);
    }
    else if (classifyOwnershipArgument(
                 packElement ? false : typeKeyIsOwnable(parameter.typeKey),
                 packElement ? sema::OwnershipKind::None : parameter.ownership,
                 arguments[argumentOrdinal].ownership,
                 arguments[argumentOrdinal].ownershipTransfer,
                 arguments[argumentOrdinal].ownershipFreshOwner,
                 arguments[argumentOrdinal].typeKey == "builtin:null") != OwnershipArgumentFit::Ok)
    {
        evaluated.viable = false;
    }
}

ProjectImplicitConversion ProjectMethodBodyModel::evaluateNamespaceFunctionCandidateArgument(
    const ClassMemberSymbol& candidate,
    std::span<const ExpressionResult> arguments,
    std::size_t argumentOrdinal,
    bool hasForwardedReceiver,
    MethodCallCandidate& evaluated) const
{
    const std::size_t parameterOrdinal = evaluated.argumentParameterOrdinals[argumentOrdinal];
    const std::optional<ProjectImplicitConversion> variadic =
        evaluateNamespaceVariadicCandidateArgument(
            arguments, argumentOrdinal, parameterOrdinal, candidate, evaluated);
    if (variadic.has_value()) return *variadic;
    const MethodParameterSymbol& parameter = candidate.parameters[parameterOrdinal];
    std::string conversionTarget = hasForwardedReceiver && argumentOrdinal == 0 &&
            parameter.passKind != ParameterPassKind::Value
        ? parameter.pointeeTypeKey
        : parameter.typeKey;
    const bool packElement = argumentOrdinal < evaluated.parameterPackElements.size() &&
        evaluated.parameterPackElements[argumentOrdinal];
    if (packElement)
    {
        if (parameter.parameterPackElementTypeKey.empty())
        {
            evaluated.viable = false;
        }
        else
        {
            conversionTarget = parameter.parameterPackElementTypeKey;
        }
    }
    ProjectImplicitConversion conversion = classifyImplicit(
        arguments[argumentOrdinal].typeKey,
        conversionTarget,
        arguments[argumentOrdinal].integerConstant);
    // EN: Freeze 3.0 P3: an `in` parameter only reads its argument, so a readonly place (another
    //     `in` parameter, a readonly field, a `ref readonly` result) may be lent to it.
    // ES: Congelamiento 3.0 P3: un parametro `in` solo lee su argumento, asi que se le puede prestar
    //     un lugar readonly (otro parametro `in`, un campo readonly, un resultado `ref readonly`).
    const bool parameterPointeeReadonly =
        parameter.pointerPointeeReadonly || parameter.passKind == ParameterPassKind::In;
    const bool discardsPointeeConst = !packElement &&
        arguments[argumentOrdinal].pointerPointeeReadonly &&
        ProjectTypeConversion::isPointer(parameter.typeKey) &&
        !parameterPointeeReadonly;
    const bool violatesPointerContract = !packElement &&
        ProjectTypeConversion::isPointer(parameter.typeKey) &&
        !language::pointerContractImplicitlyAssignable(
            pointerContract(
                arguments[argumentOrdinal].pointerNullable,
                arguments[argumentOrdinal].pointerPointeeReadonly,
                arguments[argumentOrdinal].pointerHasProvenance,
                arguments[argumentOrdinal].pointerKnownAlignment,
                arguments[argumentOrdinal].pointerAddressSpace),
            pointerContract(
                parameter.pointerNullable,
                parameterPointeeReadonly,
                parameter.pointerHasProvenance,
                parameter.pointerKnownAlignment,
                parameter.pointerAddressSpace));
    const bool implicitExtensionReceiverPass = hasForwardedReceiver && argumentOrdinal == 0;
    if (!conversion.exists() || discardsPointeeConst || violatesPointerContract ||
        (!implicitExtensionReceiverPass &&
         parameter.passKind != arguments[argumentOrdinal].argumentPassKind))
    {
        evaluated.viable = false;
    }
    else if (packElement &&
        (arguments[argumentOrdinal].ownership != sema::OwnershipKind::None ||
         arguments[argumentOrdinal].ownershipTransfer ||
         arguments[argumentOrdinal].ownershipFreshOwner))
    {
        // The temporary span owns no elements and cannot extend an owner lifetime.
        // Move-only and ownership-qualified values must be passed in a prebuilt span.
        evaluated.viable = false;
    }
    else if (!implicitExtensionReceiverPass && conversion.exists() &&
        parameter.passKind == ParameterPassKind::Value &&
        memberModel_->typeIsMoveOnly(conversionTarget) &&
        arguments[argumentOrdinal].addressable &&
        !arguments[argumentOrdinal].ownershipTransfer)
    {
        evaluated.viable = false;
        evaluated.invalidMoveOnlyArgumentOrdinal = argumentOrdinal;
    }
    validateNamespaceFunctionCandidateArgumentOwnership(
        parameter, arguments, argumentOrdinal, packElement, evaluated);
    return conversion;
}

MethodCallCandidate ProjectMethodBodyModel::evaluateNamespaceFunctionCandidate(
    std::size_t functionOrdinal,
    std::span<ast::AstNode* const> argumentSyntaxes,
    std::span<const ExpressionResult> arguments,
    bool hasForwardedReceiver,
    bool& matchingArity) const
{
    const ClassMemberSymbol& candidate = namespaceCallableAdapters()[functionOrdinal];
    MethodCallCandidate evaluated;
    evaluated.memberOrdinal = functionOrdinal;
    evaluated.isVariadicCandidate = candidate.isVariadic;
    const NamedArgumentMap mapping = mapArgumentsToParameters(argumentSyntaxes, candidate);
    if (!mapping.valid) return evaluated;
    matchingArity = true;
    evaluated.viable = true;
    evaluated.argumentParameterOrdinals = mapping.explicitParameterOrdinals;
    evaluated.defaultedParameterOrdinals = mapping.defaultedParameterOrdinals;
    evaluated.parameterPackElements = mapping.parameterPackElements;
    evaluated.parameterPackOrdinal = mapping.parameterPackOrdinal;
    evaluated.usesExpandedParameterPack = mapping.usesExpandedParameterPack;
    evaluated.synthesizesEmptyParameterPack = mapping.synthesizesEmptyParameterPack;
    normalizeNamespaceFunctionParameterPack(candidate, arguments, evaluated);
    evaluated.defaultedParameterCount = mapping.defaultedParameterOrdinals.size();
    for (std::size_t argumentOrdinal = 0; argumentOrdinal < arguments.size(); ++argumentOrdinal)
    {
        evaluated.conversions.push_back(evaluateNamespaceFunctionCandidateArgument(
            candidate, arguments, argumentOrdinal, hasForwardedReceiver, evaluated));
    }
    return evaluated;
}

void ProjectMethodBodyModel::evaluateNamespaceFunctionCandidates(
    const NamespaceFunctionReferenceBinding& reference,
    std::span<ast::AstNode* const> argumentSyntaxes,
    std::span<const ExpressionResult> arguments,
    bool hasForwardedReceiver,
    NamespaceFunctionCallBinding& call,
    bool& matchingArity) const
{
    for (const std::size_t functionOrdinal : reference.candidateFunctionOrdinals)
    {
        if (functionOrdinal >= namespaceCallableAdapters().size()) continue;
        MethodCallCandidate evaluated = evaluateNamespaceFunctionCandidate(
            functionOrdinal, argumentSyntaxes, arguments, hasForwardedReceiver, matchingArity);
        if (evaluated.viable)
        {
            call.viableCandidateFunctionOrdinals.push_back(functionOrdinal);
        }
        call.evaluatedCandidates.push_back(std::move(evaluated));
    }
}

void ProjectMethodBodyModel::reportNoViableNamespaceFunctionCall(
    const ast::AstNode* expression,
    const NamespaceFunctionReferenceBinding& reference,
    std::span<ast::AstNode* const> argumentSyntaxes,
    const NamespaceFunctionCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (const MethodCallCandidate& candidate : call.evaluatedCandidates)
    {
        if (!candidate.invalidMoveOnlyArgumentOrdinal.has_value()) continue;
        const std::size_t ordinal = *candidate.invalidMoveOnlyArgumentOrdinal;
        const ast::AstNode* argument = ordinal < argumentSyntaxes.size()
            ? argumentSyntaxes[ordinal] : expression;
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            argument != nullptr ? argument->range() : expression->range(),
            "move-only argument cannot be copied into a by-value namespace-function parameter; use move(argument)");
        return;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidConversion,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "no viable overload of namespace function '" + reference.writtenName +
            "' accepts the argument types");
}

std::vector<std::size_t> ProjectMethodBodyModel::viableNamespaceFunctionCandidates(
    const NamespaceFunctionCallBinding& call) const
{
    std::vector<std::size_t> viable;
    for (std::size_t index = 0; index < call.evaluatedCandidates.size(); ++index)
    {
        if (call.evaluatedCandidates[index].viable) viable.push_back(index);
    }
    return viable;
}

std::vector<std::size_t> ProjectMethodBodyModel::nonDominatedNamespaceFunctionCandidates(
    const NamespaceFunctionCallBinding& call,
    std::span<const std::size_t> viable) const
{
    std::vector<std::size_t> nonDominated;
    for (const std::size_t candidateIndex : viable)
    {
        const bool dominated = std::ranges::any_of(nonDominated, [&](std::size_t other)
        {
            return betterCandidate(
                call.evaluatedCandidates[other], call.evaluatedCandidates[candidateIndex]);
        });
        if (dominated) continue;
        std::erase_if(nonDominated, [&](std::size_t other)
        {
            return betterCandidate(
                call.evaluatedCandidates[candidateIndex], call.evaluatedCandidates[other]);
        });
        nonDominated.push_back(candidateIndex);
    }
    return nonDominated;
}

std::optional<std::size_t> ProjectMethodBodyModel::selectNamespaceFunctionCandidate(
    const ast::AstNode* expression,
    const NamespaceFunctionReferenceBinding& reference,
    std::span<ast::AstNode* const> argumentSyntaxes,
    std::size_t explicitArgumentCount,
    bool matchingArity,
    NamespaceFunctionCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!matchingArity)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "no overload of namespace function '" + reference.writtenName + "' accepts " +
                std::to_string(explicitArgumentCount) + " argument(s)");
        storeNamespaceFunctionCall(expression, call);
        return std::nullopt;
    }
    const std::vector<std::size_t> viable = viableNamespaceFunctionCandidates(call);
    if (viable.empty())
    {
        reportNoViableNamespaceFunctionCall(
            expression, reference, argumentSyntaxes, call, diagnostics);
        storeNamespaceFunctionCall(expression, call);
        return std::nullopt;
    }
    const std::vector<std::size_t> nonDominated =
        nonDominatedNamespaceFunctionCandidates(call, viable);
    if (nonDominated.size() == 1) return nonDominated.front();
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidCall,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "ambiguous namespace function call to '" + reference.writtenName + "'");
    call.resolution = MethodCallResolutionKind::Ambiguous;
    storeNamespaceFunctionCall(expression, call);
    return std::nullopt;
}

bool ProjectMethodBodyModel::validateNamespaceFunctionDefinition(
    const ast::AstNode* expression,
    std::size_t functionOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (namespaceFunctionModel_ == nullptr ||
        functionOrdinal >= namespaceFunctionModel_->functions().size())
    {
        return true;
    }
    const NamespaceFunctionSymbol& function =
        namespaceFunctionModel_->functions()[functionOrdinal];
    if (function.definitionSyntax != nullptr || function.isExternC || function.isPrecompiled)
    {
        return true;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::MissingFunctionDefinition,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "namespace function '" + function.signatureKey +
            "' is called but has no definition in the project");
    return false;
}

void ProjectMethodBodyModel::checkNamespaceFunctionArgumentPassKinds(
    const ast::AstNode* expression,
    const ClassMemberSymbol& callable,
    std::size_t functionOrdinal,
    const std::vector<ExpressionResult>& arguments,
    const MethodCallCandidate& selected,
    bool hasForwardedReceiver,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!hasForwardedReceiver)
    {
        checkArgumentPassKinds(
            expression, callable.name, namespaceCallableBase_ + functionOrdinal,
            arguments, diagnostics, selected.argumentParameterOrdinals);
        return;
    }
    std::vector<ExpressionResult> explicitArguments(arguments.begin() + 1, arguments.end());
    const std::span<const std::size_t> parameterOrdinals(
        selected.argumentParameterOrdinals.data() + 1,
        selected.argumentParameterOrdinals.size() - 1);
    checkArgumentPassKinds(
        expression, callable.name, namespaceCallableBase_ + functionOrdinal,
        explicitArguments, diagnostics, parameterOrdinals);
}

void ProjectMethodBodyModel::recordNamespaceFunctionBorrows(
    const ast::AstNode* expression,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    std::size_t functionOrdinal,
    std::span<ast::AstNode* const> argumentSyntaxes,
    const std::vector<ExpressionResult>& arguments,
    const MethodCallCandidate& selected)
{
    for (const std::size_t argumentOrdinal : selected.fullExpressionBorrowArgumentOrdinals)
    {
        if (argumentOrdinal >= arguments.size() ||
            argumentOrdinal >= selected.argumentParameterOrdinals.size())
        {
            continue;
        }
        MethodBorrowBinding borrow;
        borrow.kind = MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter;
        borrow.syntax = argumentOrdinal < argumentSyntaxes.size()
            ? argumentSyntaxes[argumentOrdinal] : expression;
        borrow.methodMemberOrdinal = callerCallableOrdinal;
        borrow.scopeOrdinal = scopeOrdinal;
        borrow.targetMemberOrdinal = namespaceCallableBase_ + functionOrdinal;
        borrow.targetParameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
        borrow.typeKey = arguments[argumentOrdinal].typeKey;
        borrowBindings_.push_back(std::move(borrow));
    }
}

void ProjectMethodBodyModel::recordNamespaceFunctionDefaults(
    const ClassMemberSymbol& callable,
    const MethodCallCandidate& selected,
    NamespaceFunctionCallBinding& call) const
{
    for (const std::size_t parameterOrdinal : selected.defaultedParameterOrdinals)
    {
        call.selectedDefaultArguments.push_back(
            callable.parameters[parameterOrdinal].defaultValueSyntax);
        call.selectedDefaultArgumentParameterOrdinals.push_back(parameterOrdinal);
    }
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::completeNamespaceFunctionCall(
    const ast::AstNode* expression,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    std::size_t functionOrdinal,
    std::span<ast::AstNode* const> argumentSyntaxes,
    const std::vector<ExpressionResult>& arguments,
    const MethodCallCandidate& selected,
    NamespaceFunctionCallBinding& call,
    bool hasForwardedReceiver,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const ClassMemberSymbol& callable = namespaceCallableAdapters()[functionOrdinal];
    checkNamespaceFunctionArgumentPassKinds(
        expression, callable, functionOrdinal, arguments,
        selected, hasForwardedReceiver, diagnostics);
    call.resolution = MethodCallResolutionKind::Resolved;
    call.selectedFunctionOrdinal = functionOrdinal;
    call.selectedConversions = selected.conversions;
    call.selectedArgumentParameterOrdinals = selected.argumentParameterOrdinals;
    call.selectedArgumentSyntaxes.assign(argumentSyntaxes.begin(), argumentSyntaxes.end());
    call.selectedParameterPackElements = selected.parameterPackElements;
    call.selectedParameterPackOrdinal = selected.parameterPackOrdinal;
    call.selectedUsesExpandedParameterPack = selected.usesExpandedParameterPack;
    call.selectedSynthesizesEmptyParameterPack = selected.synthesizesEmptyParameterPack;
    recordNamespaceFunctionBorrows(
        expression, callerCallableOrdinal, scopeOrdinal, functionOrdinal,
        argumentSyntaxes, arguments, selected);
    recordNamespaceFunctionDefaults(callable, selected, call);
    call.resultTypeKey = callable.declaredTypeKey;
    ExpressionResult result;
    result.typeKey = callable.declaredTypeKey;
    result.pointerPointeeReadonly = callable.declaredPointerPointeeReadonly;
    result.pointerNullable = callable.declaredPointerNullable;
    result.pointerHasProvenance = callable.declaredPointerHasProvenance;
    result.pointerKnownAlignment = callable.declaredPointerKnownAlignment;
    result.pointerAddressSpace = callable.declaredPointerAddressSpace;
    result.valid = true;
    result.assignable = callable.isReferenceReturn;
    result.addressable = callable.isReferenceReturn;
    result.ownership = callable.declaredOwnership;
    result.ownershipTransfer = callable.declaredOwnership == sema::OwnershipKind::Owned;
    result.ownershipFreshOwner = result.ownershipTransfer;
    if (result.ownership != sema::OwnershipKind::None)
    {
        result.objectLifetimeOwnership = result.ownership;
        result.storageOwnership = result.ownership;
    }
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeNamespaceFunctionCall(
    const ast::AstNode* expression,
    const NamespaceFunctionReferenceBinding& reference,
    std::size_t callerCallableOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics,
    const ExpressionResult* forwardedReceiver,
    const ast::AstNode* extraArgumentSyntax)
{
    ExpressionResult result;
    NamespaceFunctionCallBinding call;
    call.syntax = expression;
    call.callerCallableOrdinal = callerCallableOrdinal;
    call.scopeOrdinal = scopeOrdinal;
    call.writtenName = reference.writtenName;
    call.forwardedReceiver = forwardedReceiver != nullptr;
    call.receiverSyntax = forwardedReceiver != nullptr ? reference.receiverSyntax : nullptr;
    call.candidateFunctionOrdinals = reference.candidateFunctionOrdinals;

    // NS304-T08-F02: a synthetic call (e.g. an extension-property assignment lowered to its
    // `set_` accessor) may carry one explicit argument that is NOT a child of `expression`.
    // `extraArgumentSyntax` replaces the expression children as the explicit argument list.
    const bool usesExtraArgument = extraArgumentSyntax != nullptr;
    const std::size_t explicitArgumentCount =
        usesExtraArgument ? 1U : expression->children().size();
    std::vector<ast::AstNode*> argumentSyntaxes = namespaceFunctionArgumentSyntaxes(
        expression, reference, forwardedReceiver, extraArgumentSyntax, explicitArgumentCount);
    bool argumentsValid = true;
    std::vector<ExpressionResult> arguments = analyzeNamespaceFunctionArguments(
        expression, reference, argumentSyntaxes, explicitArgumentCount,
        callerCallableOrdinal, scopeOrdinal, forwardedReceiver,
        extraArgumentSyntax, argumentsValid, diagnostics);
    if (!argumentsValid)
    {
        storeNamespaceFunctionCall(expression, call);
        return result;
    }

    bool matchingArity = false;
    evaluateNamespaceFunctionCandidates(
        reference, argumentSyntaxes, arguments, forwardedReceiver != nullptr,
        call, matchingArity);
    const std::optional<std::size_t> selectedIndex = selectNamespaceFunctionCandidate(
        expression, reference, argumentSyntaxes, explicitArgumentCount,
        matchingArity, call, diagnostics);
    if (!selectedIndex.has_value()) return result;
    const MethodCallCandidate& selected = call.evaluatedCandidates[*selectedIndex];
    const std::size_t functionOrdinal = selected.memberOrdinal;
    if (!validateNamespaceFunctionDefinition(
            expression, functionOrdinal, diagnostics))
    {
        storeNamespaceFunctionCall(expression, call);
        return result;
    }
    result = completeNamespaceFunctionCall(
        expression, callerCallableOrdinal, scopeOrdinal, functionOrdinal,
        argumentSyntaxes, arguments, selected, call,
        forwardedReceiver != nullptr, diagnostics);
    storeNamespaceFunctionCall(expression, call);
    return result;
}

void ProjectMethodBodyModel::recognizeCoreStringOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.String" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> coreKind;
        if (selectedMember.name == "Concat" && selectedMember.declaredTypeKey == "builtin:string" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:string" &&
            selectedMember.parameters[1].typeKey == "builtin:string")
        {
            coreKind = MethodCoreOperationKind::StringConcat;
        }
        else if (selectedMember.name == "Equals" && selectedMember.declaredTypeKey == "builtin:bool" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:string" &&
            selectedMember.parameters[1].typeKey == "builtin:string")
        {
            coreKind = MethodCoreOperationKind::StringEquals;
        }
        else if (selectedMember.name == "Substring" && selectedMember.declaredTypeKey == "builtin:string" &&
            selectedMember.parameters.size() == 3 &&
            selectedMember.parameters[0].typeKey == "builtin:string" &&
            selectedMember.parameters[1].typeKey == "builtin:int" &&
            selectedMember.parameters[2].typeKey == "builtin:int")
        {
            coreKind = MethodCoreOperationKind::StringSubstring;
        }
        else if (selectedMember.name == "FromCharArray" &&
            selectedMember.declaredTypeKey == "builtin:string" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:char[]")
        {
            coreKind = MethodCoreOperationKind::StringFromCharArray;
        }
        // SHR-010: `String.CharacterPointer(texto, indice)` -> `char*`. El
        // reconocimiento es por firma EXACTA, igual que las cuatro de arriba: clase
        // cualificada, estatico, tipo de retorno y tipos de parametros. Un metodo que se
        // llame igual con otra firma no es esta operacion y se compila como codigo
        // normal.
        else if (selectedMember.name == "CharacterPointer" &&
            selectedMember.declaredTypeKey == "builtin:char*" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:string" &&
            selectedMember.parameters[1].typeKey == "builtin:int")
        {
            coreKind = MethodCoreOperationKind::StringCharacterPointer;
        }
        if (coreKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *coreKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreFloat128Operations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Entrega 228: SIMD por familia justificada, reconocida por identidad y firma exactas.
    // Comparar los parametros con el tipo de retorno evita acoplar esta regla a la
    // codificacion interna del type-key del proyecto que contiene NativeSharp.Core.
    const std::string compactFloatVectorName =
        declaringType.identity.fullyQualifiedName();
    if ((compactFloatVectorName == "NativeSharp.Core.Numerics.Float32x2" ||
            compactFloatVectorName == "NativeSharp.Core.Numerics.Float32x3" ||
            compactFloatVectorName == "NativeSharp.Core.Numerics.Float32x4") &&
        selectedMember.isStatic &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
        selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey)
    {
        std::optional<MethodCoreOperationKind> simdKind;
        if (selectedMember.name == "Add")
            simdKind = MethodCoreOperationKind::SimdFloat32x4Add;
        else if (selectedMember.name == "Subtract")
            simdKind = MethodCoreOperationKind::SimdFloat32x4Subtract;
        else if (selectedMember.name == "Multiply")
            simdKind = MethodCoreOperationKind::SimdFloat32x4Multiply;
        if (simdKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *simdKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
        }
    }
    if (compactFloatVectorName == "NativeSharp.Core.Numerics.Float32x4" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> shuffleKind;
        if (selectedMember.name == "SwizzleWzyx" && selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey)
            shuffleKind = MethodCoreOperationKind::SimdSwizzleWzyx;
        else if (selectedMember.name == "ShuffleLowHigh" && selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
            selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey)
            shuffleKind = MethodCoreOperationKind::SimdShuffleLowHigh;
        if (shuffleKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *shuffleKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
        }
    }
    if (compactFloatVectorName == "NativeSharp.Core.Numerics.Float32x4" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> mathKind;
        if (selectedMember.name == "Sqrt" && selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey)
            mathKind = MethodCoreOperationKind::SimdSqrt;
        else if (selectedMember.name == "ReciprocalSqrt" && selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey)
            mathKind = MethodCoreOperationKind::SimdReciprocalSqrt;
        else if (selectedMember.name == "FusedMultiplyAdd" && selectedMember.parameters.size() == 3 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
            selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey &&
            selectedMember.parameters[2].typeKey == selectedMember.declaredTypeKey)
            mathKind = MethodCoreOperationKind::SimdFusedMultiplyAdd;
        if (mathKind.has_value())
            coreBindings_.push_back(MethodCoreBinding{*mathKind, expression, methodMemberOrdinal,
                scopeOrdinal, selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
    }
}

void ProjectMethodBodyModel::recognizeCoreInt128Operations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Item 347: the two public signed 128-bit integer carriers share one intrinsic
    // family. Identity and exact self/self -> self signatures prevent user methods with
    // coincidentally matching names from acquiring compiler semantics.
    const std::string integerVectorName = declaringType.identity.fullyQualifiedName();
    if ((integerVectorName == "NativeSharp.Core.Numerics.Int32x4" ||
            integerVectorName == "NativeSharp.Core.Numerics.Int64x2") &&
        selectedMember.isStatic && selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
        selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey)
    {
        std::optional<MethodCoreOperationKind> simdKind;
        if (selectedMember.name == "Add")
            simdKind = MethodCoreOperationKind::SimdInt128Add;
        else if (selectedMember.name == "Subtract")
            simdKind = MethodCoreOperationKind::SimdInt128Subtract;
        else if (selectedMember.name == "Multiply")
            simdKind = MethodCoreOperationKind::SimdInt128Multiply;
        if (simdKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *simdKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
        }
    }
    if (integerVectorName == "NativeSharp.Core.Numerics.Int32x4" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> memoryKind;
        if (selectedMember.name == "LoadAligned" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey + "*")
        {
            memoryKind = MethodCoreOperationKind::SimdLoadAligned128;
        }
        else if (selectedMember.name == "StoreAligned" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey ==
                selectedMember.parameters[1].typeKey + "*")
        {
            memoryKind = MethodCoreOperationKind::SimdStoreAligned128;
        }
        else if (selectedMember.name == "LoadUnaligned" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey + "*")
        {
            memoryKind = MethodCoreOperationKind::SimdLoadUnaligned128;
        }
        else if (selectedMember.name == "StoreUnaligned" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey ==
                selectedMember.parameters[1].typeKey + "*")
        {
            memoryKind = MethodCoreOperationKind::SimdStoreUnaligned128;
        }
        if (memoryKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *memoryKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreHalfOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Item 354: Half is a two-byte ABI carrier. Only these exact conversion signatures
    // acquire native IEEE binary16 semantics.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Numerics.Half" &&
        selectedMember.isStatic && selectedMember.parameters.size() == 1)
    {
        std::optional<MethodCoreOperationKind> halfKind;
        if (selectedMember.name == "FromFloat" &&
            selectedMember.parameters[0].typeKey == "builtin:float" &&
            selectedMember.declaredTypeKey != "builtin:float")
            halfKind = MethodCoreOperationKind::HalfFromFloat;
        else if (selectedMember.name == "ToFloat" &&
            selectedMember.declaredTypeKey == "builtin:float" &&
            selectedMember.parameters[0].typeKey != "builtin:float")
            halfKind = MethodCoreOperationKind::HalfToFloat;
        if (halfKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *halfKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey, selectedMember.parameters[0].typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreScalarOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    recognizeCoreHalfOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Numerics.BitOperations" &&
        selectedMember.isStatic && selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
        (selectedMember.declaredTypeKey == "builtin:uint" ||
            selectedMember.declaredTypeKey == "builtin:ulong"))
    {
        std::optional<MethodCoreOperationKind> bitKind;
        if (selectedMember.name == "PopCount")
            bitKind = MethodCoreOperationKind::BitPopulationCount;
        else if (selectedMember.name == "LeadingZeroCount")
            bitKind = MethodCoreOperationKind::BitLeadingZeroCount;
        else if (selectedMember.name == "TrailingZeroCount")
            bitKind = MethodCoreOperationKind::BitTrailingZeroCount;
        if (bitKind.has_value())
            coreBindings_.push_back(MethodCoreBinding{*bitKind, expression, methodMemberOrdinal,
                scopeOrdinal, selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
    }
    if (declaringType.identity.fullyQualifiedName() ==
            "NativeSharp.Core.Runtime.CompilerServices.HardwareIntrinsics" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> hardwareKind;
        if (selectedMember.name == "PrefetchRead" && selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:void*" &&
            selectedMember.declaredTypeKey == "builtin:void")
            hardwareKind = MethodCoreOperationKind::CpuPrefetchRead;
        else if (selectedMember.name == "Pause" && selectedMember.parameters.empty() &&
            selectedMember.declaredTypeKey == "builtin:void")
            hardwareKind = MethodCoreOperationKind::CpuPause;
        else if (selectedMember.name == "ReadTimestamp" && selectedMember.parameters.empty() &&
            selectedMember.declaredTypeKey == "builtin:ulong")
            hardwareKind = MethodCoreOperationKind::CpuReadTimestamp;
        if (hardwareKind.has_value())
            coreBindings_.push_back(MethodCoreBinding{*hardwareKind, expression, methodMemberOrdinal,
                scopeOrdinal, selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters[0].typeKey,
                false, std::nullopt, std::nullopt});
    }
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Numerics.BitOperations" &&
        selectedMember.isStatic &&
        (selectedMember.declaredTypeKey == "builtin:uint" || selectedMember.declaredTypeKey == "builtin:ulong"))
    {
        std::optional<MethodCoreOperationKind> rotateKind;
        if ((selectedMember.name == "RotateLeft" || selectedMember.name == "RotateRight") &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
            selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey)
            rotateKind = selectedMember.name == "RotateLeft"
                ? MethodCoreOperationKind::BitRotateLeft : MethodCoreOperationKind::BitRotateRight;
        else if (selectedMember.name == "ReverseEndianness" && selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey)
            rotateKind = MethodCoreOperationKind::BitByteSwap;
        if (rotateKind.has_value())
            coreBindings_.push_back(MethodCoreBinding{*rotateKind, expression, methodMemberOrdinal,
                scopeOrdinal, selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
    }
}

void ProjectMethodBodyModel::recognizeCoreVector256Operations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Item 348: the 256-bit floating family uses the same exact-signature gate as the
    // earlier SIMD families while retaining distinct MIR operations for FP semantics.
    const std::string floatingVector256Name =
        declaringType.identity.fullyQualifiedName();
    if ((floatingVector256Name == "NativeSharp.Core.Numerics.Float32x8" ||
            floatingVector256Name == "NativeSharp.Core.Numerics.Float64x4") &&
        selectedMember.isStatic && selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
        selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey)
    {
        std::optional<MethodCoreOperationKind> simdKind;
        if (selectedMember.name == "Add")
            simdKind = MethodCoreOperationKind::SimdFloat256Add;
        else if (selectedMember.name == "Subtract")
            simdKind = MethodCoreOperationKind::SimdFloat256Subtract;
        else if (selectedMember.name == "Multiply")
            simdKind = MethodCoreOperationKind::SimdFloat256Multiply;
        if (simdKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *simdKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
        }
    }
    const std::string integerVector256Name =
        declaringType.identity.fullyQualifiedName();
    if ((integerVector256Name == "NativeSharp.Core.Numerics.Int32x8" ||
            integerVector256Name == "NativeSharp.Core.Numerics.Int64x4") &&
        selectedMember.isStatic && selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == selectedMember.declaredTypeKey &&
        selectedMember.parameters[1].typeKey == selectedMember.declaredTypeKey)
    {
        std::optional<MethodCoreOperationKind> simdKind;
        if (selectedMember.name == "Add")
            simdKind = MethodCoreOperationKind::SimdInt256Add;
        else if (selectedMember.name == "Subtract")
            simdKind = MethodCoreOperationKind::SimdInt256Subtract;
        else if (selectedMember.name == "Multiply")
            simdKind = MethodCoreOperationKind::SimdInt256Multiply;
        if (simdKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *simdKind, expression, methodMemberOrdinal, scopeOrdinal,
                selectedMember.declaredTypeKey, selectedMember.declaredTypeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreConsoleRuntimeOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // SHR-022. Las cuatro salidas de consola. La firma es identica en las cuatro
    // --estatica, void, un solo `string`-- asi que lo unico que las separa es el nombre, y
    // por eso la tabla es de nombre a operacion en vez de cuatro `if` copiados.
    //
    // POR QUE `ErrorWriteLine` Y NO `Console.Error.WriteLine`. Lo segundo se lee mejor y
    // pide un objeto: `Error` tendria que ser una propiedad que devuelve un TextWriter, con
    // despacho de instancia. TextWriter es SHR-020 y vive en el Bloque 5. Adelantarlo para
    // ganar un punto en el nombre habria arrastrado un tipo entero y su modelo de despacho
    // dentro del Bloque 2. Cuando TextWriter exista, estas cuatro siguen siendo las
    // primitivas de debajo y la forma con objeto se construye encima sin tocar el backend.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Console" &&
        selectedMember.isStatic &&
        selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters.front().typeKey == "builtin:string")
    {
        std::optional<MethodCoreOperationKind> consoleKind;
        if (selectedMember.name == "WriteLine")
            consoleKind = MethodCoreOperationKind::ConsoleWriteLine;
        else if (selectedMember.name == "Write")
            consoleKind = MethodCoreOperationKind::ConsoleWrite;
        else if (selectedMember.name == "ErrorWriteLine")
            consoleKind = MethodCoreOperationKind::ConsoleErrorWriteLine;
        else if (selectedMember.name == "ErrorWrite")
            consoleKind = MethodCoreOperationKind::ConsoleErrorWrite;
        if (consoleKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *consoleKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                "builtin:void",
                "builtin:string",
                false, std::nullopt, std::nullopt});
        }
    }
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Console" &&
        selectedMember.isStatic && selectedMember.name == "ReadLine" &&
        selectedMember.declaredTypeKey == "builtin:string" &&
        selectedMember.declaredOwnership == sema::OwnershipKind::Owned &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters.front().pointeeTypeKey == "builtin:int" &&
        selectedMember.parameters.front().passKind == ParameterPassKind::Out)
    {
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::ConsoleReadLine,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            "builtin:string",
            "builtin:int",
            false, std::nullopt, std::nullopt});
    }
    // C8 "type root" -- NativeSharp.Core.RuntimeHelpers' two primitives. Same recognition
    // shape as every core operation before them (exact class, exact static method name,
    // exact signature): read a type's name out of its runtime descriptor, and turn a
    // reference into an integer. Neither can be written in NativeSharp, which is the
    // whole reason they are core operations rather than library code -- see Object.ns.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.RuntimeHelpers" &&
        selectedMember.isStatic && selectedMember.parameters.size() == 1)
    {
        std::optional<MethodCoreOperationKind> helperKind;
        if (selectedMember.name == "TypeName" &&
            selectedMember.declaredTypeKey == "builtin:string")
        {
            helperKind = MethodCoreOperationKind::ObjectTypeName;
        }
        else if (selectedMember.name == "ReferenceHash" &&
            selectedMember.declaredTypeKey == "builtin:int")
        {
            helperKind = MethodCoreOperationKind::ObjectReferenceHash;
        }
        if (helperKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *helperKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreClockOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Stage 53.1 "Time" -- NativeSharp.Core.Diagnostics.NativeClock's three raw
    // zero-argument Win32 wrappers. Same recognition shape as ConsoleWriteLine just
    // above (exact class, exact static method name, exact signature), just three
    // sibling methods instead of one. See NativeClock.ns for the full rationale.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Diagnostics.NativeClock" &&
        selectedMember.isStatic &&
        selectedMember.declaredTypeKey == "builtin:long" &&
        selectedMember.parameters.empty())
    {
        std::optional<MethodCoreOperationKind> clockKind;
        if (selectedMember.name == "QueryCounterRaw")
        {
            clockKind = MethodCoreOperationKind::ClockQueryCounter;
        }
        else if (selectedMember.name == "QueryFrequencyRaw")
        {
            clockKind = MethodCoreOperationKind::ClockQueryFrequency;
        }
        else if (selectedMember.name == "QuerySystemFileTimeRaw")
        {
            clockKind = MethodCoreOperationKind::ClockQuerySystemFileTime;
        }
        if (clockKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *clockKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                "builtin:long",
                std::string{},
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreClockEnvironmentOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    recognizeCoreClockOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    // Stage 53.2 "Environment" -- NativeSharp.Core.Environment's three raw Win32
    // wrappers. TickCount64/ProcessorCount reuse the exact zero-argument recognition
    // shape NativeClock established just above (only the declared return type differs
    // between the two); Exit reuses the one-argument/void shape Console.WriteLine
    // established further above. See Environment.ns for why GetEnvironmentVariable/
    // SetEnvironmentVariable are NOT recognized here -- that needs dynamic string
    // allocation from the runtime, a genuinely new kind of marshaling deferred to its
    // own round (53.2b).
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Environment" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> environmentKind;
        if (selectedMember.name == "TickCount64" &&
            selectedMember.declaredTypeKey == "builtin:long" &&
            selectedMember.parameters.empty())
        {
            environmentKind = MethodCoreOperationKind::EnvironmentTickCount64;
        }
        else if (selectedMember.name == "ProcessorCount" &&
            selectedMember.declaredTypeKey == "builtin:int" &&
            selectedMember.parameters.empty())
        {
            environmentKind = MethodCoreOperationKind::EnvironmentProcessorCount;
        }
        else if (selectedMember.name == "Exit" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:int")
        {
            environmentKind = MethodCoreOperationKind::EnvironmentExit;
        }
        // Stage 53.2b "Environment (variables)" -- GetEnvironmentVariable/
        // SetEnvironmentVariable, the piece Stage 53.2's first round deliberately left
        // out (see Environment.ns). Recognized the same way as every other core
        // operation: exact class, exact static method name, exact signature.
        else if (selectedMember.name == "GetEnvironmentVariable" &&
            selectedMember.declaredTypeKey == "builtin:string" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:string")
        {
            environmentKind = MethodCoreOperationKind::EnvironmentGetVariable;
        }
        else if (selectedMember.name == "SetEnvironmentVariable" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:string" &&
            selectedMember.parameters[1].typeKey == "builtin:string")
        {
            environmentKind = MethodCoreOperationKind::EnvironmentSetVariable;
        }
        // Stage 53.5 "Command-line arguments" -- GetCommandLineArgCount (zero-argument,
        // same shape as TickCount64/ProcessorCount above) and GetCommandLineArgs (one
        // builtin:string[] argument -- the "buffer provided by the caller" pattern, see
        // Environment.ns's header comment).
        // Bloque 4, fase A. CurrentDirectory: cero argumentos, builtin:string fuera --
        // la misma forma de cero operandos que TickCount64/ProcessorCount, con el tipo de
        // salida de GetEnvironmentVariable. Cierra `current_path`, la ultima de las siete
        // operaciones de filesystem que el doc 81 midio.
        else if (selectedMember.name == "CurrentDirectory" &&
            selectedMember.declaredTypeKey == "builtin:string" &&
            selectedMember.parameters.empty())
        {
            environmentKind = MethodCoreOperationKind::EnvironmentCurrentDirectory;
        }
        else if (selectedMember.name == "GetCommandLineArgCount" &&
            selectedMember.declaredTypeKey == "builtin:int" &&
            selectedMember.parameters.empty())
        {
            environmentKind = MethodCoreOperationKind::EnvironmentGetCommandLineArgCount;
        }
        else if (selectedMember.name == "GetCommandLineArgs" &&
            selectedMember.declaredTypeKey == "builtin:int" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:string[]")
        {
            environmentKind = MethodCoreOperationKind::EnvironmentGetCommandLineArgs;
        }
        if (environmentKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *environmentKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty()
                    ? std::string{}
                    : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreFileDirectoryOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Stage 53.3 "Filesystem/Paths" -- NativeSharp.Core.IO.File.Exists/Delete and
    // NativeSharp.Core.IO.Directory.Exists/CreateDirectory. Same one-argument recognition
    // shape as Environment.GetEnvironmentVariable (Exists) or Environment.Exit
    // (Delete/CreateDirectory, one argument + void return) above -- two classes instead
    // of one, so this is its own "if", not folded into the Environment block.
    if ((declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.IO.File" ||
            declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.IO.Directory") &&
        selectedMember.isStatic)
    {
        const bool isFile =
            declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.IO.File";
        std::optional<MethodCoreOperationKind> ioKind =
            basicFileDirectoryOperation(selectedMember, isFile);
        if (!ioKind.has_value()) ioKind = fileContentOperation(selectedMember, isFile);
        if (!ioKind.has_value()) ioKind = directoryEnumerationOperation(selectedMember, isFile);
        if (ioKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *ioKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

std::optional<MethodCoreOperationKind> ProjectMethodBodyModel::basicFileDirectoryOperation(
    const ClassMemberSymbol& selectedMember,
    bool isFile)
{
    if (selectedMember.name == "Exists" &&
        selectedMember.declaredTypeKey == "builtin:bool" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return isFile ? MethodCoreOperationKind::FileExists
                      : MethodCoreOperationKind::DirectoryExists;
    if (isFile && selectedMember.name == "Delete" &&
        selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::FileDelete;
    if (!isFile && selectedMember.name == "CreateDirectory" &&
        selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::DirectoryCreate;
    // Stage 53.3b "Filesystem/Paths (contenido de archivo)" -- ReadAllText/
    // WriteAllText/AppendAllText, File-only (Directory has no equivalent). Same
    // one-argument-returns-string shape as GetEnvironmentVariable (ReadAllText), and
    // the same two-argument-void shape as SetEnvironmentVariable
    // (WriteAllText/AppendAllText).
    if (isFile && selectedMember.name == "ReadAllText" &&
        selectedMember.declaredTypeKey == "builtin:string" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::FileReadAllText;
    if (isFile && selectedMember.name == "WriteAllText" &&
        selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:string")
        return MethodCoreOperationKind::FileWriteAllText;
    return std::nullopt;
}

std::optional<MethodCoreOperationKind> ProjectMethodBodyModel::fileContentOperation(
    const ClassMemberSymbol& selectedMember,
    bool isFile)
{
    // Bloque 4, fase A -- SHR-028. ReadAllBytes: misma forma de un argumento que
    // ReadAllText, y el mismo tipo de salida en memoria: un `byte[]` y un `string`
    // comparten layout, { i64 longitud; datos }.
    if (isFile && selectedMember.name == "ReadAllBytes" &&
        selectedMember.declaredTypeKey == "builtin:byte[]" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::FileReadAllBytes;
    // ReadBytes: ruta + arreglo del llamador, cuantos bytes se leyeron fuera. El
    // patron "buffer provisto por el llamador" de Directory.GetFiles, que es el que
    // no asigna y por tanto el que usara el driver.
    if (isFile && selectedMember.name == "ReadBytes" &&
        selectedMember.declaredTypeKey == "builtin:int" &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:byte[]")
        return MethodCoreOperationKind::FileReadBytes;
    // SHR-029. WriteAllBytes: ruta + arreglo + CUANTOS bytes escribir. El contador es
    // explicito y no la longitud del arreglo porque quien construye un buffer casi
    // siempre lo tiene mas grande que lo que ha llenado.
    if (isFile && selectedMember.name == "WriteAllBytes" &&
        selectedMember.declaredTypeKey == "builtin:bool" &&
        selectedMember.parameters.size() == 3 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:byte[]" &&
        selectedMember.parameters[2].typeKey == "builtin:int")
        return MethodCoreOperationKind::FileWriteAllBytes;
    // Bloque 4, fase A -- SHR-030. `File.GetSize(string)` devuelve builtin:long.
    // Misma forma de un argumento que ReadAllText, otro tipo de salida.
    if (isFile && selectedMember.name == "GetSize" &&
        selectedMember.declaredTypeKey == "builtin:long" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::FileGetSize;
    if (isFile && selectedMember.name == "AppendAllText" &&
        selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:string")
        return MethodCoreOperationKind::FileAppendAllText;
    return std::nullopt;
}

std::optional<MethodCoreOperationKind> ProjectMethodBodyModel::directoryEnumerationOperation(
    const ClassMemberSymbol& selectedMember,
    bool isFile)
{
    // Stage 53.4 "Filesystem/Paths (cierre)" -- Directory.Delete, calco exacto de
    // File.Delete arriba (mismo one-argument/void shape), solo del lado !isFile.
    if (!isFile && selectedMember.name == "Delete" &&
        selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::DirectoryDelete;
    // Stage 53.6 "Enumeracion de directorios" -- GetFileCount/GetDirectoryCount (un
    // builtin:string entra, builtin:int sale) y GetFiles/GetDirectories (un
    // builtin:string + un builtin:string[] entran -- la "buffer provisto por el
    // caller" que GetCommandLineArgs establecio -- builtin:int sale). Ver el
    // comentario de cabecera de Directory.ns para el diseno completo.
    if (!isFile && selectedMember.name == "GetFileCount" &&
        selectedMember.declaredTypeKey == "builtin:int" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::DirectoryGetFileCount;
    if (!isFile && selectedMember.name == "GetFiles" &&
        selectedMember.declaredTypeKey == "builtin:int" &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:string[]")
        return MethodCoreOperationKind::DirectoryGetFiles;
    if (!isFile && selectedMember.name == "GetDirectoryCount" &&
        selectedMember.declaredTypeKey == "builtin:int" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
        return MethodCoreOperationKind::DirectoryGetDirectoryCount;
    if (!isFile && selectedMember.name == "GetDirectories" &&
        selectedMember.declaredTypeKey == "builtin:int" &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:string[]")
        return MethodCoreOperationKind::DirectoryGetDirectories;
    // Pre-Stage-56 "utilidades basicas" -- File.CopyRaw/MoveRaw, Directory.MoveRaw.
    // CopyRaw is File-only, three operands (source, dest, overwrite) -- joins
    // StringSubstring in the HIR bucket's expected=3U list. MoveRaw exists on BOTH
    // File and Directory (Win32's MoveFileA works identically for either -- see
    // buildMoveFile in WindowsRuntimeBuilder.cpp, the first runtime hook shared by a
    // File AND a Directory core operation, same "shared wrapper" precedent
    // Semaphore.Wait/Event.Wait established in Stage 54.5), two operands each, falls
    // through to the default expected=2U bucket.
    if (isFile && selectedMember.name == "CopyRaw" &&
        selectedMember.declaredTypeKey == "builtin:bool" &&
        selectedMember.parameters.size() == 3 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:string" &&
        selectedMember.parameters[2].typeKey == "builtin:bool")
        return MethodCoreOperationKind::FileCopyRaw;
    if (selectedMember.name == "MoveRaw" &&
        selectedMember.declaredTypeKey == "builtin:bool" &&
        selectedMember.parameters.size() == 2 &&
        selectedMember.parameters[0].typeKey == "builtin:string" &&
        selectedMember.parameters[1].typeKey == "builtin:string")
        return isFile ? MethodCoreOperationKind::FileMoveRaw
                      : MethodCoreOperationKind::DirectoryMoveRaw;
    return std::nullopt;
}

void ProjectMethodBodyModel::recognizeCoreFilePathOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    recognizeCoreFileDirectoryOperations(
        expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    // Stage 53.4 "Filesystem/Paths (cierre)" -- NativeSharp.Core.IO.Path.GetFullPath, la
    // PRIMERA operacion nucleo de Path (ver Path.ns): un builtin:string (path) entra, un
    // builtin:string (recien alocado por el runtime, NO owned -- mismo patron que
    // GetEnvironmentVariable/ReadAllText) sale. Su propio "if" porque Path es una tercera
    // clase distinta de File/Directory arriba, y todos los demas metodos de Path
    // (Combine, GetFileName, etc.) deben seguir siendo NativeSharp puro, sin pasar por
    // esta rama en absoluto.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.IO.Path" &&
        selectedMember.isStatic &&
        selectedMember.name == "GetFullPath" &&
        selectedMember.declaredTypeKey == "builtin:string" &&
        selectedMember.parameters.size() == 1 &&
        selectedMember.parameters[0].typeKey == "builtin:string")
    {
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::PathGetFullPath,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            selectedMember.declaredTypeKey,
            selectedMember.parameters.front().typeKey,
            false, std::nullopt, std::nullopt});
    }
}

void ProjectMethodBodyModel::recognizeCoreThreadOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Stage 54.1 "Threads" -- NativeSharp.Core.Threading.Thread's tres helpers privados y
    // estaticos. A diferencia de TODOS los core operations anteriores, estos tres NO son
    // llamados directamente por codigo NativeSharp del usuario -- son llamados desde
    // adentro de la propia clase Thread (constructor/Join()/Detach()), que es codigo
    // NativeSharp normal, no interceptado. Por eso mismo el reconocimiento sigue siendo
    // "clase exacta + nombre exacto + firma exacta", igual que cualquier otro core
    // operation: el compilador no distingue "quien" llama, solo QUE se llama. Ver
    // Thread.ns para el diseno completo.
    //
    // ThreadCreate: un argumento de tipo delegate (ThreadStart) entra, builtin:nint (el
    // HANDLE de Win32, guardado por valor) sale. Primera vez que un core operation recibe
    // un delegate como operando -- ver ProjectOopHirModel.cpp para como el bloque HIR
    // compartido (bucket expected=1U) no necesita ningun cambio para aceptar esto: nunca
    // exigio un tipo de operando particular, solo bajo lo que hubiera ahi con
    // lowerExpression.
    // ThreadJoin/ThreadDetach: un builtin:nint (el HANDLE que el llamador ya tiene
    // guardado) entra, builtin:void sale -- mismo shape que DirectoryDelete, cambiando
    // solo el tipo del operando (nint en vez de string).
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.Thread" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> threadKind;
        if (selectedMember.name == "ThreadCreate" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.size() == 1)
        {
            // ThreadStart's typeKey is "type:" + identity.key(), y key() incluye el
            // moduleName ("modulo::NombreCalificado") -- NO simplemente
            // "type:<FullyQualifiedName>". En vez de asumir un moduleName literal (fragil
            // si cambia el nombre del modulo de la biblioteca), resolvemos una vez el ordinal
            // de ThreadStart por su nombre completamente calificado al iniciar el modelo y
            // comparamos contra la key() real -- el mismo patron defensivo que
            // delegateTypeOrdinalForTypeKey (mas abajo en este archivo) usa yendo en la
            // direccion inversa (typeKey -> ordinal).
            const bool parameterIsThreadStart = !coreThreadStartTypeKey_.empty() &&
                selectedMember.parameters[0].typeKey == coreThreadStartTypeKey_;
            if (parameterIsThreadStart)
            {
                threadKind = MethodCoreOperationKind::ThreadCreate;
            }
        }
        else if (selectedMember.name == "ThreadJoin" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            threadKind = MethodCoreOperationKind::ThreadJoin;
        }
        else if (selectedMember.name == "ThreadDetach" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            threadKind = MethodCoreOperationKind::ThreadDetach;
        }
        // Pre-Stage-56 "utilidades basicas" -- Thread.SleepRaw. Same one-argument
        // (builtin:int)/void shape as Environment.Exit (53.2) -- sleeps the CALLING
        // thread (Win32 Sleep(ms)), not any particular Thread instance, which is why it
        // lives here as a static helper rather than on a Thread receiver.
        else if (selectedMember.name == "SleepRaw" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:int")
        {
            threadKind = MethodCoreOperationKind::ThreadSleepRaw;
        }
        if (threadKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *threadKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

bool ProjectMethodBodyModel::isMemoryOrderTypeKey(std::string_view typeKey) const
{
    return !coreMemoryOrderTypeKey_.empty() && typeKey == coreMemoryOrderTypeKey_;
}

void ProjectMethodBodyModel::recognizeCoreAtomicValueOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    ExpressionResult& result,
    diagnostics::DiagnosticEngine& diagnostics)
{
    (void)result;
    // Stage 54.2/Item 311 atomics. Recognition is deliberately exact by declaring type,
    // name, parameter mode, value type, result type, and MemoryOrder position.
    //
    // Wrapper calls operate on their receiver's private `value_` field. This block resolves
    // that field once and stores its ordinal in implicitFieldMemberOrdinal, allowing HIR to
    // synthesize a FieldAddress without exposing implementation storage.
    //
    // Public Interlocked calls are static and begin with a true `ref int`, `ref long`, or
    // `ref void*` parameter. For those bindings implicitFieldMemberOrdinal stays empty:
    // HIR consumes the already-address-valued first argument as the location. Both source
    // shapes reuse the same MethodCoreOperationKind identities and all later lowering.
    const std::string atomicDeclaringType = declaringType.identity.fullyQualifiedName();
    const bool atomicWrapper =
        atomicDeclaringType == "NativeSharp.Core.Threading.AtomicInt32" ||
        atomicDeclaringType == "NativeSharp.Core.Threading.AtomicInt64" ||
        atomicDeclaringType == "NativeSharp.Core.Threading.AtomicPointer";
    const bool publicInterlocked =
        atomicDeclaringType == "NativeSharp.Core.Threading.Interlocked" &&
        selectedMember.isStatic && !selectedMember.parameters.empty() &&
        selectedMember.parameters[0].passKind == ParameterPassKind::Ref &&
        (selectedMember.parameters[0].pointeeTypeKey == "builtin:int" ||
         selectedMember.parameters[0].pointeeTypeKey == "builtin:long" ||
         selectedMember.parameters[0].pointeeTypeKey == "builtin:void*");
    if (!((atomicWrapper && !selectedMember.isStatic) || publicInterlocked)) return;
    const std::size_t parameterOffset = publicInterlocked ? 1U : 0U;
    const std::string locationValueType = publicInterlocked
        ? selectedMember.parameters[0].pointeeTypeKey
        : atomicDeclaringType == "NativeSharp.Core.Threading.AtomicInt32"
            ? "builtin:int" : atomicDeclaringType == "NativeSharp.Core.Threading.AtomicPointer"
                ? "builtin:void*" : "builtin:long";
    const bool isInt32 = locationValueType == "builtin:int";
    const bool isPointer = locationValueType == "builtin:void*";
    const std::string valueTypeKey =
        isInt32 ? "builtin:int" : isPointer ? "builtin:void*" : "builtin:long";
    bool orderedLoad = false;
    bool orderedStore = false;
    bool orderedRmw = false;
    bool orderedCompareExchange = false;
    classifyOrderedAtomicCall(
        orderedLoad, orderedStore, orderedRmw, orderedCompareExchange,
        selectedMember, parameterOffset, valueTypeKey);
    std::optional<MethodCoreOperationKind> atomicKind = atomicOperationKind(
        selectedMember, parameterOffset, valueTypeKey, isInt32, isPointer,
        orderedLoad, orderedStore, orderedRmw, orderedCompareExchange);
    std::optional<std::uint8_t> atomicMemoryOrder;
    std::optional<std::uint8_t> atomicFailureMemoryOrder;
    if (atomicKind.has_value() && orderedCompareExchange)
        validateAtomicCompareExchangeOrders(
            atomicKind, atomicMemoryOrder, atomicFailureMemoryOrder,
            expression, selectedMember, parameterOffset, methodMemberOrdinal, diagnostics);
    else if (atomicKind.has_value() && (orderedLoad || orderedStore || orderedRmw))
        validateAtomicMemoryOrder(
            atomicKind, atomicMemoryOrder, expression, selectedMember,
            methodMemberOrdinal, orderedLoad, orderedStore, diagnostics);
    if (atomicKind.has_value())
        bindAtomicOperation(
            *atomicKind, atomicMemoryOrder, atomicFailureMemoryOrder,
            expression, selectedMember, valueTypeKey, atomicWrapper, publicInterlocked,
            methodMemberOrdinal, scopeOrdinal);
}

void ProjectMethodBodyModel::classifyOrderedAtomicCall(
    bool& orderedLoad,
    bool& orderedStore,
    bool& orderedRmw,
    bool& orderedCompareExchange,
    const ClassMemberSymbol& selectedMember,
    std::size_t parameterOffset,
    const std::string& valueTypeKey) const
{
    orderedLoad = selectedMember.name == "Load" &&
        selectedMember.parameters.size() == parameterOffset + 1U &&
        isMemoryOrderTypeKey(selectedMember.parameters[parameterOffset].typeKey) &&
        selectedMember.declaredTypeKey == valueTypeKey;
    orderedStore = selectedMember.name == "Store" &&
        selectedMember.parameters.size() == parameterOffset + 2U &&
        selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
        isMemoryOrderTypeKey(selectedMember.parameters[parameterOffset + 1U].typeKey) &&
        selectedMember.declaredTypeKey == "builtin:void";
    const bool orderedUnaryRmw =
        (selectedMember.name == "Increment" || selectedMember.name == "Decrement") &&
        selectedMember.parameters.size() == parameterOffset + 1U &&
        isMemoryOrderTypeKey(selectedMember.parameters[parameterOffset].typeKey) &&
        selectedMember.declaredTypeKey == valueTypeKey;
    const bool orderedValueRmw =
        (selectedMember.name == "Add" || selectedMember.name == "Exchange") &&
        selectedMember.parameters.size() == parameterOffset + 2U &&
        selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
        isMemoryOrderTypeKey(selectedMember.parameters[parameterOffset + 1U].typeKey) &&
        selectedMember.declaredTypeKey == valueTypeKey;
    orderedRmw = orderedUnaryRmw || orderedValueRmw;
    orderedCompareExchange = selectedMember.name == "CompareExchange" &&
        selectedMember.parameters.size() == parameterOffset + 4U &&
        selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
        selectedMember.parameters[parameterOffset + 1U].typeKey == valueTypeKey &&
        isMemoryOrderTypeKey(selectedMember.parameters[parameterOffset + 2U].typeKey) &&
        isMemoryOrderTypeKey(selectedMember.parameters[parameterOffset + 3U].typeKey) &&
        selectedMember.declaredTypeKey == valueTypeKey;
}

std::optional<MethodCoreOperationKind> ProjectMethodBodyModel::atomicOperationKind(
    const ClassMemberSymbol& selectedMember,
    std::size_t parameterOffset,
    const std::string& valueTypeKey,
    bool isInt32,
    bool isPointer,
    bool orderedLoad,
    bool orderedStore,
    bool orderedRmw,
    bool orderedCompareExchange)
{
    if ((selectedMember.name == "Load" &&
            selectedMember.parameters.size() == parameterOffset &&
            selectedMember.declaredTypeKey == valueTypeKey) || orderedLoad)
        return isPointer ? MethodCoreOperationKind::AtomicLoadPointer
                         : isInt32 ? MethodCoreOperationKind::AtomicLoadInt32
                                   : MethodCoreOperationKind::AtomicLoadInt64;
    if ((selectedMember.name == "Store" &&
            selectedMember.parameters.size() == parameterOffset + 1U &&
            selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
            selectedMember.declaredTypeKey == "builtin:void") || orderedStore)
        return isPointer ? MethodCoreOperationKind::AtomicStorePointer
                         : isInt32 ? MethodCoreOperationKind::AtomicStoreInt32
                                   : MethodCoreOperationKind::AtomicStoreInt64;
    if (!isPointer &&
        ((selectedMember.name == "Increment" &&
            selectedMember.parameters.size() == parameterOffset &&
            selectedMember.declaredTypeKey == valueTypeKey) ||
         (orderedRmw && selectedMember.name == "Increment")))
        return isInt32 ? MethodCoreOperationKind::InterlockedIncrementInt32
                       : MethodCoreOperationKind::InterlockedIncrementInt64;
    if (!isPointer &&
        ((selectedMember.name == "Decrement" &&
            selectedMember.parameters.size() == parameterOffset &&
            selectedMember.declaredTypeKey == valueTypeKey) ||
         (orderedRmw && selectedMember.name == "Decrement")))
        return isInt32 ? MethodCoreOperationKind::InterlockedDecrementInt32
                       : MethodCoreOperationKind::InterlockedDecrementInt64;
    if (!isPointer &&
        ((selectedMember.name == "Add" &&
            selectedMember.parameters.size() == parameterOffset + 1U &&
            selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
            selectedMember.declaredTypeKey == valueTypeKey) ||
         (orderedRmw && selectedMember.name == "Add")))
        return isInt32 ? MethodCoreOperationKind::InterlockedAddInt32
                       : MethodCoreOperationKind::InterlockedAddInt64;
    if ((selectedMember.name == "Exchange" &&
            selectedMember.parameters.size() == parameterOffset + 1U &&
            selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
            selectedMember.declaredTypeKey == valueTypeKey) ||
        (orderedRmw && selectedMember.name == "Exchange"))
        return isPointer ? MethodCoreOperationKind::InterlockedExchangePointer
                         : isInt32 ? MethodCoreOperationKind::InterlockedExchangeInt32
                                   : MethodCoreOperationKind::InterlockedExchangeInt64;
    if ((selectedMember.name == "CompareExchange" &&
            selectedMember.parameters.size() == parameterOffset + 2U &&
            selectedMember.parameters[parameterOffset].typeKey == valueTypeKey &&
            selectedMember.parameters[parameterOffset + 1U].typeKey == valueTypeKey &&
            selectedMember.declaredTypeKey == valueTypeKey) || orderedCompareExchange)
        return isPointer ? MethodCoreOperationKind::InterlockedCompareExchangePointer
                         : isInt32 ? MethodCoreOperationKind::InterlockedCompareExchangeInt32
                                   : MethodCoreOperationKind::InterlockedCompareExchangeInt64;
    return std::nullopt;
}

void ProjectMethodBodyModel::validateAtomicCompareExchangeOrders(
    std::optional<MethodCoreOperationKind>& atomicKind,
    std::optional<std::uint8_t>& atomicMemoryOrder,
    std::optional<std::uint8_t>& atomicFailureMemoryOrder,
    const ast::AstNode* expression,
    const ClassMemberSymbol& selectedMember,
    std::size_t parameterOffset,
    std::size_t methodMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const ast::AstNode* successSyntax = expression->children().size() >= parameterOffset + 3U
        ? expression->children()[parameterOffset + 2U] : nullptr;
    const ast::AstNode* failureSyntax = expression->children().size() >= parameterOffset + 4U
        ? expression->children()[parameterOffset + 3U] : nullptr;
    const std::optional<ProjectIntegerValue> foldedSuccess =
        foldIntegerConstant(successSyntax, selectedMember.declaringTypeOrdinal, methodMemberOrdinal);
    const std::optional<ProjectIntegerValue> foldedFailure =
        foldIntegerConstant(failureSyntax, selectedMember.declaringTypeOrdinal, methodMemberOrdinal);
    if (!foldedSuccess.has_value() || !foldedFailure.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::ConstRequiresConstant,
            diagnostics::DiagnosticSeverity::Error,
            !foldedSuccess.has_value() && successSyntax != nullptr
                ? successSyntax->range()
                : failureSyntax != nullptr ? failureSyntax->range() : expression->range(),
            "atomic compare-exchange orders must be compile-time MemoryOrder constants");
        atomicKind.reset();
        return;
    }
    const std::uint64_t success = foldedSuccess->bits;
    const std::uint64_t failure = foldedFailure->bits;
    const std::optional<language::MemoryOrder> successOrder =
        language::decodeMemoryOrder(success);
    const std::optional<language::MemoryOrder> failureOrder =
        language::decodeMemoryOrder(failure);
    if (!successOrder.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            successSyntax != nullptr ? successSyntax->range() : expression->range(),
            "atomic compare-exchange success order is outside MemoryOrder");
        atomicKind.reset();
    }
    else if (!failureOrder.has_value() ||
        !language::validMemoryOrder(language::AtomicOperation::Load, *failureOrder))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            failureSyntax != nullptr ? failureSyntax->range() : expression->range(),
            "atomic compare-exchange failure order must be Relaxed, Acquire, or SequentiallyConsistent");
        atomicKind.reset();
    }
    else if (!language::validCompareExchangeOrders(*successOrder, *failureOrder))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            failureSyntax != nullptr ? failureSyntax->range() : expression->range(),
            "atomic compare-exchange failure order cannot be stronger than success order");
        atomicKind.reset();
    }
    else
    {
        atomicMemoryOrder = static_cast<std::uint8_t>(success);
        atomicFailureMemoryOrder = static_cast<std::uint8_t>(failure);
    }
}

void ProjectMethodBodyModel::validateAtomicMemoryOrder(
    std::optional<MethodCoreOperationKind>& atomicKind,
    std::optional<std::uint8_t>& atomicMemoryOrder,
    const ast::AstNode* expression,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    bool orderedLoad,
    bool orderedStore,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const ast::AstNode* orderSyntax =
        expression->children().empty() ? nullptr : expression->children().back();
    const std::optional<ProjectIntegerValue> foldedOrder =
        foldIntegerConstant(orderSyntax, selectedMember.declaringTypeOrdinal, methodMemberOrdinal);
    if (!foldedOrder.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::ConstRequiresConstant,
            diagnostics::DiagnosticSeverity::Error,
            orderSyntax != nullptr ? orderSyntax->range() : expression->range(),
            "atomic memory order must be a compile-time MemoryOrder constant");
        atomicKind.reset();
        return;
    }
    const std::uint64_t order = foldedOrder->bits;
    const std::optional<language::MemoryOrder> decodedOrder =
        language::decodeMemoryOrder(order);
    const language::AtomicOperation operation = orderedLoad
        ? language::AtomicOperation::Load
        : orderedStore ? language::AtomicOperation::Store
                       : language::AtomicOperation::ReadModifyWrite;
    const bool legal = decodedOrder.has_value() &&
        language::validMemoryOrder(operation, *decodedOrder);
    if (!legal)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            orderSyntax != nullptr ? orderSyntax->range() : expression->range(),
            orderedLoad
                ? "atomic Load order must be Relaxed, Acquire, or SequentiallyConsistent"
                : orderedStore
                    ? "atomic Store order must be Relaxed, Release, or SequentiallyConsistent"
                    : "atomic read-modify-write order is outside MemoryOrder");
        atomicKind.reset();
        return;
    }
    atomicMemoryOrder = static_cast<std::uint8_t>(order);
}

void ProjectMethodBodyModel::bindAtomicOperation(
    MethodCoreOperationKind atomicKind,
    std::optional<std::uint8_t> atomicMemoryOrder,
    std::optional<std::uint8_t> atomicFailureMemoryOrder,
    const ast::AstNode* expression,
    const ClassMemberSymbol& selectedMember,
    const std::string& valueTypeKey,
    bool atomicWrapper,
    bool publicInterlocked,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    std::optional<std::size_t> valueFieldOrdinal;
    if (atomicWrapper && memberModel_ != nullptr)
    {
        for (const std::size_t candidateOrdinal : memberModel_->findMembers(
                 selectedMember.declaringTypeOrdinal, "value_"))
        {
            if (candidateOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
            if (candidate.kind == ClassMemberKind::Field)
            {
                valueFieldOrdinal = candidateOrdinal;
                break;
            }
        }
    }
    if (!publicInterlocked && !valueFieldOrdinal.has_value()) return;
    MethodCoreBinding binding{
        atomicKind,
        expression,
        methodMemberOrdinal,
        scopeOrdinal,
        selectedMember.declaredTypeKey,
        valueTypeKey,
        false, std::nullopt, std::nullopt};
    binding.atomicMemoryOrder = atomicMemoryOrder;
    binding.atomicFailureMemoryOrder = atomicFailureMemoryOrder;
    if (valueFieldOrdinal.has_value())
        binding.implicitFieldMemberOrdinal = valueFieldOrdinal;
    coreBindings_.push_back(std::move(binding));
}

void ProjectMethodBodyModel::recognizeCoreAtomicOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    ExpressionResult& result,
    diagnostics::DiagnosticEngine& diagnostics)
{
    recognizeCoreAtomicValueOperations(
        expression, declaringType, selectedMember,
        methodMemberOrdinal, scopeOrdinal, result, diagnostics);
    // Item 307: static fences consume MemoryOrder during semantic analysis. The source
    // argument never becomes a runtime integer; HIR carries only the validated ABI value.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.AtomicFence" &&
        selectedMember.isStatic && selectedMember.declaredTypeKey == "builtin:void" &&
        selectedMember.parameters.size() == 1 && !coreMemoryOrderTypeKey_.empty())
    {
        const bool isMemoryOrder =
            isMemoryOrderTypeKey(selectedMember.parameters[0].typeKey);
        std::optional<MethodCoreOperationKind> fenceKind;
        if (isMemoryOrder && selectedMember.name == "Thread")
            fenceKind = MethodCoreOperationKind::AtomicThreadFence;
        else if (isMemoryOrder && selectedMember.name == "Compiler")
            fenceKind = MethodCoreOperationKind::AtomicCompilerFence;
        if (fenceKind.has_value())
        {
            const ast::AstNode* orderSyntax = expression->children().empty()
                ? nullptr
                : expression->children()[0];
            const std::optional<ProjectIntegerValue> foldedOrder =
                foldIntegerConstant(
                    orderSyntax, selectedMember.declaringTypeOrdinal, methodMemberOrdinal);
            if (!foldedOrder.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::ConstRequiresConstant,
                    diagnostics::DiagnosticSeverity::Error,
                    orderSyntax != nullptr ? orderSyntax->range() : expression->range(),
                    "atomic fence order must be a compile-time MemoryOrder constant");
            }
            else if (const std::optional<language::MemoryOrder> order =
                         language::decodeMemoryOrder(foldedOrder->bits);
                !order.has_value() ||
                !language::validMemoryOrder(language::AtomicOperation::Fence, *order))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    orderSyntax != nullptr ? orderSyntax->range() : expression->range(),
                    "atomic fence order must be Acquire, Release, AcquireRelease, or SequentiallyConsistent");
            }
            else
            {
                MethodCoreBinding binding{
                    *fenceKind, expression, methodMemberOrdinal, scopeOrdinal,
                    "builtin:void", "", false, std::nullopt, std::nullopt};
                binding.atomicMemoryOrder = static_cast<std::uint8_t>(foldedOrder->bits);
                coreBindings_.push_back(std::move(binding));
            }
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreMutexOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Stage 54.3 "Mutex" -- NativeSharp.Core.Threading.Mutex's cuatro helpers privados y
    // estaticos. EXACTO mismo patron estructural que Thread arriba (Stage 54.1): un
    // HANDLE de Win32 guardado por valor como builtin:nint, ida y vuelta entre llamadas
    // NativeSharp separadas. MutexCreate es la PRIMERA core operation de todo el proyecto
    // sin ningun operando explicito y sin receptor -- ver el bucket "expected=0U" nuevo
    // en ProjectOopHirModel.cpp.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.Mutex" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> mutexKind;
        if (selectedMember.name == "MutexCreate" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.empty())
        {
            mutexKind = MethodCoreOperationKind::MutexCreate;
        }
        else if (selectedMember.name == "MutexCreateNamed" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:string")
        {
            mutexKind = MethodCoreOperationKind::MutexCreateNamed;
        }
        else if (selectedMember.name == "MutexLock" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            mutexKind = MethodCoreOperationKind::MutexLock;
        }
        else if (selectedMember.name == "MutexUnlock" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            mutexKind = MethodCoreOperationKind::MutexUnlock;
        }
        else if (selectedMember.name == "MutexClose" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            mutexKind = MethodCoreOperationKind::MutexClose;
        }
        if (mutexKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *mutexKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreMutexMonitorOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    recognizeCoreMutexOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    // Stage 54.4 "Condition Variables" -- NativeSharp.Core.Threading.Monitor's five
    // INSTANCE methods. Same "search the declaring type's own members by field name"
    // mechanism Stage 54.2 established for AtomicInt32/AtomicInt64's `value_`, just
    // resolving TWO distinct field names (`lock_`, `cv_`) instead of one -- see
    // MethodCoreOperationKind::MonitorWait's comment in ProjectMethodBodyModel.h for why
    // Wait needs both ordinals recorded here (never re-lowering the receiver expression a
    // second time at HIR level).
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.Monitor" &&
        !selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> monitorKind;
        if (selectedMember.name == "Enter" && selectedMember.parameters.empty() &&
            selectedMember.declaredTypeKey == "builtin:void")
        {
            monitorKind = MethodCoreOperationKind::MonitorEnter;
        }
        else if (selectedMember.name == "Exit" && selectedMember.parameters.empty() &&
            selectedMember.declaredTypeKey == "builtin:void")
        {
            monitorKind = MethodCoreOperationKind::MonitorExit;
        }
        else if (selectedMember.name == "Wait" && selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:int" &&
            selectedMember.declaredTypeKey == "builtin:bool")
        {
            monitorKind = MethodCoreOperationKind::MonitorWait;
        }
        else if (selectedMember.name == "Pulse" && selectedMember.parameters.empty() &&
            selectedMember.declaredTypeKey == "builtin:void")
        {
            monitorKind = MethodCoreOperationKind::MonitorPulse;
        }
        else if (selectedMember.name == "PulseAll" && selectedMember.parameters.empty() &&
            selectedMember.declaredTypeKey == "builtin:void")
        {
            monitorKind = MethodCoreOperationKind::MonitorPulseAll;
        }
        if (monitorKind.has_value() && memberModel_ != nullptr)
        {
            std::optional<std::size_t> lockFieldOrdinal;
            std::optional<std::size_t> cvFieldOrdinal;
            for (const std::size_t candidateOrdinal : memberModel_->findMembers(
                     selectedMember.declaringTypeOrdinal, "lock_"))
            {
                if (candidateOrdinal >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
                if (candidate.kind == ClassMemberKind::Field)
                    lockFieldOrdinal = candidateOrdinal;
            }
            for (const std::size_t candidateOrdinal : memberModel_->findMembers(
                     selectedMember.declaringTypeOrdinal, "cv_"))
            {
                if (candidateOrdinal >= memberModel_->members().size()) continue;
                const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
                if (candidate.kind == ClassMemberKind::Field)
                    cvFieldOrdinal = candidateOrdinal;
            }
            const bool needsBoth = *monitorKind == MethodCoreOperationKind::MonitorWait;
            const bool needsLockOnly = *monitorKind == MethodCoreOperationKind::MonitorEnter ||
                *monitorKind == MethodCoreOperationKind::MonitorExit;
            const bool ready = needsBoth
                ? (lockFieldOrdinal.has_value() && cvFieldOrdinal.has_value())
                : (needsLockOnly ? lockFieldOrdinal.has_value() : cvFieldOrdinal.has_value());
            if (ready)
            {
                MethodCoreBinding binding{
                    *monitorKind,
                    expression,
                    methodMemberOrdinal,
                    scopeOrdinal,
                    selectedMember.declaredTypeKey,
                    selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                    false, std::nullopt, std::nullopt};
                if (needsBoth)
                {
                    // Primary = cv_ (SleepConditionVariableSRW's FIRST pointer argument),
                    // secondary = lock_ (its SECOND) -- see ProjectOopHirModel.cpp's
                    // CoreMonitorWait case for the argument-order contract this mirrors.
                    binding.implicitFieldMemberOrdinal = cvFieldOrdinal;
                    binding.secondaryImplicitFieldMemberOrdinal = lockFieldOrdinal;
                }
                else if (needsLockOnly)
                {
                    binding.implicitFieldMemberOrdinal = lockFieldOrdinal;
                }
                else
                {
                    binding.implicitFieldMemberOrdinal = cvFieldOrdinal;
                }
                coreBindings_.push_back(std::move(binding));
            }
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreSemaphoreOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Stage 54.5 "Otras primitivas de sincronizacion" -- NativeSharp.Core.Threading.
    // Semaphore's cuatro helpers privados y estaticos, EXACTO mismo patron que Mutex
    // arriba (Stage 54.3): HANDLE de kernel via builtin:nint, cero mecanismo de
    // FieldAddress. SemaphoreWait es la UNICA diferencia real de forma: toma un SEGUNDO
    // operando (el timeout en milisegundos) y devuelve builtin:bool (true = adquirido,
    // false = timeout expirado) en vez de builtin:void -- ver el comentario de
    // MethodCoreOperationKind::SemaphoreWait en el header para el detalle completo.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.Semaphore" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> semaphoreKind;
        if (selectedMember.name == "SemaphoreCreate" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:int" &&
            selectedMember.parameters[1].typeKey == "builtin:int")
        {
            semaphoreKind = MethodCoreOperationKind::SemaphoreCreate;
        }
        else if (selectedMember.name == "SemaphoreCreateNamed" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.size() == 3 &&
            selectedMember.parameters[0].typeKey == "builtin:int" &&
            selectedMember.parameters[1].typeKey == "builtin:int" &&
            selectedMember.parameters[2].typeKey == "builtin:string")
        {
            semaphoreKind = MethodCoreOperationKind::SemaphoreCreateNamed;
        }
        else if (selectedMember.name == "SemaphoreWait" &&
            selectedMember.declaredTypeKey == "builtin:bool" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:nint" &&
            selectedMember.parameters[1].typeKey == "builtin:int")
        {
            semaphoreKind = MethodCoreOperationKind::SemaphoreWait;
        }
        else if (selectedMember.name == "SemaphoreRelease" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            semaphoreKind = MethodCoreOperationKind::SemaphoreRelease;
        }
        else if (selectedMember.name == "SemaphoreClose" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            semaphoreKind = MethodCoreOperationKind::SemaphoreClose;
        }
        if (semaphoreKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *semaphoreKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreSemaphoreEventOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    recognizeCoreSemaphoreOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    // NativeSharp.Core.Threading.Event's cinco helpers privados y estaticos, mismo patron
    // que Semaphore arriba. EventCreate toma DOS operandos explicitos (manualReset,
    // initialState, ambos builtin:bool) sin receptor -- mismo bucket compartido
    // "expected=2U" que SemaphoreCreate ya usa. EventWait comparte la MISMA forma y el
    // MISMO runtime hook de bajo nivel que SemaphoreWait (ver WindowsRuntimeBuilder.cpp).
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Threading.Event" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> eventKind;
        if (selectedMember.name == "EventCreate" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:bool" &&
            selectedMember.parameters[1].typeKey == "builtin:bool")
        {
            eventKind = MethodCoreOperationKind::EventCreate;
        }
        else if (selectedMember.name == "EventCreateNamed" &&
            selectedMember.declaredTypeKey == "builtin:nint" &&
            selectedMember.parameters.size() == 3 &&
            selectedMember.parameters[0].typeKey == "builtin:bool" &&
            selectedMember.parameters[1].typeKey == "builtin:bool" &&
            selectedMember.parameters[2].typeKey == "builtin:string")
        {
            eventKind = MethodCoreOperationKind::EventCreateNamed;
        }
        else if (selectedMember.name == "EventSet" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            eventKind = MethodCoreOperationKind::EventSet;
        }
        else if (selectedMember.name == "EventReset" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            eventKind = MethodCoreOperationKind::EventReset;
        }
        else if (selectedMember.name == "EventWait" &&
            selectedMember.declaredTypeKey == "builtin:bool" &&
            selectedMember.parameters.size() == 2 &&
            selectedMember.parameters[0].typeKey == "builtin:nint" &&
            selectedMember.parameters[1].typeKey == "builtin:int")
        {
            eventKind = MethodCoreOperationKind::EventWait;
        }
        else if (selectedMember.name == "EventClose" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:nint")
        {
            eventKind = MethodCoreOperationKind::EventClose;
        }
        if (eventKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *eventKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreProcessOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // Stage 55.1 "Process" -- NativeSharp.Core.Diagnostics.Process. Five zero-argument Raw
    // getters reuse NativeClock's (53.1) exact zero-argument/builtin:long recognition
    // shape; SetPriorityClassRaw reuses Environment.Exit's (53.2) one-argument
    // (builtin:int)/void shape. Only the CURRENT process is ever addressed -- the Win32
    // HANDLE (GetCurrentProcess()) is obtained inside the runtime wrapper, never exposed
    // to NativeSharp, so no receiver/implicit-field machinery is needed here at all.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Diagnostics.Process" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> processKind;
        // Bloque 4, fase C -- SHR-032. `Run(exe, linea, salida)`: tres builtin:string
        // entran, builtin:int sale. Es la PRIMERA operacion nucleo de esta clase que
        // habla de OTRO proceso -- las cinco de Stage 55.1 son todas sobre el actual, y
        // su comentario de arriba lo dice. La forma de tres argumentos es la misma que
        // String.Substring y File.WriteAllBytes ya usan.
        if (selectedMember.name == "Run" &&
            selectedMember.declaredTypeKey == "builtin:int" &&
            selectedMember.parameters.size() == 3 &&
            selectedMember.parameters[0].typeKey == "builtin:string" &&
            selectedMember.parameters[1].typeKey == "builtin:string" &&
            selectedMember.parameters[2].typeKey == "builtin:string")
        {
            processKind = MethodCoreOperationKind::ProcessRun;
        }
        else if (selectedMember.declaredTypeKey == "builtin:long" && selectedMember.parameters.empty())
        {
            if (selectedMember.name == "GetCurrentProcessIdRaw")
            {
                processKind = MethodCoreOperationKind::ProcessGetCurrentProcessIdRaw;
            }
            else if (selectedMember.name == "GetWorkingSetSizeRaw")
            {
                processKind = MethodCoreOperationKind::ProcessGetWorkingSetSizeRaw;
            }
            else if (selectedMember.name == "GetKernelTimeRaw")
            {
                processKind = MethodCoreOperationKind::ProcessGetKernelTimeRaw;
            }
            else if (selectedMember.name == "GetUserTimeRaw")
            {
                processKind = MethodCoreOperationKind::ProcessGetUserTimeRaw;
            }
            else if (selectedMember.name == "GetPriorityClassRaw")
            {
                processKind = MethodCoreOperationKind::ProcessGetPriorityClassRaw;
            }
        }
        else if (selectedMember.name == "SetPriorityClassRaw" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:int")
        {
            processKind = MethodCoreOperationKind::ProcessSetPriorityClassRaw;
        }
        if (processKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *processKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::recognizeCoreDiagnosticsOperations(
    const ast::AstNode* expression,
    const NamedTypeDeclaration& declaringType,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    recognizeCoreProcessOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    // Stage 55.2 "Hardware / System Info" -- NativeSharp.Core.Diagnostics.SystemInfo. All
    // six reuse the same zero-argument/builtin:long shape as Process above. Deliberately
    // does NOT duplicate Environment.ProcessorCount (53.2) -- see the enum header comment
    // in ProjectMethodBodyModel.h for the full rationale on scope.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Diagnostics.SystemInfo" &&
        selectedMember.isStatic &&
        selectedMember.declaredTypeKey == "builtin:long" &&
        selectedMember.parameters.empty())
    {
        std::optional<MethodCoreOperationKind> systemInfoKind;
        if (selectedMember.name == "GetPageSizeRaw")
        {
            systemInfoKind = MethodCoreOperationKind::SystemInfoGetPageSizeRaw;
        }
        else if (selectedMember.name == "GetProcessorArchitectureRaw")
        {
            systemInfoKind = MethodCoreOperationKind::SystemInfoGetProcessorArchitectureRaw;
        }
        else if (selectedMember.name == "GetAllocationGranularityRaw")
        {
            systemInfoKind = MethodCoreOperationKind::SystemInfoGetAllocationGranularityRaw;
        }
        else if (selectedMember.name == "GetTotalPhysicalMemoryBytesRaw")
        {
            systemInfoKind = MethodCoreOperationKind::SystemInfoGetTotalPhysicalMemoryBytesRaw;
        }
        else if (selectedMember.name == "GetAvailablePhysicalMemoryBytesRaw")
        {
            systemInfoKind = MethodCoreOperationKind::SystemInfoGetAvailablePhysicalMemoryBytesRaw;
        }
        else if (selectedMember.name == "GetMemoryLoadPercentRaw")
        {
            systemInfoKind = MethodCoreOperationKind::SystemInfoGetMemoryLoadPercentRaw;
        }
        if (systemInfoKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *systemInfoKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                "builtin:long",
                std::string{},
                false, std::nullopt, std::nullopt});
        }
    }
    // Stage 55.3 "Diagnostics" -- NativeSharp.Core.Diagnostics.Debug. The two Raw getters
    // reuse the same zero-argument/builtin:long shape; OutputStringRaw reuses File.Delete's
    // (53.3) one-argument(builtin:string)/void shape, marshaled through the exact same
    // buildNullTerminatedCopy helper Environment.SetEnvironmentVariable (53.2b) introduced.
    if (declaringType.identity.fullyQualifiedName() == "NativeSharp.Core.Diagnostics.Debug" &&
        selectedMember.isStatic)
    {
        std::optional<MethodCoreOperationKind> debugKind;
        if (selectedMember.declaredTypeKey == "builtin:long" && selectedMember.parameters.empty())
        {
            if (selectedMember.name == "IsDebuggerPresentRaw")
            {
                debugKind = MethodCoreOperationKind::DebugIsDebuggerPresentRaw;
            }
            else if (selectedMember.name == "CaptureStackFrameCountRaw")
            {
                debugKind = MethodCoreOperationKind::DebugCaptureStackFrameCountRaw;
            }
        }
        else if (selectedMember.name == "OutputStringRaw" &&
            selectedMember.declaredTypeKey == "builtin:void" &&
            selectedMember.parameters.size() == 1 &&
            selectedMember.parameters[0].typeKey == "builtin:string")
        {
            debugKind = MethodCoreOperationKind::DebugOutputStringRaw;
        }
        if (debugKind.has_value())
        {
            coreBindings_.push_back(MethodCoreBinding{
                *debugKind,
                expression,
                methodMemberOrdinal,
                scopeOrdinal,
                selectedMember.declaredTypeKey,
                selectedMember.parameters.empty() ? std::string{} : selectedMember.parameters.front().typeKey,
                false, std::nullopt, std::nullopt});
        }
    }
}

void ProjectMethodBodyModel::applySelectedCoreOperations(
    const ast::AstNode* expression,
    const ClassMemberSymbol& selectedMember,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    MethodCallBinding& call,
    ExpressionResult& result,
    diagnostics::DiagnosticEngine& diagnostics)
{
    (void)call;
    if (nameLookup_ != nullptr && selectedMember.declaringTypeOrdinal < nameLookup_->namedTypes().size())
    {
        const NamedTypeDeclaration& declaringType = nameLookup_->namedTypes()[selectedMember.declaringTypeOrdinal];
        recognizeCoreStringOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreFloat128Operations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreInt128Operations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreScalarOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreVector256Operations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreConsoleRuntimeOperations(
            expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreClockEnvironmentOperations(
            expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreFilePathOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreThreadOperations(expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreAtomicOperations(
            expression, declaringType, selectedMember,
            methodMemberOrdinal, scopeOrdinal, result, diagnostics);
        recognizeCoreMutexMonitorOperations(
            expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreSemaphoreEventOperations(
            expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
        recognizeCoreDiagnosticsOperations(
            expression, declaringType, selectedMember, methodMemberOrdinal, scopeOrdinal);
    }
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeAssertionIntrinsicCall(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    // T07-F06: compiler-owned assertion family. These bindings are formed before ordinary name
    // lookup because the parser marks only unqualified contextual call heads. The argument is
    // analyzed exactly once here and later becomes the single HIR/MIR child.
    const std::string_view assertionIntrinsic = expression->auxiliaryText();
    if (assertionIntrinsic == "builtin-assert" ||
        assertionIntrinsic == "builtin-unreachable" ||
        assertionIntrinsic == "builtin-assume" ||
        assertionIntrinsic == "builtin-panic")
    {
        const bool hasCondition = assertionIntrinsic == "builtin-assert" ||
            assertionIntrinsic == "builtin-assume";
        const bool isPanic = assertionIntrinsic == "builtin-panic";
        if (!validateAssertionIntrinsicArguments(
                expression, assertionIntrinsic, hasCondition, isPanic,
                methodMemberOrdinal, scopeOrdinal, diagnostics)) return ExpressionResult{};

        MethodCoreBinding binding;
        binding.kind = assertionIntrinsic == "builtin-assert"
            ? MethodCoreOperationKind::Assert
            : assertionIntrinsic == "builtin-unreachable"
                ? MethodCoreOperationKind::Unreachable
                : assertionIntrinsic == "builtin-assume"
                    ? MethodCoreOperationKind::Assume
                    : MethodCoreOperationKind::Panic;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = scopeOrdinal;
        binding.resultTypeKey = hasCondition ? "builtin:void" : "builtin:never";
        coreBindings_.push_back(std::move(binding));
        result.typeKey = hasCondition ? "builtin:void" : "builtin:never";
        result.valid = true;
        if (!hasCondition) ownershipFlowReachable_ = false;
        return result;
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::validateAssertionIntrinsicArguments(
    const ast::AstNode* expression,
    std::string_view assertionIntrinsic,
    bool hasCondition,
    bool isPanic,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::string_view displayName = assertionIntrinsic.substr(8);
    const std::size_t expectedArguments = hasCondition ? 1U : 0U;
    const bool invalidArity = isPanic
        ? expression->children().size() > 1U
        : expression->children().size() != expectedArguments;
    if (invalidArity || !expression->typeSyntaxes().empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            std::string(displayName) + (isPanic
                ? "() accepts zero or one string payload and no generic type arguments"
                : "() requires exactly " + std::to_string(expectedArguments) +
                    " value argument" + (expectedArguments == 1U ? "" : "s") +
                " and no generic type arguments"));
        return false;
    }
    if (hasCondition)
    {
        const ExpressionResult condition = analyzeExpression(
            expression->children().front(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (!condition.valid) return false;
        if (condition.typeKey != "builtin:bool")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCondition,
                diagnostics::DiagnosticSeverity::Error,
                expression->children().front()->range(),
                std::string(displayName) + "() condition must have type bool");
            return false;
        }
    }
    else if (isPanic && !expression->children().empty())
    {
        const ExpressionResult payload = analyzeExpression(
            expression->children().front(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (!payload.valid) return false;
        if (payload.typeKey != "builtin:string" && payload.typeKey != "string-literal")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->children().front()->range(),
                "panic() payload must have type string");
            return false;
        }
    }
    return true;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeNamespaceCallHead(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (namespaceFunctionModel_ != nullptr)
    {
        if (const NamespaceFunctionReferenceBinding* reference =
                namespaceFunctionModel_->referenceBinding(expression);
            reference != nullptr)
        {
            bool shadowed = lookupScopeName(scopeOrdinal,
                expression->first() != nullptr ? expression->first()->text() : std::string{}).has_value();
            if (!reference->qualified && !shadowed && memberModel_ != nullptr)
            {
                const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
                if (caller != nullptr && caller->declaringTypeOrdinal < nameLookup_->namedTypes().size())
                {
                    for (const std::size_t ordinal : memberModel_->findMembers(
                             caller->declaringTypeOrdinal,
                             expression->first() != nullptr ? expression->first()->text() : std::string{}))
                    {
                        if (ordinal < memberModel_->members().size() &&
                            memberModel_->members()[ordinal].kind == ClassMemberKind::Method)
                        {
                            shadowed = true;
                            break;
                        }
                    }
                }
            }
            if (reference->qualified || !shadowed)
            {
                return analyzeNamespaceFunctionCall(
                    expression, *reference, methodMemberOrdinal, scopeOrdinal, diagnostics);
            }
        }
    }
    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeListPatternRestCall(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    // NS304-T03-F03: list-pattern rest capture `.. var x`. For a receiver without a
    // public Slice(int,int) -- native array, inline array, native string -- synthesize
    // the borrowed view directly. Other receivers fall through to ordinary Slice
    // method resolution below. (This first slice covers the non-generic StringView
    // case; array/inline synthesize Span<T>/ReadOnlySpan<T> in a later step.)
    if (expression->auxiliaryText() == "list-pattern-rest" &&
        expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::MemberExpression &&
        expression->first()->first() != nullptr)
    {
        ExpressionResult restReceiver = analyzeExpression(
            expression->first()->first(), methodMemberOrdinal, scopeOrdinal, diagnostics);
        const bool nativeString = restReceiver.valid && !restReceiver.methodGroup &&
            (ProjectTypeConversion::isNativeString(restReceiver.typeKey) ||
             restReceiver.typeKey == "string-literal");
        if (nativeString)
        {
            if (const auto handled = analyzeNativeStringPatternRestCall(
                    expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
                handled.has_value()) return *handled;
        }
        if (const auto handled = analyzeArrayPatternRestCall(
                expression, restReceiver, methodMemberOrdinal, scopeOrdinal, diagnostics);
            handled.has_value()) return *handled;
    }

    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzePreCallKinds(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (const auto handled = analyzeAssertionIntrinsicCall(
            expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return handled;
    if (const auto handled = analyzeNamespaceCallHead(
            expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return handled;
    if (const auto handled = analyzeListPatternRestCall(
            expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return handled;
    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ExpressionResult>
ProjectMethodBodyModel::analyzeNativeStringPatternRestCall(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool argumentsValid = expression->children().size() == 2;
    for (const ast::AstNode* child : expression->children())
    {
        const ExpressionResult arg = analyzeExpression(
            child, methodMemberOrdinal, scopeOrdinal, diagnostics);
        argumentsValid = argumentsValid && arg.valid && !arg.methodGroup;
    }
    if (!argumentsValid) return std::nullopt;
    // La clave de tipo de un named type SIEMPRE lleva el modulo: `type:<modulo>::...`.
    if (coreStringViewTypeKey_.empty()) return ExpressionResult{};
    coreBindings_.push_back(MethodCoreBinding{
        MethodCoreOperationKind::StringViewFromString, expression, methodMemberOrdinal,
        scopeOrdinal, coreStringViewTypeKey_, "builtin:string",
        false, std::nullopt, std::nullopt});
    ExpressionResult viewResult;
    viewResult.typeKey = coreStringViewTypeKey_;
    viewResult.valid = true;
    viewResult.assignable = false;
    viewResult.addressable = false;
    return viewResult;
}

std::optional<ProjectMethodBodyModel::ExpressionResult>
ProjectMethodBodyModel::analyzeArrayPatternRestCall(
    const ast::AstNode* expression,
    const ExpressionResult& restReceiver,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NS304-T03-F03: array nativo / inline array sin Slice publico -> sintetiza
    // Span<T> (receptor mutable) o ReadOnlySpan<T> (receptor solo lectura) sin copia.
    const std::optional<std::string> arrayElement =
        restReceiver.valid && !restReceiver.methodGroup &&
                ProjectTypeConversion::isNativeArray(restReceiver.typeKey)
            ? ProjectTypeConversion::nativeArrayElementType(restReceiver.typeKey)
            : std::nullopt;
    const std::optional<std::string> fixedElement =
        !arrayElement.has_value() && restReceiver.valid && !restReceiver.methodGroup
            ? ProjectMemberModel::fixedArrayElementTypeKey(restReceiver.typeKey)
            : std::nullopt;
    if (!arrayElement.has_value() && !fixedElement.has_value()) return std::nullopt;
    bool argumentsValid = expression->children().size() == 2;
    for (const ast::AstNode* child : expression->children())
    {
        const ExpressionResult arg = analyzeExpression(
            child, methodMemberOrdinal, scopeOrdinal, diagnostics);
        argumentsValid = argumentsValid && arg.valid && !arg.methodGroup;
    }
    if (!argumentsValid) return std::nullopt;
    const std::string& elementKey = arrayElement.has_value() ? *arrayElement : *fixedElement;
    const bool readOnly = restReceiver.readonlyReceiver;
    std::string resultKey = genericModel_ != nullptr
        ? genericModel_->closedTypeKeyForTemplate(readOnly ? "ReadOnlySpan" : "Span", elementKey)
        : std::string{};
    const bool materialized = genericModel_ != nullptr && !resultKey.empty() &&
        genericModel_->specializationForTypeKey(resultKey) != nullptr;
    if (materialized)
    {
        MethodCoreOperationKind kind = MethodCoreOperationKind::SpanFromArray;
        if (fixedElement.has_value())
            kind = readOnly ? MethodCoreOperationKind::ReadOnlySpanFromFixedArray
                            : MethodCoreOperationKind::SpanFromFixedArray;
        else if (readOnly)
            kind = MethodCoreOperationKind::ReadOnlySpanFromArray;
        coreBindings_.push_back(MethodCoreBinding{
            kind, expression, methodMemberOrdinal, scopeOrdinal, resultKey, elementKey,
            false, std::nullopt, std::nullopt});
        ExpressionResult viewResult;
        viewResult.typeKey = std::move(resultKey);
        viewResult.valid = true;
        viewResult.assignable = false;
        viewResult.addressable = false;
        return viewResult;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "array/inline rest capture requires a materialized Span<T>/ReadOnlySpan<T>");
    return ExpressionResult{};
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeEnumRootCall(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    // C8 "type root", struct half. The member access above resolved to a synthesized struct
    // member (see the facade at the end of analyzeMember); this is where its arity and argument
    // are checked and the core binding is recorded. Everything downstream -- HIR, MIR, LLVM --
    // ID 129. La llamada al `ToString()` sintetizado de un enum. Cero argumentos, y el tipo del
    // enum viaja en `elementTypeKey` para que el HIR pueda buscar sus enumeradores.
    if (callee.methodGroup && callee.typeKey == kEnumRootFacadeTypeKey)
    {
        if (!expression->children().empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "the compiler-provided 'ToString' on an enum takes no arguments");
            return ExpressionResult{};
        }
        // NO HAY RESTRICCION SOBRE EL RECEPTOR. La primera version de esto exigia un receptor
        // re-evaluable --la misma lista que usa la asignacion compuesta sobre propiedades-- porque
        // la bajada era una cadena de condicionales que bajaba el receptor una vez por enumerador,
        // y `Dame().ToString()` habria llamado a `Dame()` una vez por nombre.
        //
        // Eso era una limitacion del COMPILADOR escrita como si fuera una regla del LENGUAJE. La
        // bajada usa ahora un nodo propio que baja el valor UNA SOLA VEZ (ProjectOopHirModel.cpp,
        // ProjectOopHirNodeKind::EnumName), asi que la restriccion sobra y cualquier expresion de
        // tipo enum vale como receptor.
        coreBindings_.push_back(MethodCoreBinding{
            MethodCoreOperationKind::EnumToString,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            "builtin:string",
            callee.receiverTypeKey,
            false, std::nullopt, std::nullopt});
        ExpressionResult enumResult;
        enumResult.typeKey = "builtin:string";
        enumResult.valid = true;
        return enumResult;
    }

    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeIteratorFacadeCall(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    // NS304-T06-F08: `it.MoveNext()`/`it.GetEnumerator()` zero-argument calls over an opaque
    // generator iterator. MoveNext returns builtin:bool; GetEnumerator returns the iterator itself.
    if (callee.methodGroup && callee.typeKey == kIteratorFacadeTypeKey)
    {
        if (!expression->children().empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "the compiler-provided iterator MoveNext/GetEnumerator takes no arguments");
            return ExpressionResult{};
        }
        const std::string& memberName = call.methodName;
        const bool isMoveNext = memberName == "MoveNext";
        const bool isDrop = memberName == "Drop";
        const bool isMoveNextAsync = memberName == "MoveNextAsync";
        const bool isCancel = memberName == "Cancel";
        // NS304-T07-F02/F03: MoveNextAsync returns the future of the next demand with the
        // async-iterator's error type (future:<bool>|<E>).
        const std::optional<std::string_view> asyncError =
            language::asyncIteratorErrorTypeKey(callee.receiverTypeKey);
        const std::string moveNextAsyncResult = language::futureTypeKey(
            "builtin:bool", asyncError.has_value() ? std::string(*asyncError) : std::string{});
        const std::string resultTypeKey = isMoveNext
            ? "builtin:bool"
            : isMoveNextAsync ? moveNextAsyncResult
            : isDrop ? "builtin:void" : isCancel ? "builtin:void" : callee.receiverTypeKey;
        // NS304-T07-F04: a `ref`/`ref mut` Current borrow lives only until the next demand or
        // destruction. Advancing or dropping the iterator while that borrow is live is rejected.
        if (isMoveNext || isMoveNextAsync || isDrop)
        {
            bool receiverPrecise = true;
            const std::optional<std::string> receiverPlace = storagePlaceKey(
                callee.receiverSyntax, scopeOrdinal, &receiverPrecise);
            if (receiverPlace.has_value() &&
                conflictingMutableReference(*receiverPlace, scopeOrdinal).has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->range(),
                    "cannot advance or drop the iterator while a reference to its Current item is still live");
                return ExpressionResult{};
            }
        }
        coreBindings_.push_back(MethodCoreBinding{
            isMoveNext ? MethodCoreOperationKind::IteratorMoveNext
                       : isMoveNextAsync ? MethodCoreOperationKind::IteratorMoveNextAsync
                       : isDrop ? MethodCoreOperationKind::IteratorDrop
                       : isCancel ? MethodCoreOperationKind::IteratorCancel
                                : MethodCoreOperationKind::IteratorGetEnumerator,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            resultTypeKey,
            callee.receiverTypeKey,
            false, std::nullopt, std::nullopt});
        ExpressionResult iteratorResult;
        iteratorResult.typeKey = resultTypeKey;
        iteratorResult.valid = true;
        return iteratorResult;
    }

    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeStructRootCall(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    // then treats it exactly like any other core operation.
    if (callee.methodGroup && callee.typeKey == kStructRootFacadeTypeKey)
    {
        const std::string& memberName = call.methodName;
        const std::string& structTypeKey = callee.receiverTypeKey;
        std::string resultTypeKey;
        std::size_t expectedArity = 0;
        const std::optional<MethodCoreOperationKind> structKind = structRootOperation(
            memberName, structTypeKey, resultTypeKey, expectedArity);
        if (!structKind.has_value()) return ExpressionResult{};
        if (expression->children().size() != expectedArity)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "the compiler-provided '" + memberName + "' on struct '" + structTypeKey +
                    "' takes " + std::to_string(expectedArity) + " argument(s)");
            return ExpressionResult{};
        }
        // A synthesized Equals compares a struct with ITS OWN type. Widening it to take a root
        // reference would mean boxing, which this language does not do; a struct that wants to
        // compare against anything else declares its own Equals.
        if (expectedArity == 1)
        {
            const ExpressionResult other = analyzeExpression(
                expression->children().front(), methodMemberOrdinal, scopeOrdinal, diagnostics);
            if (!other.valid) return ExpressionResult{};
            if (other.typeKey != structTypeKey)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->children().front()->range(),
                    "the compiler-provided 'Equals' on struct '" + structTypeKey +
                        "' compares against its own type, not '" + other.typeKey +
                        "'; declare an Equals of your own to compare against anything else");
                return ExpressionResult{};
            }
        }
        if (!validateStructRootOperation(
                expression, *structKind, memberName, structTypeKey, diagnostics))
            return ExpressionResult{};
        coreBindings_.push_back(MethodCoreBinding{
            *structKind,
            expression,
            methodMemberOrdinal,
            scopeOrdinal,
            resultTypeKey,
            structTypeKey,
            false, std::nullopt, std::nullopt});
        result.typeKey = resultTypeKey;
        result.valid = true;
        result.assignable = false;
        return result;
    }

    return std::nullopt;
}

std::optional<MethodCoreOperationKind> ProjectMethodBodyModel::structRootOperation(
    std::string_view memberName,
    const std::string& structTypeKey,
    std::string& resultTypeKey,
    std::size_t& expectedArity)
{
    if (memberName == "ToString")
    {
        resultTypeKey = "builtin:string";
        return MethodCoreOperationKind::StructToString;
    }
    if (memberName == "GetHashCode")
    {
        resultTypeKey = "builtin:int";
        return MethodCoreOperationKind::StructHashCode;
    }
    if (memberName == "Equals")
    {
        resultTypeKey = "builtin:bool";
        expectedArity = 1;
        return MethodCoreOperationKind::StructEquals;
    }
    if (memberName == "Clone")
    {
        resultTypeKey = structTypeKey;
        return MethodCoreOperationKind::StructClone;
    }
    return std::nullopt;
}

bool ProjectMethodBodyModel::validateStructRootOperation(
    const ast::AstNode* expression,
    MethodCoreOperationKind structKind,
    const std::string& memberName,
    const std::string& structTypeKey,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // Equality and hashing read every scalar leaf of the value. A union has no such reading:
    // only one of its members is live at a time and nothing in the value says which. C refuses
    // to compare unions for the same reason, and guessing here would be worse than refusing.
    if (structKind != MethodCoreOperationKind::StructToString && aggregateIsUnion(structTypeKey))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "union '" + structTypeKey + "' has no compiler-provided '" + memberName +
                "': only one member of a union is live at a time, so comparing or hashing "
                "every field would be meaningless; declare your own");
        return false;
    }
    std::string unsupportedReason;
    if (!aggregateSupportsSynthesizedMember(structTypeKey, memberName, unsupportedReason))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            unsupportedReason);
        return false;
    }
    return true;
}

std::optional<std::size_t> ProjectMethodBodyModel::callNamedStorageKey(
    const ast::AstNode* syntax,
    std::size_t scopeOrdinal) const
{
    if (syntax == nullptr || syntax->kind() != ast::AstNodeKind::IdentifierExpression)
        return std::nullopt;
    const std::optional<ScopeName> name = lookupScopeName(scopeOrdinal, syntax->text());
    if (!name.has_value() ||
        (name->kind != ScopeNameKind::Local && name->kind != ScopeNameKind::Parameter))
        return std::nullopt;
    return name->semanticOrdinal * 2U +
        (name->kind == ScopeNameKind::Parameter ? 1U : 0U);
}

std::optional<std::size_t> ProjectMethodBodyModel::callMarkerCase(std::string_view marker)
{
    const std::size_t separator = marker.rfind(':');
    if (separator == std::string_view::npos || separator + 1U >= marker.size())
        return std::nullopt;
    std::size_t value = 0;
    const auto [end, error] = std::from_chars(
        marker.data() + separator + 1U, marker.data() + marker.size(), value);
    return error == std::errc{} && end == marker.data() + marker.size()
        ? std::optional<std::size_t>(value) : std::nullopt;
}

void ProjectMethodBodyModel::seedCallLambdaTargets(
    std::vector<std::string>& lambdaTargetTypes,
    const ast::AstNode* expression,
    const std::vector<std::string>& parameterTypes) const
{
    if (parameterTypes.size() != expression->children().size()) return;
    for (std::size_t index = 0; index < parameterTypes.size(); ++index)
    {
        if (expression->children()[index]->kind() == ast::AstNodeKind::LambdaExpression &&
            isDelegateTypeKey(parameterTypes[index]))
        {
            lambdaTargetTypes[index] = parameterTypes[index];
        }
    }
}

void ProjectMethodBodyModel::analyzeCallArguments(
    std::vector<ExpressionResult>& arguments,
    bool& allArgumentsValid,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const MethodCallBinding& call,
    std::span<const std::string> lambdaTargetTypes,
    bool coreStringForwarding,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < expression->children().size(); ++argumentOrdinal)
    {
        const ast::AstNode* argument = expression->children()[argumentOrdinal];
        ExpressionResult argumentResult;
        if (argument->kind() == ast::AstNodeKind::LambdaExpression &&
            !lambdaTargetTypes[argumentOrdinal].empty())
        {
            argumentResult = bindLambdaToDelegate(
                argument, lambdaTargetTypes[argumentOrdinal],
                methodMemberOrdinal, scopeOrdinal, diagnostics);
        }
        else
        {
            argumentResult = analyzeExpression(
                argument, methodMemberOrdinal, scopeOrdinal, diagnostics, true, false, true);
            if (argumentResult.methodGroup && callee.methodGroup)
            {
                std::vector<std::string> targets;
                for (const std::size_t candidateOrdinal : callee.methodCandidates)
                {
                    const ClassMemberSymbol* candidate = methodSymbol(candidateOrdinal);
                    if (candidate == nullptr || candidate->kind != ClassMemberKind::Method) continue;
                    const NamedArgumentMap mapping = mapArgumentsToParameters(
                        expression->children(), *candidate, coreStringForwarding ? 1U : 0U);
                    if (!mapping.valid || argumentOrdinal >= mapping.explicitParameterOrdinals.size())
                        continue;
                    const std::size_t parameterOrdinal =
                        mapping.explicitParameterOrdinals[argumentOrdinal];
                    if (parameterOrdinal >= candidate->parameters.size()) continue;
                    const std::string& target = candidate->parameters[parameterOrdinal].typeKey;
                    if (methodGroupExactlyMatchesDelegate(argumentResult, target) &&
                        std::ranges::find(targets, target) == targets.end())
                        targets.push_back(target);
                }
                if (targets.size() == 1)
                {
                    argumentResult = bindMethodGroupToDelegate(
                        argument, argumentResult, targets.front(), methodMemberOrdinal,
                        scopeOrdinal, diagnostics);
                }
                else
                {
                    diagnostics.report(
                        targets.empty() ? diagnostics::DiagnosticId::InvalidConversion
                                        : diagnostics::DiagnosticId::InvalidCall,
                        diagnostics::DiagnosticSeverity::Error,
                        argument->range(),
                        targets.empty()
                            ? "method group does not match any delegate parameter of '" +
                                call.methodName + "'"
                            : "method group argument has more than one compatible delegate target in '" +
                                call.methodName + "'");
                }
            }
        }
        if (!argumentResult.valid || argumentResult.methodGroup) allArgumentsValid = false;
        arguments.push_back(std::move(argumentResult));
    }
}

const ast::AstNode* ProjectMethodBodyModel::resolvedCallArgumentSyntax(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::size_t ordinal,
    bool coreStringForwarding) const
{
    if (coreStringForwarding)
    {
        if (ordinal == 0) return callee.receiverSyntax;
        --ordinal;
    }
    return ordinal < expression->children().size() ? expression->children()[ordinal] : nullptr;
}

bool ProjectMethodBodyModel::callIsFullExpressionBorrow(
    const ClassMemberSymbol& candidate,
    std::size_t argumentOrdinal,
    const ExpressionResult& argument) const
{
    if (!argument.valid || !argument.ownershipFreshOwner || !argument.ownershipTransfer ||
        argument.ownership != sema::OwnershipKind::Owned ||
        argumentOrdinal >= candidate.parameters.size() ||
        candidate.parameters[argumentOrdinal].typeKey != "builtin:string")
    {
        return false;
    }
    if (candidate.parameters[argumentOrdinal].ownership == sema::OwnershipKind::Borrowed)
        return true;
    return candidate.parameters[argumentOrdinal].ownership == sema::OwnershipKind::None;
}

bool ProjectMethodBodyModel::callIsStringLiteralBorrow(
    const ClassMemberSymbol& candidate,
    std::size_t parameterOrdinal,
    const ExpressionResult& argument) noexcept
{
    return parameterOrdinal < candidate.parameters.size() &&
        candidate.parameters[parameterOrdinal].ownership == sema::OwnershipKind::Borrowed &&
        candidate.parameters[parameterOrdinal].typeKey == "builtin:string" &&
        argument.typeKey == "string-literal";
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeGenericLambdaSpecializedCall(
    const ast::AstNode* expression,
    [[maybe_unused]] const ExpressionResult& callee,
    std::vector<ExpressionResult>& arguments,
    bool allArgumentsValid,
    [[maybe_unused]] MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (expression->auxiliaryText() == "generic-lambda-specialized-call")
    {
        if (!allArgumentsValid || expression->second() == nullptr ||
            expression->typeSyntax() == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr)
            return result;
        const std::string targetType =
            memberModel_->canonicalTypeKey(expression->typeSyntax(), *nameLookup_);
        const ExpressionResult specialized = bindLambdaToDelegate(
            expression->second(), targetType, methodMemberOrdinal, scopeOrdinal, diagnostics);
        if (!specialized.valid) return result;
        const auto lambda = std::ranges::find_if(
            lambdaBindings_.rbegin(), lambdaBindings_.rend(), [&](const MethodLambdaBinding& item)
            {
                return item.syntax == expression->second();
            });
        if (lambda == lambdaBindings_.rend()) return result;

        std::string returnType;
        std::vector<std::string> parameterTypes;
        std::size_t delegateOrdinal = 0;
        if (!delegateSignature(targetType, returnType, parameterTypes, delegateOrdinal)) return result;
        if (parameterTypes.size() != arguments.size())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "generic lambda invocation argument count does not match its specialized signature");
            return result;
        }

        MethodDelegateBinding binding;
        binding.kind = MethodDelegateOperationKind::Invoke;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = scopeOrdinal;
        binding.delegateTypeKey = targetType;
        binding.delegateTypeOrdinal = delegateOrdinal;
        binding.directLambdaName = lambda->generatedName;
        binding.directLambdaUsesEnvironment = true;
        binding.resultTypeKey = returnType;
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            if (memberModel_->typeIsMoveOnly(parameterTypes[index]) &&
                arguments[index].addressable && !arguments[index].ownershipTransfer)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->children()[index]->range(),
                    "move-only argument cannot be copied into a by-value generic-lambda parameter; use move(argument)");
                return result;
            }
            ProjectImplicitConversion conversion = classifyImplicit(
                arguments[index].typeKey, parameterTypes[index], arguments[index].integerConstant);
            if (!conversion.exists())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->children()[index]->range(),
                    "generic lambda argument is not implicitly convertible to its specialized parameter type");
                return result;
            }
            binding.argumentConversions.push_back(std::move(conversion));
        }
        delegateBindings_.push_back(std::move(binding));
        result.typeKey = returnType;
        result.valid = true;
        return result;
    }

    return std::nullopt;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeEventInvocationCall(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::vector<ExpressionResult>& arguments,
    bool allArgumentsValid,
    MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    (void)call;
    ExpressionResult result;
    if (callee.eventMemberOrdinal.has_value())
    {
        if (!allArgumentsValid || memberModel_ == nullptr ||
            *callee.eventMemberOrdinal >= memberModel_->members().size())
        {
            return result;
        }
        const ClassMemberSymbol& event = memberModel_->members()[*callee.eventMemberOrdinal];
        const ClassMemberSymbol* caller = methodSymbol(methodMemberOrdinal);
        if (caller == nullptr || caller->declaringTypeOrdinal != event.declaringTypeOrdinal)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InaccessibleMember,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "Stage 42 events may only be invoked from within their declaring type");
            return result;
        }
        std::string returnType;
        std::vector<std::string> parameterTypes;
        std::size_t delegateOrdinal = 0;
        if (!delegateSignature(event.declaredTypeKey, returnType, parameterTypes, delegateOrdinal))
        {
            return result;
        }
        if (parameterTypes.size() != arguments.size())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "event invocation argument count does not match its delegate signature");
            return result;
        }
        MethodEventBinding binding;
        binding.kind = MethodEventOperationKind::Invoke;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = scopeOrdinal;
        binding.eventMemberOrdinal = *callee.eventMemberOrdinal;
        const std::optional<std::size_t> backing = eventBackingMember(*callee.eventMemberOrdinal);
        if (!backing.has_value()) return result;
        binding.backingMemberOrdinal = *backing;
        binding.delegateTypeKey = event.declaredTypeKey;
        binding.delegateTypeOrdinal = delegateOrdinal;
        binding.receiverSyntax = callee.receiverSyntax;
        binding.receiverTypeKey = callee.receiverTypeKey;
        binding.receiverTypeOrdinal = callee.receiverTypeOrdinal;
        binding.staticEvent = event.isStatic;
        binding.resultTypeKey = returnType;
        if (!bindEventInvocationArguments(
                expression, arguments, parameterTypes, binding, diagnostics)) return result;
        eventBindings_.push_back(std::move(binding));
        result.typeKey = returnType;
        result.valid = true;
        return result;
    }

    return std::nullopt;
}

bool ProjectMethodBodyModel::bindEventInvocationArguments(
    const ast::AstNode* expression,
    std::span<const ExpressionResult> arguments,
    std::span<const std::string> parameterTypes,
    MethodEventBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        if (memberModel_->typeIsMoveOnly(parameterTypes[index]) &&
            arguments[index].addressable && !arguments[index].ownershipTransfer)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                diagnostics::DiagnosticSeverity::Error,
                expression->children()[index]->range(),
                "move-only event argument cannot be copied into a by-value delegate parameter; use move(argument)");
            return false;
        }
        ProjectImplicitConversion conversion = classifyImplicit(
            arguments[index].typeKey, parameterTypes[index], arguments[index].integerConstant);
        if (!conversion.exists())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                expression->children()[index]->range(),
                "event invocation argument is not implicitly convertible to delegate parameter type");
            return false;
        }
        binding.argumentConversions.push_back(std::move(conversion));
    }
    return true;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeDelegateInvocationCall(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::vector<ExpressionResult>& arguments,
    bool allArgumentsValid,
    MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (!callee.methodGroup && isDelegateTypeKey(callee.typeKey))
    {
        if (!allArgumentsValid) return result;
        std::string returnType;
        std::vector<std::string> parameterTypes;
        std::size_t delegateOrdinal = 0;
        if (!delegateSignature(callee.typeKey, returnType, parameterTypes, delegateOrdinal)) return result;
        const std::optional<language::FunctionType> structuralFunction =
            language::parseFunctionTypeIdentity(callee.typeKey);
        const bool variadic = structuralFunction.has_value() && structuralFunction->variadic;
        // NS304-T04-F04: aplicacion de defaults en el call-site de delegate. Cada parametro
        // omitido debe tener un default consteval en la declaracion del delegate; su sintaxis se
        // guarda para que el HIR lo emita como argumento real.
        std::vector<const ast::AstNode*> defaultedSyntaxes;
        std::vector<std::string> defaultedTypeKeys;
        if (!collectDelegateDefaultArguments(
                defaultedSyntaxes, defaultedTypeKeys, expression, arguments.size(),
                parameterTypes, delegateOrdinal, variadic, diagnostics)) return result;
        MethodDelegateBinding binding;
        binding.kind = MethodDelegateOperationKind::Invoke;
        binding.syntax = expression;
        binding.methodMemberOrdinal = methodMemberOrdinal;
        binding.scopeOrdinal = scopeOrdinal;
        binding.delegateTypeKey = callee.typeKey;
        binding.delegateTypeOrdinal = delegateOrdinal;
        if (const std::optional<language::AnonymousLambdaType> lambdaType =
                language::parseAnonymousLambdaTypeIdentity(callee.typeKey))
            binding.directLambdaName = language::anonymousLambdaGeneratedName(*lambdaType);
        binding.resultTypeKey = returnType;
        if (!bindDelegateInvocationArguments(
                expression, arguments, parameterTypes, binding, diagnostics)) return result;
        if (!validateCallableTraitInvocation(
                expression, callee, scopeOrdinal, diagnostics)) return result;
        binding.selectedDefaultArguments = std::move(defaultedSyntaxes);
        binding.selectedDefaultArgumentTypeKeys = std::move(defaultedTypeKeys);
        // EN: Freeze 3.0 C-13: invoking a `throws E` delegate is a call to a `throws E` callable.
        // ES: Congelamiento 3.0 C-13: invocar un delegate `throws E` es llamar a un callable `throws E`.
        requireDeclaredErrorHandled(
            expression, delegateErrorTypeKey(callee.typeKey),
            expression->first() != nullptr ? expression->first()->text() : std::string_view("delegate"),
            methodMemberOrdinal, diagnostics);
        delegateBindings_.push_back(std::move(binding));
        result.typeKey = returnType;
        result.valid = true;
        return result;
    }

    call.candidateMemberOrdinals = callee.methodCandidates;
    return std::nullopt;
}

bool ProjectMethodBodyModel::collectDelegateDefaultArguments(
    std::vector<const ast::AstNode*>& defaultedSyntaxes,
    std::vector<std::string>& defaultedTypeKeys,
    const ast::AstNode* expression,
    std::size_t argumentCount,
    std::span<const std::string> parameterTypes,
    std::size_t delegateOrdinal,
    bool variadic,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!variadic && argumentCount < parameterTypes.size())
    {
        std::vector<const ast::AstNode*> delegateParameters;
        if (nameLookup_ != nullptr && delegateOrdinal < nameLookup_->namedTypes().size())
        {
            const ast::AstNode* delegateSyntax = nameLookup_->namedTypes()[delegateOrdinal].syntax;
            if (delegateSyntax != nullptr)
            {
                for (const ast::AstNode* child : delegateSyntax->children())
                {
                    if (child != nullptr && child->kind() == ast::AstNodeKind::ParameterDeclaration)
                        delegateParameters.push_back(child);
                }
            }
        }
        bool missingDefault = delegateParameters.size() != parameterTypes.size();
        for (std::size_t index = argumentCount;
             index < parameterTypes.size() && !missingDefault; ++index)
        {
            if (index >= delegateParameters.size() || delegateParameters[index]->first() == nullptr)
                missingDefault = true;
            else
            {
                defaultedSyntaxes.push_back(delegateParameters[index]->first());
                defaultedTypeKeys.push_back(parameterTypes[index]);
            }
        }
        if (missingDefault)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "delegate invocation argument count does not match its signature");
            return false;
        }
    }
    if ((!variadic && parameterTypes.size() != argumentCount + defaultedSyntaxes.size()) ||
        (variadic && argumentCount < parameterTypes.size()))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "delegate invocation argument count does not match its signature");
        return false;
    }
    return true;
}

bool ProjectMethodBodyModel::bindDelegateInvocationArguments(
    const ast::AstNode* expression,
    std::span<const ExpressionResult> arguments,
    std::span<const std::string> parameterTypes,
    MethodDelegateBinding& binding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        if (index >= parameterTypes.size())
        {
            ProjectImplicitConversion conversion = classifyCVariadicPromotion(arguments[index].typeKey);
            if (!conversion.exists() ||
                arguments[index].argumentPassKind != ParameterPassKind::Value ||
                arguments[index].ownership != sema::OwnershipKind::None ||
                arguments[index].ownershipTransfer || arguments[index].ownershipFreshOwner)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidConversion,
                    diagnostics::DiagnosticSeverity::Error,
                    expression->children()[index]->range(),
                    "variadic function-pointer arguments must be by-value C ABI scalars, "
                    "raw pointers, enums, or function pointers");
                return false;
            }
            binding.argumentConversions.push_back(std::move(conversion));
            continue;
        }
        if (memberModel_->typeIsMoveOnly(parameterTypes[index]) &&
            arguments[index].addressable && !arguments[index].ownershipTransfer)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                diagnostics::DiagnosticSeverity::Error,
                expression->children()[index]->range(),
                "move-only argument cannot be copied into a by-value delegate parameter; use move(argument)");
            return false;
        }
        ProjectImplicitConversion conversion = classifyImplicit(
            arguments[index].typeKey, parameterTypes[index], arguments[index].integerConstant);
        if (!conversion.exists())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                expression->children()[index]->range(),
                "delegate invocation argument is not implicitly convertible to parameter type");
            return false;
        }
        binding.argumentConversions.push_back(std::move(conversion));
    }
    return true;
}

bool ProjectMethodBodyModel::validateCallableTraitInvocation(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!callee.callableTrait.has_value() || expression->first() == nullptr ||
        expression->first()->kind() != ast::AstNodeKind::IdentifierExpression) return true;
    const std::optional<ScopeName> callableName = lookupScopeName(
        scopeOrdinal, expression->first()->text());
    if (!callableName.has_value() || callableName->kind != ScopeNameKind::Local ||
        callableName->semanticOrdinal >= locals_.size()) return true;
    const std::size_t localOrdinal = callableName->semanticOrdinal;
    const MethodLocalSymbol& callableLocal = locals_[localOrdinal];
    if (*callee.callableTrait == language::CallableTrait::FnMut && callableLocal.isConst)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "FnMut closure local '" + callableLocal.name +
                "' cannot be invoked through a const binding");
        return false;
    }
    if (*callee.callableTrait != language::CallableTrait::FnOnce) return true;
    if (invokedFnOnceLocals_.contains(localOrdinal) ||
        maybeInvokedFnOnceLocals_.contains(localOrdinal))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::NS3020InvalidValueCategory,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "FnOnce closure local '" + callableLocal.name +
                "' was already invoked or may have been invoked on another path");
        return false;
    }
    invokedFnOnceLocals_.insert(localOrdinal);
    return true;
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::analyzeSpecialCallKinds(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::vector<ExpressionResult>& arguments,
    bool allArgumentsValid,
    MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (const auto handled = analyzeGenericLambdaSpecializedCall(
            expression, callee, arguments, allArgumentsValid, call,
            methodMemberOrdinal, scopeOrdinal, diagnostics); handled.has_value()) return handled;
    if (const auto handled = analyzeEventInvocationCall(
            expression, callee, arguments, allArgumentsValid, call,
            methodMemberOrdinal, scopeOrdinal, diagnostics); handled.has_value()) return handled;
    if (const auto handled = analyzeDelegateInvocationCall(
            expression, callee, arguments, allArgumentsValid, call,
            methodMemberOrdinal, scopeOrdinal, diagnostics); handled.has_value()) return handled;
    return std::nullopt;
}

void ProjectMethodBodyModel::evaluateCallCandidates(
    bool& hasMatchingArity,
    std::vector<const ast::AstNode*>& constraintRejectedSyntaxes,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    bool coreStringForwarding,
    MethodCallBinding& call)
{
    hasMatchingArity = false;
    // Las candidatas que eran viables por tipos y propiedad y cayeron SOLO por su `where`. Si al
    // final no queda ninguna viable, estas son las que explican por que.
    constraintRejectedSyntaxes.clear();
    if (memberModel_ == nullptr) return;
    call.evaluatedCandidates.reserve(callee.methodCandidates.size());
    call.viableCandidateMemberOrdinals.reserve(callee.methodCandidates.size());
    for (const std::size_t candidateOrdinal : callee.methodCandidates)
    {
        if (candidateOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
        if (candidate.kind != ClassMemberKind::Method) continue;
        MethodCallCandidate candidateBinding;
        candidateBinding.memberOrdinal = candidateOrdinal;
        candidateBinding.isVariadicCandidate = candidate.isVariadic;
        if (!prepareCallCandidateMapping(
                candidateBinding, candidate, expression,
                resolvedArguments, coreStringForwarding))
        {
            call.evaluatedCandidates.push_back(std::move(candidateBinding));
            continue;
        }
        hasMatchingArity = true;
        candidateBinding.viable = true;
        evaluateCallCandidateArguments(candidateBinding, candidate, resolvedArguments);
        finishCallCandidateEvaluation(
            constraintRejectedSyntaxes, candidateBinding, candidate, candidateOrdinal, call);
        call.evaluatedCandidates.push_back(std::move(candidateBinding));
    }
}

bool ProjectMethodBodyModel::prepareCallCandidateMapping(
    MethodCallCandidate& candidateBinding,
    const ClassMemberSymbol& candidate,
    const ast::AstNode* expression,
    std::span<const ExpressionResult> resolvedArguments,
    bool coreStringForwarding) const
{
    const NamedArgumentMap explicitMapping = mapArgumentsToParameters(
        expression->children(), candidate, coreStringForwarding ? 1U : 0U);
    if (!explicitMapping.valid) return false;
    candidateBinding.argumentParameterOrdinals = explicitMapping.explicitParameterOrdinals;
    candidateBinding.defaultedParameterOrdinals = explicitMapping.defaultedParameterOrdinals;
    candidateBinding.parameterPackElements = explicitMapping.parameterPackElements;
    candidateBinding.parameterPackOrdinal = explicitMapping.parameterPackOrdinal;
    candidateBinding.usesExpandedParameterPack = explicitMapping.usesExpandedParameterPack;
    candidateBinding.synthesizesEmptyParameterPack = explicitMapping.synthesizesEmptyParameterPack;
    candidateBinding.defaultedParameterCount = explicitMapping.defaultedParameterOrdinals.size();
    if (coreStringForwarding)
    {
        candidateBinding.argumentParameterOrdinals.insert(
            candidateBinding.argumentParameterOrdinals.begin(), 0U);
        candidateBinding.parameterPackElements.insert(
            candidateBinding.parameterPackElements.begin(), false);
    }
    if (!candidateBinding.usesExpandedParameterPack ||
        std::ranges::count(candidateBinding.parameterPackElements, true) != 1 ||
        !candidateBinding.parameterPackOrdinal.has_value()) return true;
    const auto packElement = std::ranges::find(candidateBinding.parameterPackElements, true);
    const std::size_t argumentOrdinal = static_cast<std::size_t>(
        std::distance(candidateBinding.parameterPackElements.begin(), packElement));
    const MethodParameterSymbol& pack = candidate.parameters[*candidateBinding.parameterPackOrdinal];
    if (classifyImplicit(
            resolvedArguments[argumentOrdinal].typeKey,
            pack.typeKey,
            resolvedArguments[argumentOrdinal].integerConstant).exists())
    {
        candidateBinding.parameterPackElements[argumentOrdinal] = false;
        candidateBinding.usesExpandedParameterPack = false;
    }
    return true;
}

void ProjectMethodBodyModel::evaluateCallCandidateArguments(
    MethodCallCandidate& candidateBinding,
    const ClassMemberSymbol& candidate,
    std::span<const ExpressionResult> resolvedArguments) const
{
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < resolvedArguments.size(); ++argumentOrdinal)
    {
        evaluateCallCandidateArgument(
            candidateBinding, candidate, resolvedArguments[argumentOrdinal], argumentOrdinal);
    }
}

void ProjectMethodBodyModel::evaluateCallCandidateArgument(
    MethodCallCandidate& candidateBinding,
    const ClassMemberSymbol& candidate,
    const ExpressionResult& argument,
    std::size_t argumentOrdinal) const
{
    const std::size_t parameterOrdinal = candidateBinding.argumentParameterOrdinals[argumentOrdinal];
    if (parameterOrdinal >= candidate.parameters.size())
    {
        ProjectImplicitConversion conversion = classifyCVariadicPromotion(argument.typeKey);
        if (!conversion.exists() || argument.argumentPassKind != ParameterPassKind::Value ||
            argument.ownership != sema::OwnershipKind::None ||
            argument.ownershipTransfer || argument.ownershipFreshOwner)
        {
            candidateBinding.viable = false;
            candidateBinding.invalidVariadicArgumentOrdinal = argumentOrdinal;
        }
        candidateBinding.conversions.push_back(std::move(conversion));
        return;
    }
    const bool packElement = argumentOrdinal < candidateBinding.parameterPackElements.size() &&
        candidateBinding.parameterPackElements[argumentOrdinal];
    std::string conversionTarget = candidate.parameters[parameterOrdinal].typeKey;
    if (packElement)
    {
        const std::string& elementType =
            candidate.parameters[parameterOrdinal].parameterPackElementTypeKey;
        if (elementType.empty()) candidateBinding.viable = false;
        else conversionTarget = elementType;
    }
    ProjectImplicitConversion conversion = classifyImplicit(
        argument.typeKey, conversionTarget, argument.integerConstant);
    const MethodParameterSymbol& parameter = candidate.parameters[parameterOrdinal];
    // EN: Freeze 3.0 P3: as above, an `in` parameter accepts a readonly place.
    // ES: Congelamiento 3.0 P3: como arriba, un parametro `in` acepta un lugar readonly.
    const bool parameterPointeeReadonly =
        parameter.pointerPointeeReadonly || parameter.passKind == ParameterPassKind::In;
    const bool discardsPointeeConst = !packElement && argument.pointerPointeeReadonly &&
        ProjectTypeConversion::isPointer(parameter.typeKey) && !parameterPointeeReadonly;
    const bool violatesPointerContract = !packElement &&
        ProjectTypeConversion::isPointer(parameter.typeKey) &&
        !language::pointerContractImplicitlyAssignable(
            pointerContract(
                argument.pointerNullable, argument.pointerPointeeReadonly,
                argument.pointerHasProvenance, argument.pointerKnownAlignment,
                argument.pointerAddressSpace),
            pointerContract(
                parameter.pointerNullable, parameterPointeeReadonly,
                parameter.pointerHasProvenance, parameter.pointerKnownAlignment,
                parameter.pointerAddressSpace));
    if (!conversion.exists() || discardsPointeeConst || violatesPointerContract)
        candidateBinding.viable = false;
    else if (packElement &&
        (argument.ownership != sema::OwnershipKind::None || argument.ownershipTransfer ||
         argument.ownershipFreshOwner || argument.argumentPassKind != ParameterPassKind::Value))
        candidateBinding.viable = false;
    else
    {
        const sema::OwnershipKind parameterOwnership = parameter.ownership;
        // Stage 52: an owned/borrowed qualifier written on a generic parameter
        // ("owned T", "borrowed T") monomorphizes to a real qualifier even when T
        // is closed over a plain value type (int, bool, a struct, ...) that never
        // needed ownership tracking in the first place -- there is no way to write
        // the qualifier conditionally in source. Requiring move(...)/borrow-source
        // ceremony for such a parameter would make it impossible to call a generic
        // method like "void Add(owned T value)" with a plain int argument. Treat
        // the qualifier as inert or such a closed T, exactly like `delete` on a
        // value-typed expression is already a safe no-op (see ProjectOopHirModel's
        // DeleteExpression handling) -- only a parameter whose underlying type is
        // actually ownable (a class/interface reference or native string) enforces
        // the transfer contract.
        const bool parameterOwnable = typeKeyIsOwnable(parameter.typeKey);
        // NSC-LIMIT-010: la regla vive ahora en classifyOwnershipArgument, arriba de
        // este fichero, y la comparte ProjectConstructionModel. Lo que sigue a este
        // bloque es el comentario historico de Stage 52 que explica POR QUE el orden
        // de las comprobaciones es el que es; se conserva porque esa razon no esta en
        // ningun otro sitio.
        if (!packElement && !callIsStringLiteralBorrow(candidate, parameterOrdinal, argument) &&
            !callIsFullExpressionBorrow(candidate, parameterOrdinal, argument) &&
            classifyOwnershipArgument(
                parameterOwnable, parameterOwnership, argument.ownership,
                argument.ownershipTransfer, argument.ownershipFreshOwner,
                argument.typeKey == "builtin:null") != OwnershipArgumentFit::Ok)
            candidateBinding.viable = false;
    }
    if (candidateBinding.viable && parameter.passKind == ParameterPassKind::Value &&
        memberModel_->typeIsMoveOnly(conversionTarget) && argument.addressable &&
        !argument.ownershipTransfer)
    {
        candidateBinding.viable = false;
        candidateBinding.invalidMoveOnlyArgumentOrdinal = argumentOrdinal;
    }
    candidateBinding.conversions.push_back(std::move(conversion));
}

void ProjectMethodBodyModel::finishCallCandidateEvaluation(
    std::vector<const ast::AstNode*>& constraintRejectedSyntaxes,
    MethodCallCandidate& candidateBinding,
    const ClassMemberSymbol& candidate,
    std::size_t candidateOrdinal,
    MethodCallBinding& call) const
{
    // NSC-LIMIT-007, tercera vuelta. LOS CONSTRAINTS SE CONSULTAN AQUI, y solo despues
    // de que la candidata sea viable por tipos y por propiedad.
    //
    // El orden importa: una candidata que no acepta estos argumentos no incumple nada
    // --nadie la va a llamar-- y acusarla llenaria la salida de errores sobre
    // sobrecargas que el programa descarta. Y a la inversa: si es viable, su `where` es
    // parte del contrato que la llamada tiene que cumplir, exactamente igual que los
    // tipos de sus parametros.
    //
    // El modelo de constraints guarda estas violaciones sin emitirlas precisamente
    // porque solo aqui se sabe cual de las candidatas es la que el programa usa.
    if (candidateBinding.viable && genericConstraintModel_ != nullptr &&
        !genericConstraintModel_->specializationSatisfiesConstraints(candidate.syntax))
    {
        candidateBinding.viable = false;
        constraintRejectedSyntaxes.push_back(candidate.syntax);
    }
    if (candidateBinding.viable)
        call.viableCandidateMemberOrdinals.push_back(candidateOrdinal);
}

bool ProjectMethodBodyModel::collectViableCallCandidates(
    std::vector<std::size_t>& viableEvaluationOrdinals,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> arguments,
    std::span<const ExpressionResult> resolvedArguments,
    bool coreStringForwarding,
    bool hasMatchingArity,
    std::span<const ast::AstNode* const> constraintRejectedSyntaxes,
    MethodCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!hasMatchingArity)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "no overload of '" + call.methodName + "' accepts " +
                std::to_string(arguments.size()) + " argument(s)");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return false;
    }

    viableEvaluationOrdinals.clear();
    viableEvaluationOrdinals.reserve(call.viableCandidateMemberOrdinals.size());
    for (std::size_t index = 0; index < call.evaluatedCandidates.size(); ++index)
    {
        if (call.evaluatedCandidates[index].viable)
        {
            viableEvaluationOrdinals.push_back(index);
        }
    }

    if (viableEvaluationOrdinals.empty())
    {
        if (reportTerminalCallCandidateDiagnostic(
                expression, callee, coreStringForwarding,
                constraintRejectedSyntaxes, call, diagnostics)) return false;
        const bool reportedOwnershipDiagnostic = reportCallOwnershipDiagnostic(
            expression, callee, resolvedArguments, coreStringForwarding, diagnostics);
        // C4 "language references": same precision rule as the other overload-failure path --
        // when exactly one overload has the right arity and the ONLY thing wrong is the missing
        // (or extra) `ref`/`out`/`in`, say THAT. The type mismatch the programmer would otherwise
        // be shown (`int` versus `int*`) is an artifact of how a by-reference parameter is
        // represented, not something they wrote.
        const bool reportedPassKindDiagnostic = !reportedOwnershipDiagnostic &&
            reportCallPassKindDiagnostic(expression, arguments, call, diagnostics);
        if (!reportedOwnershipDiagnostic && !reportedPassKindDiagnostic)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidConversion,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "no viable overload of '" + call.methodName + "' accepts the argument types");
        }
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return false;
    }

    return true;
}

bool ProjectMethodBodyModel::reportTerminalCallCandidateDiagnostic(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    bool coreStringForwarding,
    std::span<const ast::AstNode* const> constraintRejectedSyntaxes,
    MethodCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // Ninguna candidata sobrevivio. Si alguna cayo por su `where` --y no por tipos-- ese es
    // el error del programa, y se dice con el mensaje del constraint en vez de con un
    // «ninguna sobrecarga acepta estos argumentos» que no explica nada.
    if (!constraintRejectedSyntaxes.empty() && genericConstraintModel_ != nullptr)
    {
        for (const ast::AstNode* rejected : constraintRejectedSyntaxes)
            genericConstraintModel_->reportSpecializationConstraintViolations(
                rejected, expression->range(), diagnostics);
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return true;
    }
    for (const MethodCallCandidate& candidate : call.evaluatedCandidates)
    {
        if (candidate.invalidMoveOnlyArgumentOrdinal.has_value())
        {
            const std::size_t ordinal = *candidate.invalidMoveOnlyArgumentOrdinal;
            const ast::AstNode* argument = resolvedCallArgumentSyntax(
                expression, callee, ordinal, coreStringForwarding);
            diagnostics.report(
                diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                diagnostics::DiagnosticSeverity::Error,
                argument != nullptr ? argument->range() : expression->range(),
                "move-only argument cannot be copied into a by-value parameter; use move(argument)");
            call.resolution = MethodCallResolutionKind::NoViableCandidate;
            recordCallBinding(std::move(call));
            return true;
        }
        if (!candidate.invalidVariadicArgumentOrdinal.has_value()) continue;
        const std::size_t ordinal = *candidate.invalidVariadicArgumentOrdinal;
        const ast::AstNode* argument = resolvedCallArgumentSyntax(
            expression, callee, ordinal, coreStringForwarding);
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            argument != nullptr ? argument->range() : expression->range(),
            "C variadic arguments must be by-value ABI scalars, raw pointers, enums, "
            "or extern C delegates");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return true;
    }
    return false;
}

bool ProjectMethodBodyModel::reportCallOwnershipDiagnostic(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    bool coreStringForwarding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (memberModel_ == nullptr) return false;
    for (const std::size_t candidateOrdinal : callee.methodCandidates)
    {
        if (candidateOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
        if (candidate.kind != ClassMemberKind::Method ||
            candidate.parameters.size() < resolvedArguments.size() ||
            !std::ranges::all_of(
                candidate.parameters.begin() +
                    static_cast<std::ptrdiff_t>(resolvedArguments.size()),
                candidate.parameters.end(),
                [](const MethodParameterSymbol& parameter)
                {
                    return parameter.defaultValueSyntax != nullptr;
                })) continue;
        if (reportCandidateOwnershipDiagnostic(
                expression, callee, candidate, resolvedArguments,
                coreStringForwarding, diagnostics)) return true;
    }
    return false;
}

bool ProjectMethodBodyModel::reportCandidateOwnershipDiagnostic(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const ClassMemberSymbol& candidate,
    std::span<const ExpressionResult> resolvedArguments,
    bool coreStringForwarding,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < resolvedArguments.size(); ++argumentOrdinal)
    {
        const ProjectImplicitConversion physical = classifyImplicit(
            resolvedArguments[argumentOrdinal].typeKey,
            candidate.parameters[argumentOrdinal].typeKey,
            resolvedArguments[argumentOrdinal].integerConstant);
        if (!physical.exists()) continue;
        const ast::AstNode* argumentSyntax = resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding);
        if (reportArgumentOwnershipDiagnostic(
                expression, argumentSyntax, candidate.parameters[argumentOrdinal],
                resolvedArguments[argumentOrdinal], diagnostics)) return true;
    }
    return false;
}

bool ProjectMethodBodyModel::reportArgumentOwnershipDiagnostic(
    const ast::AstNode* expression,
    const ast::AstNode* argumentSyntax,
    const MethodParameterSymbol& parameter,
    const ExpressionResult& argument,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // Stage 52: mirror the same value-type bypass applied in the primary
    // candidate-matching loop above, so this diagnostic-only re-check doesn't
    // report a misleading ownership error for a generic parameter closed over
    // a plain value type.
    const bool diagnosticParameterOwnable = typeKeyIsOwnable(parameter.typeKey);
    // NSC-LIMIT-010: este re-chequeo, que existe solo para dar un mensaje bueno,
    // llevaba escrita SU PROPIA copia de la regla -- la tercera. Ahora pregunta al
    // mismo sitio que la viabilidad, asi que no puede acusar de un incumplimiento
    // que la resolucion no vio, ni callarse uno que si.
    if (parameter.ownership == sema::OwnershipKind::Owned &&
        classifyOwnershipArgument(
            diagnosticParameterOwnable, parameter.ownership, argument.ownership,
            argument.ownershipTransfer, argument.ownershipFreshOwner,
            argument.typeKey == "builtin:null") != OwnershipArgumentFit::Ok)
    {
        std::string message = "owned parameter '" + parameter.name +
            "' requires explicit ownership transfer";
        if (argumentSyntax != nullptr &&
            argumentSyntax->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            if (argument.ownership == sema::OwnershipKind::Borrowed)
                message = "cannot convert borrowed value '" + argumentSyntax->text() +
                    "' to owned parameter";
            else
                message = "cannot copy owned value '" + argumentSyntax->text() +
                    "' into owned parameter; use move(" + argumentSyntax->text() + ")";
        }
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            argumentSyntax != nullptr ? argumentSyntax->range() : expression->range(),
            std::move(message));
        return true;
    }
    if (parameter.ownership == sema::OwnershipKind::Borrowed &&
        diagnosticParameterOwnable && argument.typeKey != "string-literal" &&
        !argument.ownershipTransfer && argument.ownership == sema::OwnershipKind::None)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            argumentSyntax != nullptr ? argumentSyntax->range() : expression->range(),
            "borrowed parameter '" + parameter.name +
                "' requires a checked owner or borrowed source");
        return true;
    }
    if (argument.ownership == sema::OwnershipKind::Borrowed &&
        parameter.ownership == sema::OwnershipKind::None)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            argumentSyntax != nullptr ? argumentSyntax->range() : expression->range(),
            "borrowed argument requires a borrowed parameter; unqualified parameter '" +
                parameter.name + "' may retain the value");
        return true;
    }
    if (argument.ownershipTransfer && parameter.ownership != sema::OwnershipKind::Owned)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            argumentSyntax != nullptr ? argumentSyntax->range() : expression->range(),
            parameter.ownership == sema::OwnershipKind::Borrowed
                ? "cannot transfer ownership with move(...) into a borrowed parameter"
                : "ownership-transfer argument requires an owned parameter");
        return true;
    }
    return false;
}

bool ProjectMethodBodyModel::reportCallPassKindDiagnostic(
    const ast::AstNode* expression,
    std::span<const ExpressionResult> arguments,
    const MethodCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (memberModel_ == nullptr) return false;
    std::size_t arityMatches = 0;
    std::size_t soleCandidate = 0;
    for (const MethodCallCandidate& candidate : call.evaluatedCandidates)
    {
        if (candidate.conversions.size() != arguments.size()) continue;
        ++arityMatches;
        soleCandidate = candidate.memberOrdinal;
    }
    if (arityMatches != 1 || soleCandidate >= memberModel_->members().size()) return false;
    const ClassMemberSymbol& only = memberModel_->members()[soleCandidate];
    bool mismatch = false;
    for (std::size_t index = 0;
         index < arguments.size() && index < only.parameters.size(); ++index)
    {
        if (only.parameters[index].passKind != arguments[index].argumentPassKind)
        {
            mismatch = true;
            break;
        }
    }
    if (!mismatch) return false;
    const std::vector<ExpressionResult> argumentValues(arguments.begin(), arguments.end());
    checkArgumentPassKinds(
        expression, call.methodName, soleCandidate, argumentValues, diagnostics);
    return true;
}

bool ProjectMethodBodyModel::selectCallCandidate(
    std::size_t& selectedIndex,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> arguments,
    std::span<const ExpressionResult> resolvedArguments,
    bool coreStringForwarding,
    MethodCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics)
{
    bool hasMatchingArity = false;
    std::vector<const ast::AstNode*> constraintRejectedSyntaxes;
    evaluateCallCandidates(
        hasMatchingArity, constraintRejectedSyntaxes, expression, callee,
        resolvedArguments, coreStringForwarding, call);
    std::vector<std::size_t> viableEvaluationOrdinals;
    if (!collectViableCallCandidates(
            viableEvaluationOrdinals, expression, callee, arguments, resolvedArguments,
            coreStringForwarding, hasMatchingArity, constraintRejectedSyntaxes,
            call, diagnostics)) return false;
    return chooseNonDominatedCallCandidate(
        selectedIndex, expression, viableEvaluationOrdinals, call, diagnostics);
}

bool ProjectMethodBodyModel::chooseNonDominatedCallCandidate(
    std::size_t& selectedIndex,
    const ast::AstNode* expression,
    std::span<const std::size_t> viableEvaluationOrdinals,
    MethodCallBinding& call,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::vector<std::size_t> nonDominated;
    nonDominated.reserve(viableEvaluationOrdinals.size());
    for (const std::size_t candidateIndex : viableEvaluationOrdinals)
    {
        const bool dominated = std::ranges::any_of(
            nonDominated, [&](const std::size_t otherIndex)
        {
            return betterCandidate(
                call.evaluatedCandidates[otherIndex],
                call.evaluatedCandidates[candidateIndex]);
        });
        if (dominated) continue;

        std::erase_if(nonDominated, [&](const std::size_t otherIndex)
        {
            return betterCandidate(
                call.evaluatedCandidates[candidateIndex],
                call.evaluatedCandidates[otherIndex]);
        });
        nonDominated.push_back(candidateIndex);
    }

    if (nonDominated.size() != 1)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "ambiguous instance method call to '" + call.methodName + "'");
        call.resolution = MethodCallResolutionKind::Ambiguous;
        recordCallBinding(std::move(call));
        return false;
    }

    selectedIndex = nonDominated.front();
    return true;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::completeSelectedCall(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> arguments,
    std::vector<ExpressionResult>& resolvedArguments,
    bool coreStringForwarding,
    bool nullConditionalCall,
    std::size_t selectedIndex,
    MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    std::optional<std::size_t> provenVariantCase;
    const MethodCallCandidate& selected = call.evaluatedCandidates[selectedIndex];
    if (!validateSelectedCallReceiver(
            provenVariantCase, expression, callee, selected,
            call, scopeOrdinal, diagnostics)) return result;
    const std::vector<ExpressionResult> argumentValues(arguments.begin(), arguments.end());
    checkArgumentPassKinds(
        expression, call.methodName, selected.memberOrdinal, argumentValues, diagnostics,
        std::span<const std::size_t>(selected.argumentParameterOrdinals).subspan(
            coreStringForwarding ? 1U : 0U));
    invalidateCallArgumentFlow(
        expression, callee, resolvedArguments, selected, coreStringForwarding, scopeOrdinal);
    call.resolution = MethodCallResolutionKind::Resolved;
    call.selectedMemberOrdinal = selected.memberOrdinal;
    updateScopedChildBorrowsForCall(
        expression, selected, call, methodMemberOrdinal, scopeOrdinal, diagnostics);
    configureSelectedCallBinding(
        result, expression, callee, selected, coreStringForwarding, call);
    if (memberModel_ != nullptr && selected.memberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& selectedMember = memberModel_->members()[selected.memberOrdinal];
        recordSelectedOutArguments(
            expression, callee, resolvedArguments, selected, selectedMember,
            coreStringForwarding, methodMemberOrdinal, scopeOrdinal);
        recordSelectedOwnershipTransfers(
            expression, callee, resolvedArguments, selected, selectedMember,
            coreStringForwarding, methodMemberOrdinal, scopeOrdinal);
        recordSelectedBorrows(
            expression, callee, resolvedArguments, selected, selectedMember,
            coreStringForwarding, methodMemberOrdinal, scopeOrdinal);
        applySelectedCoreOperations(
            expression, selectedMember, methodMemberOrdinal, scopeOrdinal, call, result, diagnostics);
        // EN: Freeze 3.0 C-13: an async callee reports its error at the await, not at the call.
        // ES: Congelamiento 3.0 C-13: un callee async reporta su error en el await, no en la llamada.
        if (!selectedMember.effects.contains(sema::Effect::Async))
            requireDeclaredErrorHandled(
                expression, selectedMember.declaredErrorTypeKey, selectedMember.name,
                methodMemberOrdinal, diagnostics);
    }
    finalizeSelectedCallResult(
        result, expression, call, nullConditionalCall, provenVariantCase, diagnostics);
    recordCallBinding(std::move(call));
    return result;
}

bool ProjectMethodBodyModel::validateSelectedCallReceiver(
    std::optional<std::size_t>& provenVariantCase,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const MethodCallCandidate& selected,
    MethodCallBinding& call,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (memberModel_ == nullptr || selected.memberOrdinal >= memberModel_->members().size())
        return true;
    const ClassMemberSymbol& selectedMember = memberModel_->members()[selected.memberOrdinal];
    if (!validateActiveIterationCallReceiver(
            expression, callee, selectedMember, call, scopeOrdinal, diagnostics)) return false;
    updateSelectedVariantFlow(
        provenVariantCase, expression, callee, selectedMember, scopeOrdinal);
    // NativeSharp 3.0 T03. Variant payload extractors have to be public so a match in a
    // LibraryReference consumer can call code transported through NSMETA, but they are not a
    // user-callable API: reading an inactive union alternative would be undefined. The parser
    // marks only the extractor call it generates after the corresponding IsCase branch.
    if (selectedMember.name.starts_with("__nsc_variant_extract_") &&
        expression->auxiliaryText() != "variant-pattern-extract")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "variant payload extraction is available only inside a matching case arm");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return false;
    }
    if (callee.readonlyReceiver && selectedMember.declaringTypeIsStruct &&
        !selectedMember.isStatic && !selectedMember.isReadonly &&
        !(selectedMember.name.starts_with("__nsc_variant_extract_") &&
          expression->auxiliaryText() == "variant-pattern-extract"))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "readonly receiver cannot invoke mutating struct member '" +
                selectedMember.name + "'");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return false;
    }
    return true;
}

bool ProjectMethodBodyModel::validateActiveIterationCallReceiver(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const ClassMemberSymbol& selectedMember,
    MethodCallBinding& call,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (selectedMember.isStatic || selectedMember.isReadonly ||
        callee.receiverSyntax == nullptr || activeIterationBorrows_.empty()) return true;
    const std::optional<std::string> receiverPlace =
        storagePlaceKey(callee.receiverSyntax, scopeOrdinal);
    if (!receiverPlace.has_value()) return true;
    for (auto iterator = activeIterationBorrows_.rbegin();
         iterator != activeIterationBorrows_.rend(); ++iterator)
    {
        if (!placesOverlap(iterator->sourcePlace, *receiverPlace)) continue;
        bool compatibleElementCall = false;
        if (iterator->mutableElements &&
            callee.receiverSyntax->kind() == ast::AstNodeKind::IndexExpression &&
            callee.receiverSyntax->first() != nullptr)
        {
            const std::optional<std::string> indexedSource =
                storagePlaceKey(callee.receiverSyntax->first(), scopeOrdinal);
            compatibleElementCall = indexedSource.has_value() &&
                *indexedSource == iterator->sourcePlace;
        }
        if (compatibleElementCall) break;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            expression->range(),
            "cannot invoke mutating member '" + selectedMember.name +
                "' on a foreach source while a lending iteration is active; "
                "the operation could invalidate yielded references");
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return false;
    }
    return true;
}

void ProjectMethodBodyModel::updateSelectedVariantFlow(
    std::optional<std::size_t>& provenVariantCase,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const ClassMemberSymbol& selectedMember,
    std::size_t scopeOrdinal)
{
    const std::string_view variantMarker = selectedMember.syntax != nullptr
        ? std::string_view(selectedMember.syntax->auxiliaryText()) : std::string_view{};
    if (variantMarker.starts_with("variant-case-test:"))
    {
        const auto storage = callNamedStorageKey(callee.receiverSyntax, scopeOrdinal);
        const auto activeCase = callMarkerCase(variantMarker);
        if (storage.has_value() && activeCase.has_value())
        {
            ConditionFlowStates flow{captureOwnershipFlow(), captureOwnershipFlow()};
            if (OwnershipFlowStorageFacts* facts =
                    mutableSnapshotStorageFacts(flow.whenTrue, *storage);
                facts != nullptr)
                facts->activeVariantCase = *activeCase;
            if (OwnershipFlowStorageFacts* facts =
                    mutableSnapshotStorageFacts(flow.whenFalse, *storage);
                facts != nullptr)
                facts->activeVariantCase.reset();
            conditionFlowBySyntax_.insert_or_assign(expression, std::move(flow));
        }
    }
    else if (variantMarker.starts_with("variant-case-extract:") &&
             expression->auxiliaryText() == "variant-pattern-extract")
    {
        const auto storage = callNamedStorageKey(callee.receiverSyntax, scopeOrdinal);
        const auto extractedCase = callMarkerCase(variantMarker);
        if (storage.has_value() && extractedCase.has_value())
        {
            const auto active = activeVariantCases_.find(*storage);
            if (active != activeVariantCases_.end() && active->second == *extractedCase)
                provenVariantCase = *extractedCase;
        }
    }
}

void ProjectMethodBodyModel::invalidateCallArgumentFlow(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    const MethodCallCandidate& selected,
    bool coreStringForwarding,
    std::size_t scopeOrdinal)
{
    // A ref/out call is a write through the argument. Any narrowing fact for that named
    // storage becomes stale at the call boundary; by-value calls cannot reassign it.
    if (memberModel_ == nullptr || selected.memberOrdinal >= memberModel_->members().size()) return;
    const ClassMemberSymbol& selectedMemberForFlow =
        memberModel_->members()[selected.memberOrdinal];
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < resolvedArguments.size() &&
         argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
    {
        const std::size_t parameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
        if (parameterOrdinal >= selectedMemberForFlow.parameters.size() ||
            (selectedMemberForFlow.parameters[parameterOrdinal].passKind != ParameterPassKind::Ref &&
             selectedMemberForFlow.parameters[parameterOrdinal].passKind != ParameterPassKind::Out))
            continue;
        const ast::AstNode* argumentSyntax = resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding);
        const ast::AstNode* targetSyntax =
            argumentSyntax != nullptr && argumentSyntax->first() != nullptr
                ? definiteAssignmentTargetRoot(argumentSyntax->first()) : nullptr;
        const auto storage = callNamedStorageKey(targetSyntax, scopeOrdinal);
        if (!storage.has_value()) continue;
        nonNullStorages_.erase(*storage);
        integerRanges_.erase(*storage);
        activeVariantCases_.erase(*storage);
    }
}

void ProjectMethodBodyModel::updateScopedChildBorrowsForCall(
    const ast::AstNode* expression,
    const MethodCallCandidate& selected,
    const MethodCallBinding& call,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NS304-T07-F06: TaskScope.Join discharges the scoped child borrows; TaskScope.Spawn of an
    // async child that captures a `ref value` registers the borrow of that source local. Block exit
    // while a borrow is still registered rejects the dangling Future.
    if (call.methodName == "Join")
    {
        scopedChildBorrows_.clear();
    }
    else if (call.methodName == "Spawn")
    {
        registerScopedChildBorrows(expression, methodMemberOrdinal, scopeOrdinal);
        // NS304-T07-F07: a thread executor may move the Future to another thread, so every `ref`
        // capture must be Send (a raw pointer, or a type containing one, does not derive Send).
        if (selectedCallUsesThreadExecutor(selected))
            validateThreadExecutorCaptures(
                expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
    }
}

void ProjectMethodBodyModel::registerScopedChildBorrows(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    for (const ast::AstNode* argument : expression->children())
    {
        if (argument == nullptr || argument->kind() != ast::AstNodeKind::CallExpression)
            continue;
        const MethodCallBinding* childCall = callBinding(argument, methodMemberOrdinal);
        if (childCall == nullptr || !childCall->selectedMemberOrdinal.has_value()) continue;
        if (*childCall->selectedMemberOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& child =
            memberModel_->members()[*childCall->selectedMemberOrdinal];
        for (std::size_t childArgOrdinal = 0;
             childArgOrdinal < argument->children().size(); ++childArgOrdinal)
        {
            if (childArgOrdinal >= child.parameters.size() ||
                child.parameters[childArgOrdinal].passKind != ParameterPassKind::Ref)
                continue;
            const MethodEscapeOrigin origin = referenceEscapeOriginOfExpression(
                argument->children()[childArgOrdinal], scopeOrdinal);
            if (origin.kind == MethodEscapeOriginKind::CurrentFrameLocal &&
                origin.ordinal.has_value())
                scopedChildBorrows_.insert(*origin.ordinal);
        }
    }
}

bool ProjectMethodBodyModel::selectedCallUsesThreadExecutor(
    const MethodCallCandidate& selected) const
{
    if (genericModel_ == nullptr || memberModel_ == nullptr || nameLookup_ == nullptr ||
        inheritanceModel_ == nullptr || interfaceModel_ == nullptr ||
        selected.memberOrdinal >= memberModel_->members().size()) return false;
    const ClassMemberSymbol& spawnMember = memberModel_->members()[selected.memberOrdinal];
    const std::size_t declaringOrdinal = spawnMember.declaringTypeOrdinal;
    if (declaringOrdinal >= nameLookup_->namedTypes().size()) return false;
    const std::string scopeKey = nameLookup_->namedTypes()[declaringOrdinal].identity.key();
    const GenericSpecializationRecord* spec =
        genericModel_->specializationForTypeKey(scopeKey);
    if (spec == nullptr || spec->typeArguments.size() != 1U) return false;
    const std::string executorKey = ProjectMemberModel::canonicalGenericArgumentKey(
        spec->typeArguments.front(), *nameLookup_);
    const std::optional<std::size_t> executorOrdinal = namedTypeOrdinalForTypeKey(executorKey);
    const std::optional<std::size_t> threadOrdinal = namedTypeOrdinalForTypeKey(
        "type:NativeSharpCore::NativeSharp.Core.IThreadExecutor");
    return executorOrdinal.has_value() && threadOrdinal.has_value() &&
        interfaceModel_->classImplementsInterface(*executorOrdinal, *threadOrdinal);
}

void ProjectMethodBodyModel::validateThreadExecutorCaptures(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (const ast::AstNode* argument : expression->children())
    {
        if (argument == nullptr || argument->kind() != ast::AstNodeKind::CallExpression)
            continue;
        const MethodCallBinding* childCall = callBinding(argument, methodMemberOrdinal);
        if (childCall == nullptr || !childCall->selectedMemberOrdinal.has_value()) continue;
        if (*childCall->selectedMemberOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& child =
            memberModel_->members()[*childCall->selectedMemberOrdinal];
        for (std::size_t childArgOrdinal = 0;
             childArgOrdinal < argument->children().size(); ++childArgOrdinal)
        {
            if (childArgOrdinal >= child.parameters.size() ||
                child.parameters[childArgOrdinal].passKind != ParameterPassKind::Ref)
                continue;
            const MethodEscapeOrigin origin = referenceEscapeOriginOfExpression(
                argument->children()[childArgOrdinal], scopeOrdinal);
            if (origin.kind != MethodEscapeOriginKind::CurrentFrameLocal ||
                !origin.ordinal.has_value() || *origin.ordinal >= locals_.size())
                continue;
            const MethodLocalSymbol& source = locals_[*origin.ordinal];
            if (!callTypeIsSend(source.typeKey))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidCall,
                    diagnostics::DiagnosticSeverity::Error,
                    argument->range(),
                    "future submitted to a thread executor is not Send: capture '" +
                        source.name + "' has type '" + source.typeKey + "'");
            }
            else if (!callTypeIsSync(source.typeKey))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidCall,
                    diagnostics::DiagnosticSeverity::Error,
                    argument->range(),
                    "shared capture '" + source.name +
                        "' requires its referred type '" + source.typeKey + "' to be Sync");
            }
        }
    }
}

bool ProjectMethodBodyModel::callTypeIsSend(const std::string& typeKey) const
{
    // Depth-first walk over the field types with an explicit stack instead of recursion. The
    // answer is the conjunction of the leaf tests the recursion made; a named type already
    // walked in this query adds no new field types, so it is not walked again (the recursion
    // had no such guard, so a cycle of field types never terminated).
    std::vector<std::string> pending{typeKey};
    std::unordered_set<std::size_t> walkedTypes;
    std::vector<std::size_t> fallbackMemberOrdinals;
    while (!pending.empty())
    {
        std::string current = std::move(pending.back());
        pending.pop_back();
        // ref:/refmut: are stripped one at a time, each time after the pointer test, as the
        // recursion did.
        while (true)
        {
            if (current.ends_with("*")) return false;
            if (!current.starts_with("ref:") && !current.starts_with("refmut:")) break;
            current = current.substr(current.find(':') + 1);
        }
        const std::optional<std::size_t> typeOrdinal = namedTypeOrdinalForTypeKey(current);
        if (!typeOrdinal.has_value() || !walkedTypes.insert(*typeOrdinal).second) continue;
        std::span<const std::size_t> memberOrdinals =
            memberModel_->membersDeclaredByType(*typeOrdinal);
        if (!memberModel_->hasMembersDeclaredByTypeIndex())
        {
            fallbackMemberOrdinals.clear();
            for (std::size_t ordinal = 0; ordinal < memberModel_->members().size(); ++ordinal)
            {
                if (memberModel_->members()[ordinal].declaringTypeOrdinal == *typeOrdinal)
                    fallbackMemberOrdinals.push_back(ordinal);
            }
            memberOrdinals = fallbackMemberOrdinals;
        }
        // Pushed in reverse so the fields are taken in declaration order, as the recursion did.
        for (auto memberOrdinal = memberOrdinals.rbegin(); memberOrdinal != memberOrdinals.rend();
             ++memberOrdinal)
        {
            if (*memberOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& member = memberModel_->members()[*memberOrdinal];
            if (member.kind != ClassMemberKind::Field) continue;
            pending.push_back(member.declaredTypeKey);
        }
    }
    return true;
}

bool ProjectMethodBodyModel::callTypeIsSync(const std::string& typeKey) const
{
    // Depth-first walk over the field types with an explicit stack instead of recursion. The
    // answer is the conjunction of the leaf tests the recursion made; a named type already
    // walked in this query adds no new field types, so it is not walked again (the recursion
    // had no such guard, so a cycle of field types never terminated).
    std::vector<std::string> pending{typeKey};
    std::unordered_set<std::size_t> walkedTypes;
    std::vector<std::size_t> fallbackMemberOrdinals;
    while (!pending.empty())
    {
        std::string current = std::move(pending.back());
        pending.pop_back();
        // ref: is stripped one at a time, each time after the pointer and refmut: tests, as the
        // recursion did.
        while (true)
        {
            if (current.ends_with("*")) return false;
            if (current.starts_with("refmut:")) return false;
            if (!current.starts_with("ref:")) break;
            current = current.substr(current.find(':') + 1);
        }
        const std::optional<std::size_t> typeOrdinal = namedTypeOrdinalForTypeKey(current);
        if (!typeOrdinal.has_value() || !walkedTypes.insert(*typeOrdinal).second) continue;
        std::span<const std::size_t> memberOrdinals =
            memberModel_->membersDeclaredByType(*typeOrdinal);
        if (!memberModel_->hasMembersDeclaredByTypeIndex())
        {
            fallbackMemberOrdinals.clear();
            for (std::size_t ordinal = 0; ordinal < memberModel_->members().size(); ++ordinal)
            {
                if (memberModel_->members()[ordinal].declaringTypeOrdinal == *typeOrdinal)
                    fallbackMemberOrdinals.push_back(ordinal);
            }
            memberOrdinals = fallbackMemberOrdinals;
        }
        // Pushed in reverse so the fields are taken in declaration order, as the recursion did.
        for (auto memberOrdinal = memberOrdinals.rbegin(); memberOrdinal != memberOrdinals.rend();
             ++memberOrdinal)
        {
            if (*memberOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& member = memberModel_->members()[*memberOrdinal];
            if (member.kind != ClassMemberKind::Field) continue;
            pending.push_back(member.declaredTypeKey);
        }
    }
    return true;
}

void ProjectMethodBodyModel::configureSelectedCallBinding(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    const MethodCallCandidate& selected,
    bool coreStringForwarding,
    MethodCallBinding& call) const
{
    call.selectedConversions = selected.conversions;
    call.selectedArgumentParameterOrdinals = selected.argumentParameterOrdinals;
    call.selectedArgumentSyntaxes.reserve(selected.argumentParameterOrdinals.size());
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
        call.selectedArgumentSyntaxes.push_back(resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding));
    call.selectedParameterPackElements = selected.parameterPackElements;
    call.selectedParameterPackOrdinal = selected.parameterPackOrdinal;
    call.selectedUsesExpandedParameterPack = selected.usesExpandedParameterPack;
    call.selectedSynthesizesEmptyParameterPack = selected.synthesizesEmptyParameterPack;
    if (memberModel_ == nullptr || selected.memberOrdinal >= memberModel_->members().size()) return;
    const ClassMemberSymbol& selectedMember = memberModel_->members()[selected.memberOrdinal];
    for (const std::size_t parameterOrdinal : selected.defaultedParameterOrdinals)
    {
        call.selectedDefaultArguments.push_back(
            selectedMember.parameters[parameterOrdinal].defaultValueSyntax);
        call.selectedDefaultArgumentParameterOrdinals.push_back(parameterOrdinal);
    }
    call.resultTypeKey = selectedMember.declaredTypeKey;
    result.pointerPointeeReadonly = selectedMember.declaredPointerPointeeReadonly;
    result.pointerNullable = selectedMember.declaredPointerNullable;
    result.pointerHasProvenance = selectedMember.declaredPointerHasProvenance;
    result.pointerKnownAlignment = selectedMember.declaredPointerKnownAlignment;
    result.pointerAddressSpace = selectedMember.declaredPointerAddressSpace;
    result.ownership = selectedMember.declaredOwnership;
    if (selectedMember.declaredOwnership != sema::OwnershipKind::None)
    {
        result.objectLifetimeOwnership = selectedMember.declaredOwnership;
        result.storageOwnership = selectedMember.declaredOwnership;
    }
    // EN: Freeze 3.0 P3: a `ref readonly` result is an addressable place that cannot be written;
    //     like an `in` parameter it admits reads and readonly members only.
    // ES: Congelamiento 3.0 P3: un resultado `ref readonly` es un lugar direccionable que no se
    //     puede escribir; como un parametro `in` solo admite lecturas y miembros readonly.
    const bool readonlyReference = isReadonlyReferenceReturn(selectedMember);
    result.assignable = selectedMember.isReferenceReturn && !readonlyReference;
    result.addressable = selectedMember.isReferenceReturn;
    result.readonlyReceiver = result.readonlyReceiver || readonlyReference;
    result.ownershipTransfer = selectedMember.declaredOwnership == sema::OwnershipKind::Owned;
    // An owned return crosses the call boundary without a named owner. It is therefore a
    // fresh owner in the caller, just like `new T(...)`, and may participate in the
    // full-expression borrowing rule below.
    result.ownershipFreshOwner = result.ownershipTransfer;
    call.staticCall = selectedMember.isStatic;
    if (call.staticCall)
    {
        call.implicitThis = false;
        call.explicitThis = false;
        call.baseReceiver = false;
        call.interfaceCall = false;
        if (!call.forwardedReceiver)
        {
            call.receiverSyntax = nullptr;
            call.receiverTypeKey.clear();
            call.receiverTypeOrdinal.reset();
        }
    }
}

void ProjectMethodBodyModel::recordSelectedOutArguments(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    const MethodCallCandidate& selected,
    const ClassMemberSymbol& selectedMember,
    bool coreStringForwarding,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    // NSC-LIMIT-006. Forwarding the current method's `out` parameter to another real `out`
    // parameter is a write just as surely as `value = ...`. Do this only after overload
    // resolution: syntax alone is insufficient because `callee(out value)` must not satisfy
    // the obligation when the selected parameter is actually `ref` (that mismatch is still
    // diagnosed by checkArgumentPassKinds above). Following the target root also covers the
    // member/index forms already accepted for explicit assignments.
    const ClassMemberSymbol* callerMember = methodSymbol(methodMemberOrdinal);
    for (std::size_t argumentOrdinal = 0; callerMember != nullptr &&
         argumentOrdinal < resolvedArguments.size() &&
         argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
    {
        const std::size_t parameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
        if (parameterOrdinal >= selectedMember.parameters.size() ||
            selectedMember.parameters[parameterOrdinal].passKind != ParameterPassKind::Out ||
            resolvedArguments[argumentOrdinal].argumentPassKind != ParameterPassKind::Out)
            continue;
        const ast::AstNode* argumentSyntax = resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding);
        if (argumentSyntax == nullptr || argumentSyntax->first() == nullptr) continue;
        const ast::AstNode* root = definiteAssignmentTargetRoot(argumentSyntax->first());
        if (root == nullptr) continue;
        const std::optional<ScopeName> target = lookupScopeName(scopeOrdinal, root->text());
        if (!target.has_value() || target->kind != ScopeNameKind::Parameter ||
            target->semanticOrdinal >= callerMember->parameters.size() ||
            callerMember->parameters[target->semanticOrdinal].passKind != ParameterPassKind::Out)
            continue;
        assignedOutParameters_.insert({methodMemberOrdinal, target->semanticOrdinal});
    }
}

void ProjectMethodBodyModel::recordSelectedOwnershipTransfers(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    const MethodCallCandidate& selected,
    const ClassMemberSymbol& selectedMember,
    bool coreStringForwarding,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < resolvedArguments.size() &&
         argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
    {
        const std::size_t parameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
        if (parameterOrdinal >= selectedMember.parameters.size() ||
            selectedMember.parameters[parameterOrdinal].ownership != sema::OwnershipKind::Owned ||
            resolvedArguments[argumentOrdinal].typeKey == "builtin:null")
            continue;
        MethodOwnershipTransferBinding transfer;
        transfer.syntax = resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding);
        transfer.methodMemberOrdinal = methodMemberOrdinal;
        transfer.scopeOrdinal = scopeOrdinal;
        transfer.sourceLocalOrdinal =
            resolvedArguments[argumentOrdinal].ownershipSourceLocalOrdinal;
        transfer.sourceParameterOrdinal =
            resolvedArguments[argumentOrdinal].ownershipSourceParameterOrdinal;
        transfer.targetMemberOrdinal = selected.memberOrdinal;
        transfer.targetParameterOrdinal = parameterOrdinal;
        transfer.typeKey = selectedMember.parameters[parameterOrdinal].typeKey;
        if (transfer.sourceLocalOrdinal.has_value())
            transfer.kind = MethodOwnershipTransferKind::MoveLocalToOwnedParameter;
        else if (transfer.sourceParameterOrdinal.has_value())
            transfer.kind = MethodOwnershipTransferKind::MoveParameterToOwnedParameter;
        else
            transfer.kind = MethodOwnershipTransferKind::OwnedReturnToOwnedParameter;
        ownershipTransferBindings_.push_back(std::move(transfer));
    }
}

void ProjectMethodBodyModel::recordSelectedBorrows(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    std::span<const ExpressionResult> resolvedArguments,
    const MethodCallCandidate& selected,
    const ClassMemberSymbol& selectedMember,
    bool coreStringForwarding,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal)
{
    for (std::size_t argumentOrdinal = 0;
         argumentOrdinal < resolvedArguments.size() &&
         argumentOrdinal < selected.argumentParameterOrdinals.size(); ++argumentOrdinal)
    {
        const std::size_t parameterOrdinal = selected.argumentParameterOrdinals[argumentOrdinal];
        if (parameterOrdinal >= selectedMember.parameters.size()) continue;
        const bool freshTemporaryBorrow = callIsFullExpressionBorrow(
            selectedMember, parameterOrdinal, resolvedArguments[argumentOrdinal]);
        if (selectedMember.parameters[parameterOrdinal].ownership !=
                sema::OwnershipKind::Borrowed &&
            !freshTemporaryBorrow)
            continue;
        MethodBorrowBinding borrow;
        borrow.syntax = resolvedCallArgumentSyntax(
            expression, callee, argumentOrdinal, coreStringForwarding);
        borrow.methodMemberOrdinal = methodMemberOrdinal;
        borrow.scopeOrdinal = scopeOrdinal;
        borrow.sourceLocalOrdinal = resolvedArguments[argumentOrdinal].borrowedLocalOrdinal.has_value()
            ? resolvedArguments[argumentOrdinal].borrowedLocalOrdinal
            : resolvedArguments[argumentOrdinal].ownershipSourceLocalOrdinal;
        borrow.sourceParameterOrdinal =
            resolvedArguments[argumentOrdinal].ownershipSourceParameterOrdinal;
        borrow.targetMemberOrdinal = selected.memberOrdinal;
        borrow.targetParameterOrdinal = parameterOrdinal;
        borrow.typeKey = selectedMember.parameters[parameterOrdinal].typeKey;
        if (freshTemporaryBorrow)
            borrow.kind = MethodBorrowKind::FreshOwnedTemporaryToBorrowedParameter;
        else if (resolvedArguments[argumentOrdinal].borrowedLocalOrdinal.has_value())
            borrow.kind = MethodBorrowKind::BorrowedLocalToBorrowedParameter;
        else if (borrow.sourceLocalOrdinal.has_value())
            borrow.kind = MethodBorrowKind::OwnerLocalToBorrowedParameter;
        else if (borrow.sourceParameterOrdinal.has_value())
        {
            const ClassMemberSymbol* callerMember = methodSymbol(methodMemberOrdinal);
            if (callerMember != nullptr &&
                *borrow.sourceParameterOrdinal < callerMember->parameters.size() &&
                callerMember->parameters[*borrow.sourceParameterOrdinal].ownership ==
                    sema::OwnershipKind::Owned)
                borrow.kind = MethodBorrowKind::OwnedParameterToBorrowedParameter;
            else
                borrow.kind = MethodBorrowKind::BorrowedParameterToBorrowedParameter;
        }
        else
            borrow.kind = MethodBorrowKind::BorrowedReturnToBorrowedParameter;
        borrowBindings_.push_back(std::move(borrow));
    }
}

void ProjectMethodBodyModel::finalizeSelectedCallResult(
    ExpressionResult& result,
    const ast::AstNode* expression,
    const MethodCallBinding& call,
    bool nullConditionalCall,
    std::optional<std::size_t> provenVariantCase,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    result.typeKey = call.resultTypeKey;
    result.valid = !isErrorType(result.typeKey);
    result.flowActiveVariantCase = provenVariantCase;
    if (!result.valid || !nullConditionalCall) return;
    if (callResultIsNullable(result.typeKey)) return;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        expression->range(),
        "a null-conditional method must return a nullable pointer/reference carrier; "
        "value and void results require Optional<T>");
    result.valid = false;
}

bool ProjectMethodBodyModel::callResultIsNullable(std::string_view typeKey) const
{
    if (ProjectTypeConversion::isPointer(typeKey) ||
        ProjectTypeConversion::isNativeArray(typeKey) ||
        typeKey == "builtin:string" || isAnyDelegateTypeKey(typeKey))
        return true;
    const std::optional<std::size_t> ordinal = referenceTypeOrdinalForTypeKey(typeKey);
    return ordinal.has_value() && inheritanceModel_ != nullptr &&
        (inheritanceModel_->isClass(*ordinal) ||
         (interfaceModel_ != nullptr && interfaceModel_->isInterface(*ordinal)));
}

std::optional<ProjectMethodBodyModel::ExpressionResult> ProjectMethodBodyModel::prepareCallCallee(
    const ast::AstNode* expression,
    MethodCallBinding& call,
    ExpressionResult& callee,
    bool& nullConditionalCall,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    call.syntax = expression;
    call.callerMethodMemberOrdinal = methodMemberOrdinal;
    call.scopeOrdinal = scopeOrdinal;
    nullConditionalCall = expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression;
    call.explicitThis = expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::MemberExpression &&
        expression->first()->first() != nullptr &&
        expression->first()->first()->kind() == ast::AstNodeKind::ThisExpression;

    callee = analyzeExpression(
        expression->first(), methodMemberOrdinal, scopeOrdinal, diagnostics, true, true);
    call.implicitThis = callee.methodGroup && callee.receiverSyntax == nullptr;
    call.baseReceiver = callee.baseReceiver;
    call.receiverSyntax = callee.receiverSyntax;
    call.receiverTypeKey = callee.receiverTypeKey;
    call.receiverTypeOrdinal = callee.receiverTypeOrdinal;
    call.interfaceCall = call.receiverTypeOrdinal.has_value() && interfaceModel_ != nullptr &&
        interfaceModel_->isInterface(*call.receiverTypeOrdinal);
    if (expression->first() != nullptr)
    {
        call.methodName = expression->first()->kind() == ast::AstNodeKind::IdentifierExpression ||
                expression->first()->kind() == ast::AstNodeKind::MemberExpression ||
                expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression
            ? expression->first()->text()
            : "<expression>";
    }

    if (callee.extensionMethodGroup && namespaceFunctionModel_ != nullptr)
    {
        if (const NamespaceFunctionReferenceBinding* extension =
                namespaceFunctionModel_->extensionBinding(expression->first());
            extension != nullptr)
        {
            return analyzeNamespaceFunctionCall(
                expression, *extension, methodMemberOrdinal, scopeOrdinal, diagnostics, &callee);
        }
    }
    // NS304-T08-F03: a static extension member (StaticMethodGroup with no receiver) resolves as a
    // direct namespace call, not the instance method resolution.
    if (!callee.extensionMethodGroup && callee.methodGroup && callee.receiverSyntax == nullptr &&
        namespaceFunctionModel_ != nullptr)
    {
        if (const NamespaceFunctionReferenceBinding* extension =
                namespaceFunctionModel_->extensionBinding(expression->first());
            extension != nullptr)
        {
            return analyzeNamespaceFunctionCall(
                expression, *extension, methodMemberOrdinal, scopeOrdinal, diagnostics, nullptr);
        }
    }

    if (const auto handled = analyzeEnumRootCall(
            expression, callee, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return *handled;
    if (const auto handled = analyzeIteratorFacadeCall(
            expression, callee, call, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return *handled;
    if (const auto handled = analyzeStructRootCall(
            expression, callee, call, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return *handled;
    return std::nullopt;
}

void ProjectMethodBodyModel::configureCallLambdaTargets(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    MethodCallBinding& call,
    bool& coreStringForwarding,
    std::vector<std::string>& lambdaTargetTypes)
{
    (void)call;
    // Native string instance syntax forwards to a static Core.String method whose first
    // parameter is the receiver.  Named source arguments must never be allowed to bind that
    // compiler-supplied parameter.
    coreStringForwarding = false;
    if (callee.receiverSyntax != nullptr && !callee.receiverTypeKey.empty() &&
        memberModel_ != nullptr && nameLookup_ != nullptr && !callee.methodCandidates.empty())
    {
        coreStringForwarding = std::ranges::all_of(callee.methodCandidates, [&](std::size_t ordinal)
        {
            if (ordinal >= memberModel_->members().size()) return false;
            const ClassMemberSymbol& member = memberModel_->members()[ordinal];
            return member.kind == ClassMemberKind::Method && member.isStatic &&
                coreStringTypeOrdinal_.has_value() &&
                member.declaringTypeOrdinal == *coreStringTypeOrdinal_;
        });
    }

    const bool hasLambdaArguments = std::ranges::any_of(
        expression->children(), [](const ast::AstNode* argument)
        {
            return argument != nullptr && argument->kind() == ast::AstNodeKind::LambdaExpression;
        });
    if (hasLambdaArguments) lambdaTargetTypes.resize(expression->children().size());

    if (hasLambdaArguments && callee.eventMemberOrdinal.has_value() && memberModel_ != nullptr &&
        *callee.eventMemberOrdinal < memberModel_->members().size())
    {
        const ClassMemberSymbol& event = memberModel_->members()[*callee.eventMemberOrdinal];
        std::string ignoredReturnType;
        std::vector<std::string> parameterTypes;
        std::size_t ignoredDelegateOrdinal = 0;
        if (delegateSignature(event.declaredTypeKey, ignoredReturnType, parameterTypes, ignoredDelegateOrdinal))
        seedCallLambdaTargets(lambdaTargetTypes, expression, parameterTypes);
    }
    else if (hasLambdaArguments && !callee.methodGroup && isDelegateTypeKey(callee.typeKey))
    {
        std::string ignoredReturnType;
        std::vector<std::string> parameterTypes;
        std::size_t ignoredDelegateOrdinal = 0;
        if (delegateSignature(callee.typeKey, ignoredReturnType, parameterTypes, ignoredDelegateOrdinal))
        seedCallLambdaTargets(lambdaTargetTypes, expression, parameterTypes);
    }
    else if (hasLambdaArguments && callee.methodGroup && memberModel_ != nullptr)
    {
        configureMethodGroupLambdaTargets(
            expression, callee, coreStringForwarding, lambdaTargetTypes);
    }

}

void ProjectMethodBodyModel::configureMethodGroupLambdaTargets(
    const ast::AstNode* expression,
    const ExpressionResult& callee,
    bool coreStringForwarding,
    std::vector<std::string>& lambdaTargetTypes)
{
    std::vector<std::pair<const ClassMemberSymbol*, NamedArgumentMap>> arityMatches;
    for (const std::size_t candidateOrdinal : callee.methodCandidates)
    {
        if (candidateOrdinal >= memberModel_->members().size()) continue;
        const ClassMemberSymbol& candidate = memberModel_->members()[candidateOrdinal];
        const NamedArgumentMap mapping = mapArgumentsToParameters(
            expression->children(), candidate, coreStringForwarding ? 1U : 0U);
        if (candidate.kind == ClassMemberKind::Method && mapping.valid)
            arityMatches.emplace_back(&candidate, mapping);
    }
    if (arityMatches.empty()) return;
    for (std::size_t index = 0; index < expression->children().size(); ++index)
    {
        if (expression->children()[index]->kind() != ast::AstNodeKind::LambdaExpression) continue;
        const std::size_t firstParameter =
            arityMatches.front().second.explicitParameterOrdinals[index];
        if (firstParameter >= arityMatches.front().first->parameters.size()) continue;
        const std::string& firstType = arityMatches.front().first->parameters[firstParameter].typeKey;
        const bool agreed = std::ranges::all_of(arityMatches, [&](const auto& candidate)
        {
            const std::size_t parameter = candidate.second.explicitParameterOrdinals[index];
            return parameter < candidate.first->parameters.size() &&
                candidate.first->parameters[parameter].typeKey == firstType;
        });
        if (agreed && isDelegateTypeKey(firstType))
        {
            lambdaTargetTypes[index] = firstType;
            continue;
        }

        // E11/R5: a lambda can only convert to a delegate, so non-delegate overloads
        // cannot make its target ambiguous. Explicit parameter types narrow compatible
        // delegate signatures further. An untyped lambda still does not guess between
        // two distinct delegate targets: it receives a context only when exactly one
        // delegate type remains.
        std::vector<std::string> compatibleDelegateTypes;
        const ast::AstNode* lambda = expression->children()[index];
        for (const auto& candidate : arityMatches)
        {
            const std::size_t parameter = candidate.second.explicitParameterOrdinals[index];
            if (parameter >= candidate.first->parameters.size()) continue;
            const std::string& candidateType = candidate.first->parameters[parameter].typeKey;
            std::string ignoredReturn;
            std::vector<std::string> candidateParameterTypes;
            std::size_t ignoredOrdinal = 0;
            if (!delegateSignature(
                    candidateType, ignoredReturn, candidateParameterTypes, ignoredOrdinal) ||
                candidateParameterTypes.size() != lambda->children().size())
                continue;

            bool matches = true;
            for (std::size_t lambdaParameter = 0;
                 lambdaParameter < lambda->children().size(); ++lambdaParameter)
            {
                const ast::AstNode* parameterSyntax = lambda->children()[lambdaParameter];
                if (parameterSyntax == nullptr || parameterSyntax->typeSyntax() == nullptr) continue;
                const std::string explicitType = memberModel_->canonicalTypeKey(
                    parameterSyntax->typeSyntax(), *nameLookup_);
                if (explicitType != candidateParameterTypes[lambdaParameter])
                {
                    matches = false;
                    break;
                }
            }
            if (matches && std::ranges::find(
                    compatibleDelegateTypes, candidateType) == compatibleDelegateTypes.end())
                compatibleDelegateTypes.push_back(candidateType);
        }
        if (compatibleDelegateTypes.size() == 1)
            lambdaTargetTypes[index] = compatibleDelegateTypes.front();
    }
}

std::vector<ProjectMethodBodyModel::ExpressionResult>
ProjectMethodBodyModel::prepareResolvedCallArguments(
    const ExpressionResult& callee,
    std::span<const ExpressionResult> arguments,
    bool coreStringForwarding) const
{
    std::vector<ExpressionResult> resolvedArguments;
    resolvedArguments.reserve(arguments.size() + (coreStringForwarding ? 1U : 0U));
    if (coreStringForwarding)
    {
        ExpressionResult receiverArgument;
        receiverArgument.typeKey = callee.receiverTypeKey;
        receiverArgument.valid = true;
        receiverArgument.integerConstant = callee.integerConstant;
        // Treat the receiver as a non-retained view for trusted NativeSharp.Core.String methods.
        receiverArgument.ownership = sema::OwnershipKind::None;
        receiverArgument.ownershipTransfer = false;
        resolvedArguments.push_back(std::move(receiverArgument));
    }
    resolvedArguments.insert(resolvedArguments.end(), arguments.begin(), arguments.end());
    return resolvedArguments;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::analyzeCall(
    const ast::AstNode* expression,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (const auto handled = analyzePreCallKinds(
            expression, methodMemberOrdinal, scopeOrdinal, diagnostics);
        handled.has_value()) return *handled;
    MethodCallBinding call;
    ExpressionResult callee;
    bool nullConditionalCall = false;
    if (const auto handled = prepareCallCallee(
            expression, call, callee, nullConditionalCall,
            methodMemberOrdinal, scopeOrdinal, diagnostics); handled.has_value()) return *handled;
    bool coreStringForwarding = false;
    std::vector<std::string> lambdaTargetTypes;
    configureCallLambdaTargets(
        expression, callee, call, coreStringForwarding, lambdaTargetTypes);
    std::vector<ExpressionResult> arguments;
    arguments.reserve(expression->children().size());
    bool allArgumentsValid = true;
    if (nullConditionalCall)
    {
        const OwnershipFlowSnapshot branchEntry = captureOwnershipFlow();
        const OwnershipFlowSnapshot argumentState = analyzeBranchState(branchEntry, [&]
        {
            analyzeCallArguments(
                arguments, allArgumentsValid, expression, callee, call, lambdaTargetTypes,
                coreStringForwarding, methodMemberOrdinal, scopeOrdinal, diagnostics);
        });
        commitBranchJoin(branchEntry, argumentState);
    }
    else
    {
        analyzeCallArguments(
            arguments, allArgumentsValid, expression, callee, call, lambdaTargetTypes,
            coreStringForwarding, methodMemberOrdinal, scopeOrdinal, diagnostics);
    }

    if (const auto handled = analyzeSpecialCallKinds(
            expression, callee, arguments, allArgumentsValid, call,
            methodMemberOrdinal, scopeOrdinal, diagnostics); handled.has_value()) return *handled;
    if (!callee.methodGroup)
    {
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        if (callee.valid)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                expression->range(),
                "call target is not an instance method group");
        }
        return result;
    }

    if (!allArgumentsValid)
    {
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return result;
    }

    // Stage 51 native string/primitive instance facade.  A call such as text.Trim()
    // resolves to the static Core.String signature Trim(string).  The receiver is
    // parameter zero for overload resolution and lowering, but is never an ownership
    // transfer merely because instance syntax was used.
    call.forwardedReceiver = coreStringForwarding;
    if (coreStringForwarding) call.interfaceCall = false;

    std::vector<ExpressionResult> resolvedArguments = prepareResolvedCallArguments(
        callee, arguments, coreStringForwarding);
    // NSC-LIMIT-021 / T41. A fresh string owner may be observed through the same string parameter
    // that already accepts a named RAII string. The hidden owner lives through the complete call
    // expression and is destroyed afterwards (or during unwind). This applies uniformly to
    // methods, interface dispatch and free functions; otherwise merely changing the callable
    // container changes whether `Log("prefix " + value)` is legal.
    // A literal's bytes have process lifetime. Passing that value to an explicitly borrowed
    // native-string parameter creates no owner and therefore needs neither a move nor a hidden
    // cleanup. Keep the exception tied to the literal type and an explicit borrowed contract.

    std::size_t selectedIndex = 0;
    if (!selectCallCandidate(
            selectedIndex, expression, callee, arguments, resolvedArguments,
            coreStringForwarding, call, diagnostics)) return result;
    return completeSelectedCall(
        expression, callee, arguments, resolvedArguments, coreStringForwarding,
        nullConditionalCall, selectedIndex, call, methodMemberOrdinal, scopeOrdinal, diagnostics);
}

bool ProjectMethodBodyModel::evaluateSyntheticCandidates(
    MethodCallBinding& call,
    const std::vector<ExpressionResult>& arguments)
{
    bool hasMatchingArity = false;
    if (memberModel_ == nullptr) return hasMatchingArity;
    for (const std::size_t candidateOrdinal : call.candidateMemberOrdinals)
    {
        const ClassMemberSymbol* candidatePtr = methodSymbol(candidateOrdinal);
        if (candidatePtr == nullptr || candidatePtr->kind != ClassMemberKind::Method) continue;
        const ClassMemberSymbol& candidate = *candidatePtr;
        MethodCallCandidate candidateBinding;
        candidateBinding.memberOrdinal = candidateOrdinal;
        if (candidate.parameters.size() != arguments.size())
        {
            call.evaluatedCandidates.push_back(std::move(candidateBinding));
            continue;
        }
        hasMatchingArity = true;
        candidateBinding.viable = true;
        for (std::size_t argumentOrdinal = 0; argumentOrdinal < arguments.size(); ++argumentOrdinal)
        {
            const MethodParameterSymbol& parameter = candidate.parameters[argumentOrdinal];
            const bool implicitOperatorReference = candidate.isUserOperator &&
                (parameter.passKind == ParameterPassKind::Ref ||
                 parameter.passKind == ParameterPassKind::In);
            const std::string_view conversionTarget = implicitOperatorReference
                ? std::string_view(parameter.pointeeTypeKey)
                : std::string_view(parameter.typeKey);
            ProjectImplicitConversion conversion = classifyImplicit(
                arguments[argumentOrdinal].typeKey,
                conversionTarget,
                arguments[argumentOrdinal].integerConstant);
            if (!conversion.exists() ||
                (implicitOperatorReference && conversion.kind != ProjectConversionKind::Identity) ||
                (implicitOperatorReference && !arguments[argumentOrdinal].addressable) ||
                (parameter.passKind == ParameterPassKind::Ref &&
                 (!arguments[argumentOrdinal].assignable ||
                  arguments[argumentOrdinal].readonlyReceiver)) ||
                (candidate.isUserOperator && parameter.passKind == ParameterPassKind::Out))
            {
                candidateBinding.viable = false;
            }
            candidateBinding.conversions.push_back(std::move(conversion));
        }
        if (candidateBinding.viable)
        {
            call.viableCandidateMemberOrdinals.push_back(candidateOrdinal);
        }
        call.evaluatedCandidates.push_back(std::move(candidateBinding));
    }
    return hasMatchingArity;
}

std::vector<std::size_t> ProjectMethodBodyModel::viableSyntheticCandidates(
    const MethodCallBinding& call) const
{
    std::vector<std::size_t> viable;
    for (std::size_t index = 0; index < call.evaluatedCandidates.size(); ++index)
    {
        if (call.evaluatedCandidates[index].viable) viable.push_back(index);
    }
    return viable;
}

void ProjectMethodBodyModel::diagnoseSyntheticNoViable(
    const ast::AstNode* syntax,
    const MethodCallBinding& call,
    const std::vector<ExpressionResult>& arguments,
    bool implicitOperatorCall,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // C4: when exactly one overload has the right arity and the ONLY thing wrong is the
    // missing (or extra) `ref`/`out`/`in`, say that instead of "no viable overload" --
    // the type mismatch the programmer sees (`int` vs `int*`) is an artifact of how a
    // by-reference parameter is represented, not something they wrote.
    std::size_t arityMatches = 0;
    std::size_t soleCandidate = 0;
    for (const MethodCallCandidate& candidate : call.evaluatedCandidates)
    {
        if (candidate.conversions.size() != arguments.size()) continue;
        ++arityMatches;
        soleCandidate = candidate.memberOrdinal;
    }
    bool reportedPassKind = false;
    if (arityMatches == 1 && memberModel_ != nullptr &&
        soleCandidate < memberModel_->members().size())
    {
        const ClassMemberSymbol& only = memberModel_->members()[soleCandidate];
        for (std::size_t index = 0;
             !implicitOperatorCall && !only.isUserOperator && index < arguments.size() &&
                 index < only.parameters.size();
             ++index)
        {
            if (only.parameters[index].passKind == arguments[index].argumentPassKind) continue;
            reportedPassKind = true;
            break;
        }
        if (reportedPassKind)
        {
            checkArgumentPassKinds(
                syntax, call.methodName, soleCandidate, arguments, diagnostics);
        }
    }
    if (!reportedPassKind)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidConversion,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "no viable Stage 38 overload of '" + call.methodName +
                "' accepts the operand/argument types");
    }
}

std::vector<std::size_t> ProjectMethodBodyModel::nonDominatedSyntheticCandidates(
    const MethodCallBinding& call,
    const std::vector<std::size_t>& viableEvaluationOrdinals) const
{
    std::vector<std::size_t> nonDominated;
    for (const std::size_t candidateIndex : viableEvaluationOrdinals)
    {
        bool dominated = false;
        for (const std::size_t otherIndex : viableEvaluationOrdinals)
        {
            if (candidateIndex == otherIndex) continue;
            if (!betterCandidate(
                    call.evaluatedCandidates[otherIndex],
                    call.evaluatedCandidates[candidateIndex])) continue;
            dominated = true;
            break;
        }
        if (!dominated) nonDominated.push_back(candidateIndex);
    }
    return nonDominated;
}

bool ProjectMethodBodyModel::validateSyntheticReadonlyReceiver(
    const ast::AstNode* syntax,
    MethodCallBinding& call,
    const MethodCallCandidate& selected,
    bool diagnoseFailure,
    bool readonlyReceiver,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (memberModel_ == nullptr || selected.memberOrdinal >= memberModel_->members().size())
    {
        return true;
    }
    const ClassMemberSymbol& selectedMember = memberModel_->members()[selected.memberOrdinal];
    if (!readonlyReceiver || !selectedMember.declaringTypeIsStruct ||
        selectedMember.isStatic || selectedMember.isReadonly) return true;
    if (diagnoseFailure)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            syntax != nullptr ? syntax->range() : SourceRange{},
            "readonly receiver cannot invoke mutating struct member '" +
                selectedMember.name + "'");
    }
    call.resolution = MethodCallResolutionKind::NoViableCandidate;
    recordCallBinding(std::move(call));
    return false;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::finishSyntheticCall(
    const ast::AstNode* syntax,
    MethodCallBinding& call,
    const MethodCallCandidate& selected,
    const std::vector<ExpressionResult>& arguments,
    bool diagnoseFailure,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ExpressionResult result;
    if (diagnoseFailure && memberModel_ != nullptr &&
        selected.memberOrdinal < memberModel_->members().size() &&
        !memberModel_->members()[selected.memberOrdinal].isUserOperator)
    {
        checkArgumentPassKinds(
            syntax, call.methodName, selected.memberOrdinal, arguments, diagnostics);
    }
    call.resolution = MethodCallResolutionKind::Resolved;
    call.selectedMemberOrdinal = selected.memberOrdinal;
    call.selectedConversions = selected.conversions;
    const ClassMemberSymbol* memberPtr = methodSymbol(selected.memberOrdinal);
    if (memberPtr != nullptr)
    {
        call.resultTypeKey = memberPtr->declaredTypeKey;
        call.staticCall = memberPtr->isStatic;
        // EN: Freeze 3.0 P3: an accessor declared `ref readonly` (see configureSelectedCallBinding).
        // ES: Congelamiento 3.0 P3: un accesor declarado `ref readonly` (ver
        //     configureSelectedCallBinding).
        const bool readonlyReference = isReadonlyReferenceReturn(*memberPtr);
        result.assignable = memberPtr->isReferenceReturn && !readonlyReference;
        result.addressable = memberPtr->isReferenceReturn;
        result.readonlyReceiver = readonlyReference;
        // EN: Freeze 3.0 C-01: a user operator or conversion that declares an `owned` return
        //     transfers ownership exactly like an ordinary owned-returning call, so
        //     `raii T sum = a + b;` takes ownership of the operator's new object.
        // ES: Congelamiento 3.0 C-01: un operador o conversion de usuario con retorno `owned`
        //     transfiere la propiedad igual que una llamada ordinaria, asi que
        //     `raii T sum = a + b;` toma la propiedad del objeto nuevo del operador.
        if (memberPtr->declaredOwnership == sema::OwnershipKind::Owned)
        {
            result.ownership = sema::OwnershipKind::Owned;
            result.ownershipTransfer = true;
            result.ownershipFreshOwner = true;
        }
        // EN: Freeze 3.0 C-13: an accessor or a user operator is a call to its `throws E` member.
        // ES: Congelamiento 3.0 C-13: un accesor o un operador de usuario es una llamada a su miembro
        //     `throws E`.
        if (diagnoseFailure)
            requireDeclaredErrorHandled(
                syntax, memberPtr->declaredErrorTypeKey, memberPtr->name,
                call.callerMethodMemberOrdinal, diagnostics);
        if (call.staticCall)
        {
            call.implicitThis = false;
            call.explicitThis = false;
            call.baseReceiver = false;
            call.receiverSyntax = nullptr;
            call.receiverTypeKey.clear();
            call.receiverTypeOrdinal.reset();
            call.interfaceCall = false;
        }
    }
    result.typeKey = call.resultTypeKey;
    result.valid = !isErrorType(result.typeKey);
    recordCallBinding(std::move(call));
    return result;
}

ProjectMethodBodyModel::ExpressionResult ProjectMethodBodyModel::resolveSyntheticCall(
    const ast::AstNode* syntax,
    std::size_t methodMemberOrdinal,
    std::size_t scopeOrdinal,
    std::string methodName,
    std::vector<std::size_t> candidates,
    const std::vector<ExpressionResult>& arguments,
    const ast::AstNode* receiverSyntax,
    std::string receiverTypeKey,
    std::optional<std::size_t> receiverTypeOrdinal,
    bool baseReceiver,
    bool interfaceCall,
    diagnostics::DiagnosticEngine& diagnostics,
    bool diagnoseFailure,
    bool readonlyReceiver)
{
    ExpressionResult result;
    MethodCallBinding call;
    call.syntax = syntax;
    call.callerMethodMemberOrdinal = methodMemberOrdinal;
    call.scopeOrdinal = scopeOrdinal;
    call.methodName = std::move(methodName);
    call.receiverSyntax = receiverSyntax;
    call.receiverTypeKey = std::move(receiverTypeKey);
    call.receiverTypeOrdinal = receiverTypeOrdinal;
    call.baseReceiver = baseReceiver;
    call.interfaceCall = interfaceCall;
    call.implicitThis = receiverSyntax == nullptr && !candidates.empty();
    call.candidateMemberOrdinals = std::move(candidates);

    const bool implicitOperatorCall =
        language::operatorTraitContractForMethod(call.methodName, arguments.size()) != nullptr;
    if (!evaluateSyntheticCandidates(call, arguments))
    {
        if (diagnoseFailure)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                syntax != nullptr ? syntax->range() : SourceRange{},
                "no Stage 38 overload of '" + call.methodName + "' accepts " +
                    std::to_string(arguments.size()) + " argument(s)");
        }
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return result;
    }

    const std::vector<std::size_t> viable = viableSyntheticCandidates(call);
    if (viable.empty())
    {
        if (diagnoseFailure)
        {
            diagnoseSyntheticNoViable(
                syntax, call, arguments, implicitOperatorCall, diagnostics);
        }
        call.resolution = MethodCallResolutionKind::NoViableCandidate;
        recordCallBinding(std::move(call));
        return result;
    }

    const std::vector<std::size_t> nonDominated =
        nonDominatedSyntheticCandidates(call, viable);
    if (nonDominated.size() != 1)
    {
        if (diagnoseFailure)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                syntax != nullptr ? syntax->range() : SourceRange{},
                "ambiguous Stage 38 call to '" + call.methodName + "'");
        }
        call.resolution = MethodCallResolutionKind::Ambiguous;
        recordCallBinding(std::move(call));
        return result;
    }

    const MethodCallCandidate& selected = call.evaluatedCandidates[nonDominated.front()];
    if (!validateSyntheticReadonlyReceiver(
            syntax, call, selected, diagnoseFailure, readonlyReceiver, diagnostics))
    {
        return result;
    }
    return finishSyntheticCall(
        syntax, call, selected, arguments, diagnoseFailure, diagnostics);
}

}
