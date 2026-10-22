#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "Support/OwnershipArgumentClassification.h"
#include "Support/OperatorClassification.h"
#include "Support/IntegralConstantArithmetic.h"
#include "Support/OwnershipFlowSnapshotQueries.h"


#include "MethodBodyInternals.h"
#include "../Internal/Stage37ConstantValue.h"

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

bool ProjectMethodBodyModel::callableCanBeAnalyzed(
    const ProjectCallableAnalysisContext& context) const
{
    const std::size_t callableOrdinal = context.callableOrdinal();
    if (analyzedCallableMembers_.contains(callableOrdinal)) return false;
    const ClassMemberSymbol* callable = methodSymbol(callableOrdinal);
    if (callable == nullptr) return false;
    // NS304-T08-F06: an open generic namespace function's body is deferred to its specialization;
    // only the concrete clone (`Twice<I>`) is analyzed.
    if (callable->syntax != nullptr && !callable->syntax->genericParameters().empty())
        return false;
    const ProjectGenericConstraintModel* constraints = context.genericConstraintModel();
    if (callableOrdinal >= namespaceCallableBase_ || constraints == nullptr)
        return true;

    if (!constraints->specializationSatisfiesConstraints(callable->syntax))
        return false;
    const ProjectNameLookup& names = context.nameLookup();
    return callable->declaringTypeOrdinal >= names.namedTypes().size() ||
        constraints->specializationSatisfiesConstraints(
            names.namedTypes()[callable->declaringTypeOrdinal].syntax);
}

void ProjectMethodBodyModel::resetCallableScratch(const ClassMemberSymbol& callable)
{
    currentCallableLocalBase_ = locals_.size();
    currentCallableParameterCount_ = callable.parameters.size();
    movedRaiiLocals_.clear();
    maybeMovedRaiiLocals_.clear();
    movedOwnedParameters_.clear();
    maybeMovedOwnedParameters_.clear();
    invokedFnOnceLocals_.clear();
    maybeInvokedFnOnceLocals_.clear();
    partiallyMovedPlaces_.clear();
    maybePartiallyMovedPlaces_.clear();
    invalidLocalBorrows_.clear();
    maybeInvalidLocalBorrows_.clear();
    assignedLocals_.clear();
    activeUnionMembers_.clear();
    nonNullStorages_.clear();
    integerRanges_.clear();
    activeVariantCases_.clear();
    conditionFlowBySyntax_.clear();
    inUnevaluatedOperand_ = false;
    definiteAssignmentWriteTarget_ = nullptr;
    activeUnionWriteTarget_ = nullptr;
    ownershipFlowReachable_ = true;
    activeRequiredFieldAssignments_.clear();
    hasRequiredConstructorExit_ = false;
    requiredConstructorExitAssignments_.clear();
    partialProjectionReceiverDepth_ = 0;
    throwFlowStates_.clear();
    awaitExpressions_.clear();
}

void ProjectMethodBodyModel::recordRequiredConstructorNormalExit(
    std::size_t callableOrdinal)
{
    const ClassMemberSymbol* callable = methodSymbol(callableOrdinal);
    if (callable == nullptr || callable->kind != ClassMemberKind::Constructor ||
        callable->isStatic)
        return;

    if (!hasRequiredConstructorExit_)
    {
        requiredConstructorExitAssignments_ = activeRequiredFieldAssignments_;
        hasRequiredConstructorExit_ = true;
        return;
    }
    std::erase_if(
        requiredConstructorExitAssignments_,
        [&](std::size_t fieldOrdinal)
        {
            return !activeRequiredFieldAssignments_.contains(fieldOrdinal);
        });
}

void ProjectMethodBodyModel::finalizeRequiredConstructorAssignments(
    const ClassMemberSymbol& callable,
    std::size_t callableOrdinal)
{
    if (callable.kind != ClassMemberKind::Constructor || callable.isStatic) return;
    if (ownershipFlowReachable_) recordRequiredConstructorNormalExit(callableOrdinal);
    if (!hasRequiredConstructorExit_) return;

    constructorsWithNormalExit_.insert(callableOrdinal);
    for (const std::size_t fieldOrdinal : requiredConstructorExitAssignments_)
        assignedRequiredFields_.insert({callableOrdinal, fieldOrdinal});
}

