#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "MethodBodyInternals.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/MemoryModel.h"
#include "nativesharp/Language/TraitSemantics.h"

#include <algorithm>
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
using method_body_detail::pointerContract;

namespace
{
int escapeClassRank(language::EscapeClass item) noexcept
{
    switch (item)
    {
        case language::EscapeClass::NoEscape: return 0;
        case language::EscapeClass::Return: return 1;
        case language::EscapeClass::CallerOwned: return 2;
        case language::EscapeClass::Global: return 3;
        case language::EscapeClass::Unknown: return 4;
    }
    return 4;
}
}

MethodEscapeOrigin ProjectMethodBodyModel::referenceEscapeOriginOfExpression(
    const ast::AstNode* expression,
    std::size_t scopeOrdinal) const
{
    return ProjectReferenceEscapeEvaluator(*this, scopeOrdinal).evaluate(expression);
}

bool ProjectMethodBodyModel::parameterMayCarryEscape(
    const MethodParameterSymbol& parameter) const noexcept
{
    return parameter.passKind != ParameterPassKind::Value ||
        parameter.ownership == sema::OwnershipKind::Borrowed ||
        parameter.typeKey.ends_with("*") || !parameter.lifetimeName.empty() ||
        typeKeyIsViewLike(parameter.typeKey);
}

std::vector<language::EscapeClass>*
ProjectMethodBodyModel::mutableParameterEscapeSummary(
    std::size_t callableOrdinal) noexcept
{
    if (isolatedCallableOrdinal_.has_value() &&
        *isolatedCallableOrdinal_ == callableOrdinal)
    {
        return &isolatedParameterEscapeSummary_;
    }
    return callableOrdinal < parameterEscapeSummaries_.size()
        ? &parameterEscapeSummaries_[callableOrdinal]
        : nullptr;
}

void ProjectMethodBodyModel::noteParameterEscape(
    std::size_t callableOrdinal,
    std::size_t parameterOrdinal,
    language::EscapeClass escapeClass)
{
    std::vector<language::EscapeClass>* summary =
        mutableParameterEscapeSummary(callableOrdinal);
    if (summary == nullptr || parameterOrdinal >= summary->size()) return;
    const ClassMemberSymbol* callable = methodSymbol(callableOrdinal);
    if (callable == nullptr || parameterOrdinal >= callable->parameters.size() ||
        !parameterMayCarryEscape(callable->parameters[parameterOrdinal]))
    {
        return;
    }
    if (escapeClassRank(escapeClass) > escapeClassRank((*summary)[parameterOrdinal]))
        (*summary)[parameterOrdinal] = escapeClass;
}

