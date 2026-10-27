#include "../HirLoweringSupport.h"
#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectMetaprogrammingModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
using internal::IterativeExpressionKind;
using internal::BoundCallFrameStage;
using internal::ReceiverFrameState;
using internal::BoundCallFrameState;
using internal::NamespaceCallFrameStage;
using internal::NamespaceCallFrameState;
using internal::EventInvokeFrameStage;
using internal::EventInvokeFrameState;
using internal::DelegateInvokeFrameStage;
using internal::DelegateInvokeFrameState;
using internal::CoreOperandCallFrameState;
using internal::AtomicCoreCallFrameStage;
using internal::AtomicCoreCallFrameState;
using internal::MonitorCoreCallFrameStage;
using internal::MonitorCoreCallFrameState;
using internal::ConsoleOutputCallFrameState;
using internal::EnumToStringCallFrameState;
using internal::StructAggregateCallFrameStage;
using internal::StructAggregateCallFrameState;
using internal::OrdinaryCallFrameStage;
using internal::OrdinaryCallFrameState;
using internal::AllocatorConstructionFrameStage;
using internal::AllocatorConstructionFrameState;
using internal::OrdinaryConstructionFrameStage;
using internal::OrdinaryConstructionFrameState;
using internal::AggregateCrossSlot;
using internal::AggregateCrossStructuralFrame;
using internal::AggregateCrossFramePhase;
using internal::AggregateCrossFrameState;
using internal::VariableDeclarationFrameMode;
using internal::VariableDeclarationFrameState;
using internal::DeconstructionDeclarationFrameStage;
using internal::DeconstructionDeclarationFrameState;
using internal::InlineAssemblyFrameState;
using internal::SwitchStatementFrameState;
using internal::ForStatementFrameState;
using internal::ForeachStatementFrameState;
using internal::ExitValueStatementFrameState;
using internal::CoroutineValueStatementFrameState;
using internal::UsingStatementFrameState;
using internal::TryStatementFrameState;
using internal::IterativeExpressionFrame;
using internal::IterativeInputKind;
using internal::LoopBlockAdvance;