std::vector<std::size_t> ProjectMethodBodyModel::createOwnedParameterCleanups(
    const ClassMemberSymbol& callable,
    std::size_t callableOrdinal,
    std::size_t rootScopeOrdinal)
{
    std::vector<std::size_t> cleanupOrdinals;
    for (std::size_t parameterOrdinal = 0;
         parameterOrdinal < callable.parameters.size(); ++parameterOrdinal)
    {
        const MethodParameterSymbol& parameter = callable.parameters[parameterOrdinal];
        const bool valueDrop = parameter.passKind == ParameterPassKind::Value &&
            memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(parameter.typeKey);
        if (parameter.ownership != sema::OwnershipKind::Owned && !valueDrop) continue;
        const std::size_t cleanupOrdinal = createCleanupRegion(
            valueDrop ? MethodCleanupRegionKind::ValueParameterDrop
                      : MethodCleanupRegionKind::OwnedParameterDestroy,
            callable.syntax, callable.syntax, callableOrdinal, rootScopeOrdinal);
        cleanupRegions_[cleanupOrdinal].resourceParameterOrdinal = parameterOrdinal;
        activeCleanupRegions_.push_back(cleanupOrdinal);
        cleanupOrdinals.push_back(cleanupOrdinal);
    }
    return cleanupOrdinals;
}

void ProjectMethodBodyModel::analyzeConstructorInitializer(
    const ClassMemberSymbol& callable,
    std::size_t callableOrdinal,
    std::size_t rootScopeOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (callable.kind != ClassMemberKind::Constructor || callable.isStatic ||
        callable.syntax->second() == nullptr)
        return;
    const ast::AstNode* initializer = callable.syntax->second();
    if (initializer->kind() != ast::AstNodeKind::BaseConstructorInitializer &&
        initializer->kind() != ast::AstNodeKind::ThisConstructorInitializer)
        return;
    for (const ast::AstNode* argument : initializer->children())
    {
        analyzeExpression(
            argument, callableOrdinal, rootScopeOrdinal, diagnostics,
            /*allowMethodGroup=*/false, /*allowEvent=*/false, /*allowMove=*/true);
    }
}

void ProjectMethodBodyModel::closeOwnedParameterCleanups(
    std::span<const std::size_t> cleanupOrdinals,
    const ClassMemberSymbol& callable,
    std::size_t callableOrdinal,
    std::size_t rootScopeOrdinal)
{
    for (std::size_t index = cleanupOrdinals.size(); index > 0; --index)
    {
        recordNormalCleanupContinuation(
            cleanupOrdinals[index - 1], callable.syntax->first(), callableOrdinal, rootScopeOrdinal);
    }
}

void ProjectMethodBodyModel::analyzeCallable(
    const ProjectCallableAnalysisContext& context,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!callableCanBeAnalyzed(context)) return;
    const std::size_t callableOrdinal = context.callableOrdinal();
    const ClassMemberSymbol& callable = *methodSymbol(callableOrdinal);
    analyzedCallableMembers_.insert(callableOrdinal);
    resetCallableScratch(callable);
    const MethodScopeKind rootKind = callable.kind == ClassMemberKind::Constructor
        ? MethodScopeKind::Constructor
        : callable.kind == ClassMemberKind::Destructor
            ? MethodScopeKind::Destructor : MethodScopeKind::Method;
    const std::size_t rootScopeOrdinal = createScope(rootKind, callableOrdinal, std::nullopt);
    seedParameters(callableOrdinal, rootScopeOrdinal, diagnostics);
    const std::vector<std::size_t> cleanupOrdinals =
        createOwnedParameterCleanups(callable, callableOrdinal, rootScopeOrdinal);
    analyzeConstructorInitializer(callable, callableOrdinal, rootScopeOrdinal, diagnostics);
    analyzeBlock(
        callable.syntax->first(), callableOrdinal, rootScopeOrdinal, false, diagnostics);
    // NS304-T05-F05: a pinned local cannot cross a suspension — its fixed-address identity is not
    // preserved across the poll return (the local moves to the frame and back). A borrowed local
    // IS allowed when its source is re-localizable to a frame slot; the MIR drags the borrow's
    // source into the frame and rejects a borrow whose source is a parameter/field (no slot).
    if (callable.effects.contains(sema::Effect::Async) && !awaitExpressions_.empty())
    {
        for (const MethodLocalSymbol& local : locals())
        {
            if (local.isPinned)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidOperand,
                    diagnostics::DiagnosticSeverity::Error,
                    local.range,
                    "async function cannot hold a pinned local across a suspension "
                    "(NS304-T05-F05)");
            }
        }
    }
    finalizeRequiredConstructorAssignments(callable, callableOrdinal);
    closeOwnedParameterCleanups(
        cleanupOrdinals, callable, callableOrdinal, rootScopeOrdinal);
    activeCleanupRegions_.clear();
}