void ProjectMethodBodyModel::finalizeEscapeSummaries(
    diagnostics::DiagnosticEngine& diagnostics)
{
    // A closure value can be returned, stored or passed onward. Until T08 gives captures an
    // explicit non-escaping callable type, a captured parameter is a global/heap escape.
    for (const MethodLambdaBinding& lambda : lambdaBindings_)
    {
        for (const MethodLambdaCapture& capture : lambda.captures)
        {
            if (capture.kind == MethodLambdaCaptureKind::Parameter &&
                capture.parameterOrdinal.has_value())
            {
                noteParameterEscape(lambda.ownerMemberOrdinal, *capture.parameterOrdinal,
                    language::EscapeClass::Global);
            }
        }
    }

    // Build one cell per (callable, parameter), then reverse edges from a callee cell to the
    // caller cells that depend on it. Argument origins are classified exactly once here. The old
    // fixed-point loop reclassified every argument and rescanned every call after every change.
    std::vector<std::size_t> callableCellOffsets(parameterEscapeSummaries_.size() + 1U, 0U);
    for (std::size_t callableOrdinal = 0;
         callableOrdinal < parameterEscapeSummaries_.size();
         ++callableOrdinal)
    {
        callableCellOffsets[callableOrdinal + 1U] =
            callableCellOffsets[callableOrdinal] +
            parameterEscapeSummaries_[callableOrdinal].size();
    }
    const std::size_t cellCount = callableCellOffsets.back();
    escapePropagationStatistics_.cells = cellCount;
    std::vector<std::size_t> cellCallableOrdinals(cellCount, 0U);
    std::vector<std::size_t> cellParameterOrdinals(cellCount, 0U);
    for (std::size_t callableOrdinal = 0;
         callableOrdinal < parameterEscapeSummaries_.size();
         ++callableOrdinal)
    {
        for (std::size_t parameterOrdinal = 0;
             parameterOrdinal < parameterEscapeSummaries_[callableOrdinal].size();
             ++parameterOrdinal)
        {
            const std::size_t cell = callableCellOffsets[callableOrdinal] + parameterOrdinal;
            cellCallableOrdinals[cell] = callableOrdinal;
            cellParameterOrdinals[cell] = parameterOrdinal;
        }
    }

    std::vector<std::vector<std::size_t>> dependents(cellCount);
    const auto addDependencies = [&](std::size_t callerOrdinal,
                                     std::size_t scopeOrdinal,
                                     std::size_t calleeOrdinal,
                                     std::span<const std::size_t> parameterOrdinals,
                                     std::span<const ast::AstNode* const> argumentSyntaxes)
    {
        if (callerOrdinal >= parameterEscapeSummaries_.size() ||
            calleeOrdinal >= parameterEscapeSummaries_.size())
            return;
        const ClassMemberSymbol* caller = methodSymbol(callerOrdinal);
        if (caller == nullptr) return;
        for (std::size_t argumentOrdinal = 0;
             argumentOrdinal < parameterOrdinals.size() &&
             argumentOrdinal < argumentSyntaxes.size(); ++argumentOrdinal)
        {
            const std::size_t calleeParameter = parameterOrdinals[argumentOrdinal];
            if (calleeParameter >= parameterEscapeSummaries_[calleeOrdinal].size() ||
                argumentSyntaxes[argumentOrdinal] == nullptr)
                continue;
            const MethodEscapeOrigin origin = referenceEscapeOriginOfExpression(
                argumentSyntaxes[argumentOrdinal], scopeOrdinal);
            ++escapePropagationStatistics_.argumentOriginsClassified;
            if (!origin.ordinal.has_value() ||
                (origin.kind != MethodEscapeOriginKind::ExternalStorage &&
                 origin.kind != MethodEscapeOriginKind::CurrentFrameParameter) ||
                *origin.ordinal >= caller->parameters.size() ||
                !parameterMayCarryEscape(caller->parameters[*origin.ordinal]))
                continue;

            const std::size_t sourceCell =
                callableCellOffsets[calleeOrdinal] + calleeParameter;
            const std::size_t dependentCell =
                callableCellOffsets[callerOrdinal] + *origin.ordinal;
            dependents[sourceCell].push_back(dependentCell);
        }
    };
    for (const MethodCallBinding& call : calls_)
    {
        if (call.resolution == MethodCallResolutionKind::Resolved &&
            call.selectedMemberOrdinal.has_value())
        {
            addDependencies(
                call.callerMethodMemberOrdinal,
                call.scopeOrdinal,
                *call.selectedMemberOrdinal,
                call.selectedArgumentParameterOrdinals,
                call.selectedArgumentSyntaxes);
        }
    }
    for (const NamespaceFunctionCallBinding& call : namespaceFunctionCalls_)
    {
        if (call.resolution == MethodCallResolutionKind::Resolved &&
            call.selectedFunctionOrdinal.has_value())
        {
            addDependencies(
                call.callerCallableOrdinal,
                call.scopeOrdinal,
                namespaceCallableBase_ + *call.selectedFunctionOrdinal,
                call.selectedArgumentParameterOrdinals,
                call.selectedArgumentSyntaxes);
        }
    }
    for (std::vector<std::size_t>& targets : dependents)
    {
        std::ranges::sort(targets);
        targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
        escapePropagationStatistics_.edges += targets.size();
    }

    std::vector<std::size_t> worklist;
    worklist.reserve(cellCount);
    std::vector<bool> queued(cellCount, true);
    for (std::size_t cell = 0; cell < cellCount; ++cell)
        worklist.push_back(cell);
    std::size_t cursor = 0;
    while (cursor < worklist.size())
    {
        const std::size_t sourceCell = worklist[cursor++];
        ++escapePropagationStatistics_.cellsEvaluated;
        queued[sourceCell] = false;
        const std::size_t sourceCallable = cellCallableOrdinals[sourceCell];
        const std::size_t sourceParameter = cellParameterOrdinals[sourceCell];
        const language::EscapeClass sourceClass =
            parameterEscapeSummaries_[sourceCallable][sourceParameter];
        for (const std::size_t dependentCell : dependents[sourceCell])
        {
            ++escapePropagationStatistics_.edgeEvaluations;
            const std::size_t targetCallable = cellCallableOrdinals[dependentCell];
            const std::size_t targetParameter = cellParameterOrdinals[dependentCell];
            language::EscapeClass& targetClass =
                parameterEscapeSummaries_[targetCallable][targetParameter];
            if (escapeClassRank(sourceClass) <= escapeClassRank(targetClass)) continue;
            targetClass = sourceClass;
            ++escapePropagationStatistics_.summaryUpdates;
            if (!queued[dependentCell])
            {
                queued[dependentCell] = true;
                worklist.push_back(dependentCell);
            }
        }
    }

    const auto validate = [&](std::size_t scopeOrdinal,
                              std::string_view calleeName,
                              std::size_t calleeOrdinal,
                              std::span<const std::size_t> parameterOrdinals,
                              std::span<const ast::AstNode* const> argumentSyntaxes)
    {
        if (calleeOrdinal >= parameterEscapeSummaries_.size()) return;
        const std::vector<language::EscapeClass>& callee =
            parameterEscapeSummaries_[calleeOrdinal];
        for (std::size_t argumentOrdinal = 0;
             argumentOrdinal < parameterOrdinals.size() &&
             argumentOrdinal < argumentSyntaxes.size(); ++argumentOrdinal)
        {
            const std::size_t targetParameter = parameterOrdinals[argumentOrdinal];
            if (targetParameter >= callee.size() || argumentSyntaxes[argumentOrdinal] == nullptr)
                continue;
            const language::EscapeClass contract = callee[targetParameter];
            if (contract != language::EscapeClass::Global) continue;
            const MethodEscapeOrigin origin = referenceEscapeOriginOfExpression(
                argumentSyntaxes[argumentOrdinal], scopeOrdinal);
            if (origin.kind != MethodEscapeOriginKind::CurrentFrameLocal &&
                origin.kind != MethodEscapeOriginKind::VariableLengthArray)
                continue;
            diagnostics.report(
                diagnostics::DiagnosticId::NS3020InvalidValueCategory,
                diagnostics::DiagnosticSeverity::Error,
                argumentSyntaxes[argumentOrdinal]->range(),
                "argument " + std::to_string(argumentOrdinal + 1U) + " of '" +
                    std::string(calleeName) + "' refers to current-frame storage, but parameter " +
                    std::to_string(targetParameter + 1U) +
                    " is retained by the callee's published escape contract");
        }
    };
    for (const MethodCallBinding& call : calls_)
    {
        if (call.resolution == MethodCallResolutionKind::Resolved &&
            call.selectedMemberOrdinal.has_value())
            validate(call.scopeOrdinal, call.methodName, *call.selectedMemberOrdinal,
                call.selectedArgumentParameterOrdinals, call.selectedArgumentSyntaxes);
    }
    for (const NamespaceFunctionCallBinding& call : namespaceFunctionCalls_)
    {
        if (call.resolution == MethodCallResolutionKind::Resolved &&
            call.selectedFunctionOrdinal.has_value())
            validate(call.scopeOrdinal, call.writtenName,
                namespaceCallableBase_ + *call.selectedFunctionOrdinal,
                call.selectedArgumentParameterOrdinals, call.selectedArgumentSyntaxes);
    }
}
}
