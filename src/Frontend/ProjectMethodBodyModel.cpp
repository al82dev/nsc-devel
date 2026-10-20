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
using method_body_detail::pointerContract;
using method_body_detail::Stage37ConstantKind;
using method_body_detail::Stage37ConstantValue;
using method_body_detail::writeMethodBodyTrace;









std::span<const MethodScope> ProjectMethodBodyModel::scopes() const noexcept
{
    return scopes_;
}

std::span<const MethodLocalSymbol> ProjectMethodBodyModel::locals() const noexcept
{
    return locals_;
}

const std::vector<const ast::AstNode*>& ProjectMethodBodyModel::awaitExpressions() const noexcept
{
    return awaitExpressions_;
}

std::span<const MethodNameBinding> ProjectMethodBodyModel::nameBindings() const noexcept
{
    return nameBindings_;
}

const MethodNameBinding* ProjectMethodBodyModel::nameBinding(
    const ast::AstNode* syntax,
    std::size_t callableOrdinal) const noexcept
{
    const std::optional<std::size_t> bindingOrdinal =
        nameBindingIndex_.latest(syntax, callableOrdinal);
    if (!bindingOrdinal.has_value() || *bindingOrdinal >= nameBindings_.size())
    {
        return nullptr;
    }
    return &nameBindings_[*bindingOrdinal];
}

std::span<const MethodShadowingEvent> ProjectMethodBodyModel::shadowingEvents() const noexcept
{
    return shadowingEvents_;
}

std::span<const MethodExpressionTypeBinding> ProjectMethodBodyModel::expressionTypes() const noexcept
{
    return expressionTypes_;
}

std::span<const ProjectConstructionSite> ProjectMethodBodyModel::constructionSites() const noexcept
{
    return constructionSites_;
}

const MethodExpressionTypeBinding* ProjectMethodBodyModel::expressionTypeBinding(
    const ast::AstNode* syntax,
    std::size_t callableOrdinal) const noexcept
{
    const std::optional<std::size_t> bindingOrdinal =
        expressionTypeBindingIndex_.latest(syntax, callableOrdinal);
    if (!bindingOrdinal.has_value() || *bindingOrdinal >= expressionTypes_.size())
    {
        return nullptr;
    }
    return &expressionTypes_[*bindingOrdinal];
}

const MethodAggregateInitializerBinding* ProjectMethodBodyModel::aggregateInitializerBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto found = aggregateInitializerBindingBySyntax_.find(syntax);
    return found == aggregateInitializerBindingBySyntax_.end()
        ? nullptr : &aggregateInitializerBindings_[found->second];
}

const MethodDeconstructionBinding* ProjectMethodBodyModel::deconstructionBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto found = deconstructionBindingBySyntax_.find(syntax);
    return found == deconstructionBindingBySyntax_.end()
        ? nullptr : &deconstructionBindings_[found->second];
}

const MethodMatchExpressionBinding* ProjectMethodBodyModel::matchExpressionBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto found = matchExpressionBindingBySyntax_.find(syntax);
    return found == matchExpressionBindingBySyntax_.end()
        ? nullptr : &matchExpressionBindings_[found->second];
}

const MethodLoopExpressionBinding* ProjectMethodBodyModel::loopExpressionBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto found = loopExpressionBindingBySyntax_.find(syntax);
    return found == loopExpressionBindingBySyntax_.end()
        ? nullptr : &loopExpressionBindings_[found->second];
}

const MethodLoopBreakBinding* ProjectMethodBodyModel::loopBreakBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto found = loopBreakBindingBySyntax_.find(syntax);
    return found == loopBreakBindingBySyntax_.end()
        ? nullptr : &loopBreakBindings_[found->second];
}


const std::string* ProjectMethodBodyModel::staticInterpolationValue(
    const ast::AstNode* syntax) const noexcept
{
    const auto found = staticInterpolationValues_.find(syntax);
    return found == staticInterpolationValues_.end() ? nullptr : &found->second;
}

std::span<const MethodRuntimeTypeBinding> ProjectMethodBodyModel::runtimeTypeBindings() const noexcept
{
    return runtimeTypeBindings_;
}

std::span<const MethodCoreBinding> ProjectMethodBodyModel::coreBindings() const noexcept
{
    return coreBindings_;
}

const MethodVariableArrayIndexBinding* ProjectMethodBodyModel::variableArrayIndexBinding(
    const ast::AstNode* syntax) const noexcept
{
    const auto found = std::find_if(
        variableArrayIndexBindings_.begin(),
        variableArrayIndexBindings_.end(),
        [&](const MethodVariableArrayIndexBinding& binding)
        {
            return binding.syntax == syntax;
        });
    return found == variableArrayIndexBindings_.end() ? nullptr : &*found;
}

std::span<const MethodExceptionBinding> ProjectMethodBodyModel::exceptionBindings() const noexcept
{
    return exceptionBindings_;
}