std::unique_ptr<ProjectMethodBodyModel> ProjectMethodBodyModel::createCallableShard(
    const ProjectCallableAnalysisContext& context) const
{
    auto shard = std::make_unique<ProjectMethodBodyModel>();
    shard->nameLookup_ = &context.nameLookup();
    shard->memberModel_ = &context.memberModel();
    shard->namespaceFunctionModel_ = &context.namespaceFunctionModel();
    shard->inheritanceModel_ = &context.inheritanceModel();
    shard->interfaceModel_ = &context.interfaceModel();
    shard->genericModel_ = context.genericModel();
    shard->genericConstraintModel_ = context.genericConstraintModel();
    shard->coreExceptionTypeOrdinal_ = coreExceptionTypeOrdinal_;
    shard->coreDisposableTypeOrdinal_ = coreDisposableTypeOrdinal_;
    shard->coreStringTypeOrdinal_ = coreStringTypeOrdinal_;
    shard->coreStringViewTypeKey_ = coreStringViewTypeKey_;
    shard->coreThreadStartTypeKey_ = coreThreadStartTypeKey_;
    shard->coreMemoryOrderTypeKey_ = coreMemoryOrderTypeKey_;
    shard->coreReadOnlyByteSpanTypeKey_ = coreReadOnlyByteSpanTypeKey_;
    shard->namespaceCallableBase_ = namespaceCallableBase_;
    shard->namespaceCallableAdaptersView_ = namespaceCallableAdapters();
    shard->isolatedCallableOrdinal_ = context.callableOrdinal();
    if (context.callableOrdinal() < parameterEscapeSummaries_.size())
    {
        shard->isolatedParameterEscapeSummary_ =
            parameterEscapeSummaries_[context.callableOrdinal()];
    }
    return shard;
}

ProjectCallableAnalysisResult ProjectMethodBodyModel::analyzeCallableIsolated(
    const ProjectCallableAnalysisContext& context) const
{
    const ClassMemberSymbol* callable = methodSymbol(context.callableOrdinal());
    std::string traceDetail = "wave=" + std::to_string(context.waveOrdinal()) +
        "|ordinal=" + std::to_string(context.callableOrdinal());
    if (callable != nullptr)
    {
        traceDetail += "|name=" + callable->name + "|signature=" + callable->signatureKey +
            "|source=" + callable->sourcePath;
    }
    writeMethodBodyTrace("CALLABLE-BEGIN", traceDetail);
    std::unique_ptr<ProjectMethodBodyModel> shard = createCallableShard(context);
    diagnostics::DiagnosticEngine localDiagnostics(
        std::numeric_limits<std::size_t>::max());
    shard->analyzeCallable(context, localDiagnostics);
    ProjectCallableAnalysisResult result(
        context.callableOrdinal(), context.waveOrdinal(),
        std::move(shard), std::move(localDiagnostics));
    collectCallableDiscoveries(result);
    result.normalizeDiscoveredCallables();
    writeMethodBodyTrace("CALLABLE-END", traceDetail);
    return result;
}