std::optional<LoopBlockAdvance>
ProjectOopHirModel::initializeScheduledValueFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    if (currentKind == IterativeExpressionKind::CoreIndex)
        return initializeScheduledCoreIndexFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::CoreBinary)
        return initializeScheduledCoreBinaryFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::RawIndex)
        return initializeScheduledRawIndexFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::ResultPropagation)
        return initializeScheduledResultPropagationFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::Destruction)
        return initializeScheduledDestructionFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::StackAllocation)
        return initializeScheduledStackAllocationFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::MoveProjection)
        return initializeScheduledMoveProjectionFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    else if (currentKind == IterativeExpressionKind::Match)
        return initializeScheduledMatchFrame(
            frame, currentKind, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, lowered, error,
                rootCleanupDepth, rootExceptionHandlerDepth);
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledCoreIndexFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    frame.core = coreBinding(current);
    if (frame.core->kind == MethodCoreOperationKind::NullConditionalArrayElement)
    {
        frame.operandSyntaxes.push_back(!current->children().empty()
            ? current->children().front() : current->second());
    }
    else if (!current->children().empty())
    {
        frame.operandSyntaxes.assign(
            current->children().begin(), current->children().end());
    }
    else
    {
        frame.operandSyntaxes.push_back(current->second());
    }
    frame.operands.reserve(frame.operandSyntaxes.size());

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledCoreBinaryFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    frame.core = coreBinding(current);

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledRawIndexFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    frame.core = coreBinding(current);
    frame.variableArrayIndex = methodBodyModel_ != nullptr
        ? methodBodyModel_->variableArrayIndexBinding(current)
        : nullptr;
    const MethodExpressionTypeBinding* baseType = expressionType(current->first());
    frame.baseIsFixedArray = baseType != nullptr &&
        ProjectMemberModel::fixedArrayElementTypeKey(baseType->typeKey).has_value();
    frame.baseIsFlexibleArray = baseType != nullptr &&
        ProjectMemberModel::flexibleArrayElementTypeKey(baseType->typeKey).has_value();
    if (!current->children().empty())
    {
        frame.operandSyntaxes.assign(
            current->children().begin(), current->children().end());
    }
    else
    {
        frame.operandSyntaxes.push_back(current->second());
    }
    frame.operands.reserve(frame.operandSyntaxes.size());

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledResultPropagationFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    frame.resultPropagation = resultPropagationBinding(current);
    frame.structuredExit = structuredExitBinding(current);

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledDestructionFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    frame.operandType = expressionType(current->first());
    const MethodCoreBinding* elementBinding = coreBinding(current->first());
    const bool directElement = elementBinding != nullptr &&
        (elementBinding->kind == MethodCoreOperationKind::ArrayElement ||
         elementBinding->kind == MethodCoreOperationKind::RawElement);
    if (current->kind() == ast::AstNodeKind::DeleteExpression)
    {
        const bool nativeString =
            frame.operandType->typeKey == "builtin:string";
        // EN: Freeze 3.0 C-12: a struct element that needs a drop is deleted through its address.
        // ES: Congelamiento 3.0 C-12: un elemento struct que requiere drop se borra por su direccion.
        const bool dropsValueElement = !current->flag() && elementBinding != nullptr &&
            elementBinding->kind == MethodCoreOperationKind::ArrayElement &&
            !isReferenceType(frame.operandType->typeKey) && !nativeString &&
            !frame.operandType->typeKey.ends_with("*") && memberModel_ != nullptr &&
            memberModel_->typeRequiresValueDrop(frame.operandType->typeKey);
        // EN: Freeze 3.0 (owned arrays): `delete[]` of an owned array field addresses the field,
        //     so the slot is cleared and the container's destruction entry finds it empty.
        // ES: Congelamiento 3.0 (arreglos owned): `delete[]` de un campo de arreglo owned direcciona
        //     el campo, asi el slot queda vacio y la entrada de destruccion del contenedor no lo libera.
        const bool clearsOwnedField = current->flag() && methodBodyModel_ != nullptr &&
            methodBodyModel_->arrayDeleteClearsOwnedField(current);
        // EN: Freeze 3.0 (item 121): `delete p` of an owned parameter addresses it, so it is cleared.
        // ES: Congelamiento 3.0 (elemento 121): `delete p` de un parametro owned lo direcciona y lo vacia.
        const bool clearsOwnedParameter = !current->flag() && methodBodyModel_ != nullptr &&
            methodBodyModel_->deleteClearsOwnedParameter(current);
        frame.operandAddressContext = current->flag()
            ? directElement || clearsOwnedField
            : (directElement &&
                (isReferenceType(frame.operandType->typeKey) || nativeString)) || dropsValueElement ||
                clearsOwnedParameter;
    }

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledStackAllocationFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    frame.core = coreBinding(current);

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledMoveProjectionFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    frame.move = moveBinding(current);

    return LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::initializeScheduledMatchFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] IterativeExpressionKind currentKind,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    [[maybe_unused]] std::size_t rootCleanupDepth,
    [[maybe_unused]] std::size_t rootExceptionHandlerDepth)
{
    frame.matchBinding = methodBodyModel_ != nullptr
        ? methodBodyModel_->matchExpressionBinding(current) : nullptr;
    if (frame.matchBinding == nullptr)
    {
        error = "T03-F03 match expression has no semantic binding";
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    if (currentAddressContext && !frame.matchBinding->referenceResult)
    {
        error = "a value match expression does not produce addressable storage";
        rollbackMatchCleanups(
        frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return LoopBlockAdvance::Error;
    }
    const MethodExpressionTypeBinding* matchType = expressionType(current);
    const std::string resultType = matchType != nullptr
        ? matchType->typeKey : "<error>";
    frame.matchNode = makeNode(
        ProjectOopHirNodeKind::MatchExpression, current, resultType);
    frame.matchNode->referenceResult = frame.matchBinding->referenceResult;
    frame.matchNode->governingReference = frame.matchBinding->governingReference;
    frame.matchNode->secondaryMemberOrdinal =
        frame.matchBinding->switchTagMemberOrdinal;
    frame.matchNode->booleanValue =
        frame.matchBinding->switchTagMemberOrdinal.has_value();

    return LoopBlockAdvance::Complete;
}

}