std::span<const MethodCleanupRegion> ProjectMethodBodyModel::cleanupRegions() const noexcept
{
    return cleanupRegions_;
}

std::span<const MethodUsingBinding> ProjectMethodBodyModel::usingBindings() const noexcept
{
    return usingBindings_;
}

std::span<const MethodMoveBinding> ProjectMethodBodyModel::moveBindings() const noexcept
{
    return moveBindings_;
}

std::span<const MethodOwnershipTransferBinding> ProjectMethodBodyModel::ownershipTransferBindings() const noexcept
{
    return ownershipTransferBindings_;
}

std::span<const MethodBorrowBinding> ProjectMethodBodyModel::borrowBindings() const noexcept
{
    return borrowBindings_;
}

std::span<const MethodLocalBorrowBinding> ProjectMethodBodyModel::localBorrowBindings() const noexcept
{
    return localBorrowBindings_;
}

std::span<const MethodSwitchBinding> ProjectMethodBodyModel::switchBindings() const noexcept
{
    return switchBindings_;
}

std::span<const MethodStructuredExitBinding> ProjectMethodBodyModel::structuredExits() const noexcept
{
    return structuredExits_;
}

std::span<const MethodCleanupContinuation> ProjectMethodBodyModel::cleanupContinuations() const noexcept
{
    return cleanupContinuations_;
}

std::span<const MethodLambdaBinding> ProjectMethodBodyModel::lambdaBindings() const noexcept
{
    return lambdaBindings_;
}

std::span<const MethodLambdaValueBinding> ProjectMethodBodyModel::lambdaValueBindings() const noexcept
{
    return lambdaValueBindings_;
}

std::span<const MethodDelegateBinding> ProjectMethodBodyModel::delegateBindings() const noexcept
{
    return delegateBindings_;
}

std::span<const MethodEventBinding> ProjectMethodBodyModel::eventBindings() const noexcept
{
    return eventBindings_;
}

std::span<const MethodOptionalCarrierBinding>
ProjectMethodBodyModel::optionalCarrierBindings() const noexcept
{
    return optionalCarrierBindings_;
}

std::span<const MethodResultPropagationBinding>
ProjectMethodBodyModel::resultPropagationBindings() const noexcept
{
    return resultPropagationBindings_;
}

std::span<const MethodRangeSliceBinding>
ProjectMethodBodyModel::rangeSliceBindings() const noexcept
{
    return rangeSliceBindings_;
}

std::span<const MethodCallBinding> ProjectMethodBodyModel::calls() const noexcept
{
    return calls_;
}

const MethodCallBinding* ProjectMethodBodyModel::callBinding(
    const ast::AstNode* syntax,
    std::size_t callableOrdinal) const noexcept
{
    const std::optional<std::size_t> bindingOrdinal =
        callBindingIndex_.latest(syntax, callableOrdinal);
    if (!bindingOrdinal.has_value() || *bindingOrdinal >= calls_.size())
    {
        return nullptr;
    }
    return &calls_[*bindingOrdinal];
}

std::span<const NamespaceFunctionCallBinding>
ProjectMethodBodyModel::namespaceFunctionCalls() const noexcept
{
    return namespaceFunctionCalls_;
}

const NamespaceFunctionCallBinding* ProjectMethodBodyModel::namespaceFunctionCall(
    const ast::AstNode* syntax) const noexcept
{
    const auto found = namespaceFunctionCallBySyntax_.find(syntax);
    return found == namespaceFunctionCallBySyntax_.end()
        ? nullptr : &namespaceFunctionCalls_[found->second];
}

bool ProjectMethodBodyModel::callableBodyAnalyzed(std::size_t memberOrdinal) const noexcept
{
    return analyzedCallableMembers_.contains(memberOrdinal);
}

bool ProjectMethodBodyModel::namespaceFunctionBodyAnalyzed(
    std::size_t functionOrdinal) const noexcept
{
    return analyzedCallableMembers_.contains(namespaceCallableBase_ + functionOrdinal);
}

std::span<const language::EscapeClass> ProjectMethodBodyModel::parameterEscapeSummary(
    std::size_t callableOrdinal) const noexcept
{
    static const std::vector<language::EscapeClass> empty;
    return callableOrdinal < parameterEscapeSummaries_.size()
        ? std::span<const language::EscapeClass>(parameterEscapeSummaries_[callableOrdinal])
        : std::span<const language::EscapeClass>(empty);
}

const ProjectOwnershipFlowStatistics&
ProjectMethodBodyModel::ownershipFlowStatistics() const noexcept
{
    return ownershipFlowStatistics_;
}

const ProjectEscapePropagationStatistics&
ProjectMethodBodyModel::escapePropagationStatistics() const noexcept
{
    return escapePropagationStatistics_;
}

const ProjectCallableAnalysisStatistics&
ProjectMethodBodyModel::callableAnalysisStatistics() const noexcept
{
    return callableAnalysisStatistics_;
}

bool ProjectMethodBodyModel::isBuilt() const noexcept
{
    return built_;
}




}