void ProjectMethodBodyModel::collectCallableDiscoveries(
    ProjectCallableAnalysisResult& result)
{
    const ProjectMethodBodyModel& shard = result.shard();
    for (const MethodCallBinding& call : shard.calls_)
    {
        if (call.resolution == MethodCallResolutionKind::Resolved &&
            call.selectedMemberOrdinal.has_value())
            result.addDiscoveredCallable(*call.selectedMemberOrdinal);
    }
    for (const MethodDelegateBinding& binding : shard.delegateBindings_)
    {
        if (binding.kind == MethodDelegateOperationKind::Create &&
            binding.selectedMemberOrdinal.has_value())
            result.addDiscoveredCallable(*binding.selectedMemberOrdinal);
    }
    for (const MethodOptionalCarrierBinding& binding : shard.optionalCarrierBindings_)
    {
        result.addDiscoveredCallable(binding.hasValueMemberOrdinal);
        result.addDiscoveredCallable(binding.uncheckedValueMemberOrdinal);
    }
    for (const MethodResultPropagationBinding& binding : shard.resultPropagationBindings_)
    {
        result.addDiscoveredCallable(binding.sourceSuccessTestMemberOrdinal);
        result.addDiscoveredCallable(binding.sourceSuccessExtractorMemberOrdinal);
        result.addDiscoveredCallable(binding.sourceErrorExtractorMemberOrdinal);
        result.addDiscoveredCallable(binding.returnErrorFactoryMemberOrdinal);
        if (binding.errorConversionMemberOrdinal.has_value())
            result.addDiscoveredCallable(*binding.errorConversionMemberOrdinal);
    }
    for (const std::size_t factoryOrdinal : shard.resultFactoryDiscoveries_)
        result.addDiscoveredCallable(factoryOrdinal);
}


// C4 "language references": an `out` parameter is an OBLIGATION, not a permission -- the caller
// is allowed to pass an uninitialized variable precisely because the callee promises to fill it.
// This is the check that makes the promise mean something.
//
// PRECISION about what this does and does not prove, because the difference matters: it verifies
// that the body assigns the parameter AT LEAST ONCE somewhere. It does NOT prove that every
// return path assigns it -- that is path-sensitive definite assignment, which needs the CFG and
// is a piece of work of its own (this compiler has a CFG, but the body model runs before it).
// So `out int x` with `if (c) { x = 1; }` and no else is accepted here and would be rejected by
// a full analysis. What the check DOES catch is the mistake people actually make: declaring an
// out parameter and never writing it at all, which would hand the caller whatever bytes the
// stack slot happened to hold. Tightening this to full path coverage is a descendant item, and
// it is named as such in the round's documentation rather than left implied.
void ProjectMethodBodyModel::verifyOutParameterAssignments(
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (memberModel_ == nullptr) return;
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel_->members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Method || member.isAbstract) continue;
        if (member.syntax == nullptr || member.syntax->first() == nullptr) continue;
        if (!analyzedCallableMembers_.contains(memberOrdinal)) continue;
        for (std::size_t index = 0; index < member.parameters.size(); ++index)
        {
            if (member.parameters[index].passKind != ParameterPassKind::Out) continue;
            if (assignedOutParameters_.contains({memberOrdinal, index})) continue;
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                member.parameters[index].range,
                "out parameter '" + member.parameters[index].name +
                    "' is never assigned in the body of '" + member.name + "'");
        }
    }
}

void ProjectMethodBodyModel::verifyRequiredFieldAssignments(
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // NS304-T09-F01: every instance `required` field must be assigned on every normal exit of
    // every constructor. assignedRequiredFields_ contains only the finalized path intersection.
    // Object initializers remain outside this constructor-body check.
    if (memberModel_ == nullptr) return;
    for (std::size_t memberOrdinal = 0; memberOrdinal < memberModel_->members().size(); ++memberOrdinal)
    {
        const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
        if (member.kind != ClassMemberKind::Constructor || member.isStatic) continue;
        if (!analyzedCallableMembers_.contains(memberOrdinal)) continue;
        // A constructor that can only throw/terminate never produces an object whose required
        // fields could be observed. Normal returns and fallthrough are recorded explicitly.
        if (!constructorsWithNormalExit_.contains(memberOrdinal)) continue;
        for (std::size_t fieldOrdinal = 0; fieldOrdinal < memberModel_->members().size(); ++fieldOrdinal)
        {
            const ClassMemberSymbol& field = memberModel_->members()[fieldOrdinal];
            if (field.kind != ClassMemberKind::Field || !field.isRequired || field.isStatic) continue;
            if (field.declaringTypeOrdinal != member.declaringTypeOrdinal) continue;
            if (assignedRequiredFields_.contains({memberOrdinal, fieldOrdinal})) continue;
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                field.syntax != nullptr ? field.syntax->range() : member.syntax->range(),
                "required field '" + field.name + "' is never assigned in constructor '" +
                    member.name + "'");
        }
    }
}
}
