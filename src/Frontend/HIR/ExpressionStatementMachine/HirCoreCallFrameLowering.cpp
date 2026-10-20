#include "../HirLoweringSupport.h"
#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <optional>
#include <utility>

namespace nativesharp::frontend
{
using internal::AtomicCoreCallFrameStage;
using internal::AtomicCoreCallFrameState;
using internal::ConsoleOutputCallFrameState;
using internal::CoreOperandCallFrameState;
using internal::EnumToStringCallFrameState;
using internal::IterativeExpressionFrame;
using internal::MonitorCoreCallFrameStage;
using internal::MonitorCoreCallFrameState;
using internal::StructAggregateCallFrameState;

bool ProjectOopHirModel::initializeAssertionCoreFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
{
    frame.core = coreBinding(frame.syntax);
    if (frame.core == nullptr ||
        (frame.core->kind != MethodCoreOperationKind::Assert &&
         frame.core->kind != MethodCoreOperationKind::Unreachable &&
         frame.core->kind != MethodCoreOperationKind::Assume &&
         frame.core->kind != MethodCoreOperationKind::Panic))
    {
        error = "Stage 34 assertion core lost its semantic binding";
        return false;
    }
    if (frame.addressContext)
    {
        error = std::string(methodCoreOperationKindName(frame.core->kind)) +
            "() has no addressable result";
        return false;
    }
    const bool hasCondition = frame.core->kind == MethodCoreOperationKind::Assert ||
        frame.core->kind == MethodCoreOperationKind::Assume;
    const bool isPanic = frame.core->kind == MethodCoreOperationKind::Panic;
    const std::size_t expectedArguments = hasCondition ? 1U : 0U;
    const bool invalidArity = isPanic
        ? frame.syntax->children().size() > 1U
        : frame.syntax->children().size() != expectedArguments;
    const std::string_view expectedType =
        hasCondition ? "builtin:void" : "builtin:never";
    if (invalidArity || frame.core->resultTypeKey != expectedType)
    {
        error = "T07-F06 assertion binding lost its arity/result contract";
        return false;
    }
    const ProjectOopHirNodeKind nodeKind =
        frame.core->kind == MethodCoreOperationKind::Assert
        ? ProjectOopHirNodeKind::Assert
        : frame.core->kind == MethodCoreOperationKind::Unreachable
            ? ProjectOopHirNodeKind::Unreachable
            : frame.core->kind == MethodCoreOperationKind::Assume
                ? ProjectOopHirNodeKind::Assume
                : ProjectOopHirNodeKind::Panic;
    frame.first = makeNode(nodeKind, frame.syntax, std::string(expectedType));
    if (isPanic)
    {
        frame.first->cleanupContinuationKind =
            MethodCleanupContinuationKind::Exception;
        for (std::size_t index = activeHirCleanupRegions_.size(); index > 0U; --index)
        {
            frame.first->cleanupRegionOrdinals.push_back(
                activeHirCleanupRegions_[index - 1U]);
        }
    }
    return true;
}

bool ProjectOopHirModel::initializeIteratorCoreCallFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
{
    frame.core = coreBinding(frame.syntax);
    if (frame.core == nullptr ||
        (frame.core->kind != MethodCoreOperationKind::IteratorMoveNext &&
         frame.core->kind != MethodCoreOperationKind::IteratorGetEnumerator &&
         frame.core->kind != MethodCoreOperationKind::IteratorDrop &&
         frame.core->kind != MethodCoreOperationKind::IteratorMoveNextAsync &&
         frame.core->kind != MethodCoreOperationKind::IteratorCancel))
    {
        error = "Stage 34 iterator core call lost its semantic binding";
        return false;
    }
    if (frame.addressContext || !frame.syntax->children().empty() ||
        frame.syntax->first() == nullptr || frame.syntax->first()->first() == nullptr)
    {
        error = std::string(methodCoreOperationKindName(frame.core->kind)) +
            "() has an invalid call shape";
        return false;
    }
    const ProjectOopHirNodeKind nodeKind =
        frame.core->kind == MethodCoreOperationKind::IteratorMoveNext
        ? ProjectOopHirNodeKind::IteratorMoveNext
        : frame.core->kind == MethodCoreOperationKind::IteratorDrop
            ? ProjectOopHirNodeKind::IteratorDrop
            : frame.core->kind == MethodCoreOperationKind::IteratorMoveNextAsync
                ? ProjectOopHirNodeKind::IteratorMoveNextAsync
                : frame.core->kind == MethodCoreOperationKind::IteratorCancel
                    ? ProjectOopHirNodeKind::IteratorCancel
                    : ProjectOopHirNodeKind::IteratorGetEnumerator;
    frame.first = makeNode(nodeKind, frame.syntax, frame.core->resultTypeKey);
    frame.first->elementTypeKey = frame.core->elementTypeKey;
    frame.operandSyntaxes.push_back(frame.syntax->first()->first());
    frame.operandAddressContext =
        frame.core->kind != MethodCoreOperationKind::IteratorGetEnumerator;
    return true;
}

bool ProjectOopHirModel::initializeCoreOperandCallFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
{
    frame.core = coreBinding(frame.syntax);
    CoreOperandCallFrameState call;
    if (frame.core->kind == MethodCoreOperationKind::StringViewFromString)
    {
        if (frame.addressContext)
        {
            error = "a StringView rest capture is a value and is not addressable";
            return false;
        }
        call.node = makeNode(
            ProjectOopHirNodeKind::StringViewFromString,
            frame.syntax,
            frame.core->resultTypeKey);
        call.node->elementTypeKey = frame.core->elementTypeKey;
        call.operandSyntaxes.push_back(frame.syntax->first()->first());
        call.operandAddressContexts.push_back(false);
        for (const ast::AstNode* child : frame.syntax->children())
        {
            call.operandSyntaxes.push_back(child);
            call.operandAddressContexts.push_back(false);
        }
    }
    else if (frame.core->kind == MethodCoreOperationKind::SpanFromArray ||
        frame.core->kind == MethodCoreOperationKind::ReadOnlySpanFromArray ||
        frame.core->kind == MethodCoreOperationKind::SpanFromFixedArray ||
        frame.core->kind == MethodCoreOperationKind::ReadOnlySpanFromFixedArray)
    {
        if (!initializeSpanCoreOperandCallFrame(frame, call, error)) return false;
    }
    else if (!initializeStringCoreOperandCallFrame(frame, call, error))
    {
        return false;
    }
    frame.coreOperandCall.emplace(std::move(call));
    return true;
}

bool ProjectOopHirModel::initializeSpanCoreOperandCallFrame(
    IterativeExpressionFrame& frame,
    CoreOperandCallFrameState& call,
    std::string& error)
{
    if (frame.addressContext)
    {
        error = "a Span/ReadOnlySpan rest capture is a value and is not addressable";
        return false;
    }
    const ProjectOopHirNodeKind nodeKind =
        frame.core->kind == MethodCoreOperationKind::SpanFromArray
        ? ProjectOopHirNodeKind::SpanFromArray
        : frame.core->kind == MethodCoreOperationKind::ReadOnlySpanFromArray
            ? ProjectOopHirNodeKind::ReadOnlySpanFromArray
            : frame.core->kind == MethodCoreOperationKind::SpanFromFixedArray
                ? ProjectOopHirNodeKind::SpanFromFixedArray
                : ProjectOopHirNodeKind::ReadOnlySpanFromFixedArray;
    call.node = makeNode(nodeKind, frame.syntax, frame.core->resultTypeKey);
    call.node->elementTypeKey = frame.core->elementTypeKey;
    call.operandSyntaxes.push_back(frame.syntax->first()->first());
    call.operandAddressContexts.push_back(
        frame.core->kind == MethodCoreOperationKind::SpanFromFixedArray ||
        frame.core->kind == MethodCoreOperationKind::ReadOnlySpanFromFixedArray);
    for (const ast::AstNode* child : frame.syntax->children())
    {
        call.operandSyntaxes.push_back(child);
        call.operandAddressContexts.push_back(false);
    }
    return true;
}

bool ProjectOopHirModel::initializeStringCoreOperandCallFrame(
    IterativeExpressionFrame& frame,
    CoreOperandCallFrameState& call,
    std::string& error)
{
    const std::optional<HirCoreOperandCallPlan> plan =
        hirCoreOperandCallPlan(frame.core->kind);
    if (!plan.has_value())
    {
        error = "Stage 34 core operand call escaped iterative classification";
        return false;
    }
    if (frame.addressContext)
    {
        error = "Stage 51 NativeSharp.Core.String result is not addressable";
        return false;
    }
    call.binding = callBinding(frame.syntax);
    if (call.binding == nullptr ||
        !call.binding->selectedMemberOrdinal.has_value())
    {
        error = "Stage 51 string intrinsic requires a resolved Core.String call";
        return false;
    }
    if (call.binding->forwardedReceiver)
    {
        if (call.binding->receiverSyntax == nullptr)
        {
            error = "Stage 51 forwarded string call has no receiver";
            return false;
        }
        call.operandSyntaxes.push_back(call.binding->receiverSyntax);
        call.operandAddressContexts.push_back(false);
    }
    for (const ast::AstNode* child : frame.syntax->children())
    {
        call.operandSyntaxes.push_back(child);
        call.operandAddressContexts.push_back(false);
    }
    if (call.operandSyntaxes.size() != plan->operandCount)
    {
        error = "Stage 51 string intrinsic received an invalid operand count";
        return false;
    }
    call.node = makeNode(plan->hirKind, frame.syntax, frame.core->resultTypeKey);
    call.appliesConversions = true;
    return true;
}

bool ProjectOopHirModel::nextCoreOperandCallChild(
    const CoreOperandCallFrameState& call,
    const ast::AstNode*& syntax,
    bool& addressContext)
{
    if (call.nextOperand >= call.operandSyntaxes.size()) return false;
    syntax = call.operandSyntaxes[call.nextOperand];
    addressContext = call.operandAddressContexts[call.nextOperand];
    return true;
}

void ProjectOopHirModel::acceptCoreOperandCallChild(
    IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode> child)
{
    CoreOperandCallFrameState& call = *frame.coreOperandCall;
    const std::size_t index = call.nextOperand++;
    if (call.appliesConversions &&
        index < call.binding->selectedConversions.size() &&
        call.binding->selectedConversions[index].kind != ProjectConversionKind::Identity)
    {
        child = wrapImplicitConversion(
            std::move(child),
            call.binding->selectedConversions[index],
            call.operandSyntaxes[index]);
    }
    call.node->children.push_back(std::move(child));
}

bool ProjectOopHirModel::initializeAtomicCoreCallFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
{
    frame.core = coreBinding(frame.syntax);
    const std::optional<HirAtomicCallPlan> plan =
        hirAtomicCallPlan(frame.core->kind);
    if (!plan.has_value())
    {
        error = "Stage 34 atomic core call escaped iterative classification";
        return false;
    }
    if (frame.addressContext)
    {
        error = "Stage 54.2 Atomic operation result is not addressable";
        return false;
    }
    AtomicCoreCallFrameState call;
    call.binding = callBinding(frame.syntax);
    if (call.binding == nullptr)
    {
        error = "Stage 54.2 Atomic core operation requires a resolved call";
        return false;
    }
    call.plan = *plan;
    call.explicitLocation = !frame.core->implicitFieldMemberOrdinal.has_value();
    call.argumentOffset = call.explicitLocation ? 1U : 0U;
    if (call.explicitLocation)
    {
        if (!call.binding->staticCall || frame.syntax->children().empty())
        {
            error = "Stage 54.2 Interlocked operation requires an explicit ref location";
            return false;
        }
        call.locationSyntax = frame.syntax->children().front();
    }
    else
    {
        if (call.binding->receiverSyntax == nullptr)
        {
            error = "Stage 54.2 Atomic wrapper operation requires a resolved receiver";
            return false;
        }
        call.locationSyntax = call.binding->receiverSyntax;
    }
    frame.atomicCoreCall.emplace(std::move(call));
    return true;
}

bool ProjectOopHirModel::nextAtomicCoreCallChild(
    const IterativeExpressionFrame& frame,
    const ast::AstNode*& syntax)
{
    const AtomicCoreCallFrameState& call = *frame.atomicCoreCall;
    if (call.stage == AtomicCoreCallFrameStage::Location)
    {
        syntax = call.locationSyntax;
        return true;
    }
    if (call.stage == AtomicCoreCallFrameStage::Values &&
        call.nextValue < call.plan.valueArgumentCount)
    {
        syntax = frame.syntax->children()[call.argumentOffset + call.nextValue];
        return true;
    }
    return false;
}

bool ProjectOopHirModel::acceptAtomicCoreCallChild(
    IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode> child,
    std::string& error)
{
    AtomicCoreCallFrameState& call = *frame.atomicCoreCall;
    if (call.stage == AtomicCoreCallFrameStage::Location)
    {
        if (!call.explicitLocation)
        {
            auto location = makeNode(
                ProjectOopHirNodeKind::FieldAddress,
                frame.syntax,
                frame.core->elementTypeKey);
            location->memberOrdinal = *frame.core->implicitFieldMemberOrdinal;
            location->children.push_back(std::move(child));
            child = std::move(location);
        }
        const std::size_t expectedExplicitArguments =
            call.argumentOffset + call.plan.valueArgumentCount +
            (frame.core->atomicMemoryOrder.has_value() ? 1U : 0U) +
            (frame.core->atomicFailureMemoryOrder.has_value() ? 1U : 0U);
        if (frame.syntax->children().size() != expectedExplicitArguments)
        {
            error =
                "Stage 54.2 Atomic core operation received an invalid explicit argument count";
            return false;
        }
        call.node = makeNode(
            call.plan.hirKind, frame.syntax, frame.core->resultTypeKey);
        if (frame.core->atomicMemoryOrder.has_value())
        {
            call.node->integerValue =
                static_cast<std::uint64_t>(*frame.core->atomicMemoryOrder) + 1U;
            if (frame.core->atomicFailureMemoryOrder.has_value())
            {
                call.node->integerValue |=
                    (static_cast<std::uint64_t>(
                        *frame.core->atomicFailureMemoryOrder) + 1U) << 8U;
            }
        }
        call.node->children.push_back(std::move(child));
        call.stage = call.plan.valueArgumentCount == 0U
            ? AtomicCoreCallFrameStage::Ready
            : AtomicCoreCallFrameStage::Values;
        return true;
    }

    const std::size_t sourceIndex = call.argumentOffset + call.nextValue++;
    if (sourceIndex < call.binding->selectedConversions.size() &&
        call.binding->selectedConversions[sourceIndex].kind !=
            ProjectConversionKind::Identity)
    {
        child = wrapImplicitConversion(
            std::move(child),
            call.binding->selectedConversions[sourceIndex],
            frame.syntax->children()[sourceIndex]);
    }
    call.node->children.push_back(std::move(child));
    if (call.nextValue == call.plan.valueArgumentCount)
        call.stage = AtomicCoreCallFrameStage::Ready;
    return true;
}

bool ProjectOopHirModel::initializeMonitorCoreCallFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
{
    frame.core = coreBinding(frame.syntax);
    const std::optional<HirMonitorCallPlan> plan =
        hirMonitorCallPlan(frame.core->kind);
    if (!plan.has_value())
    {
        error = "Stage 34 monitor core call escaped iterative classification";
        return false;
    }

    MonitorCoreCallFrameState call;
    call.plan = *plan;
    if (frame.addressContext)
    {
        error = call.plan.waits
            ? "Stage 54.4 Monitor.Wait result is not addressable"
            : "Stage 54.4 Monitor operation result is not addressable";
        return false;
    }
    if (call.plan.waits)
    {
        if (!frame.core->implicitFieldMemberOrdinal.has_value() ||
            !frame.core->secondaryImplicitFieldMemberOrdinal.has_value())
        {
            error = "Stage 54.4 Monitor.Wait is missing one of its two implicit field member ordinals";
            return false;
        }
    }
    else if (!frame.core->implicitFieldMemberOrdinal.has_value())
    {
        error = "Stage 54.4 Monitor core operation is missing its implicit field member ordinal";
        return false;
    }

    call.binding = callBinding(frame.syntax);
    if (call.binding == nullptr || call.binding->receiverSyntax == nullptr)
    {
        error = call.plan.waits
            ? "Stage 54.4 Monitor.Wait requires a resolved receiver"
            : "Stage 54.4 Monitor core operation requires a resolved receiver";
        return false;
    }
    if (call.plan.waits)
    {
        if (frame.syntax->children().size() != 1U)
        {
            error = "Stage 54.4 Monitor.Wait takes exactly one explicit millisecondsTimeout argument";
            return false;
        }
        call.timeoutSyntax = frame.syntax->children().front();
    }
    else if (!frame.syntax->children().empty())
    {
        error = "Stage 54.4 Monitor.Enter/Exit/Pulse/PulseAll take no explicit arguments";
        return false;
    }
    frame.monitorCoreCall.emplace(std::move(call));
    return true;
}

bool ProjectOopHirModel::nextMonitorCoreCallChild(
    const MonitorCoreCallFrameState& call,
    const ast::AstNode*& syntax)
{
    if (call.stage == MonitorCoreCallFrameStage::Receiver)
    {
        syntax = call.binding->receiverSyntax;
        return true;
    }
    if (call.stage == MonitorCoreCallFrameStage::Timeout)
    {
        syntax = call.timeoutSyntax;
        return true;
    }
    return false;
}

void ProjectOopHirModel::acceptMonitorCoreCallChild(
    IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode> child)
{
    MonitorCoreCallFrameState& call = *frame.monitorCoreCall;
    if (call.stage == MonitorCoreCallFrameStage::Receiver)
    {
        if (call.plan.waits)
        {
            call.receiver = std::move(child);
            call.stage = MonitorCoreCallFrameStage::Timeout;
            return;
        }
        auto location = makeNode(
            ProjectOopHirNodeKind::FieldAddress,
            frame.syntax,
            frame.core->elementTypeKey);
        location->memberOrdinal = *frame.core->implicitFieldMemberOrdinal;
        location->children.push_back(std::move(child));
        call.node = makeNode(
            call.plan.hirKind, frame.syntax, frame.core->resultTypeKey);
        call.node->children.push_back(std::move(location));
        call.stage = MonitorCoreCallFrameStage::Ready;
        return;
    }

    if (!call.binding->selectedConversions.empty() &&
        call.binding->selectedConversions.front().kind !=
            ProjectConversionKind::Identity)
    {
        child = wrapImplicitConversion(
            std::move(child),
            call.binding->selectedConversions.front(),
            frame.syntax->children().front());
    }
    call.node = makeNode(
        call.plan.hirKind, frame.syntax, frame.core->resultTypeKey);
    call.node->memberOrdinal = *frame.core->implicitFieldMemberOrdinal;
    call.node->secondaryMemberOrdinal =
        *frame.core->secondaryImplicitFieldMemberOrdinal;
    call.node->children.push_back(std::move(call.receiver));
    call.node->children.push_back(std::move(child));
    call.stage = MonitorCoreCallFrameStage::Ready;
}

bool ProjectOopHirModel::initializeConsoleOutputCallFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
{
    frame.core = coreBinding(frame.syntax);
    if (frame.addressContext || frame.syntax->children().size() != 1U)
    {
        error = "SHR-022 NativeSharp.Core.Console output requires one value argument";
        return false;
    }
    ConsoleOutputCallFrameState call;
    call.binding = callBinding(frame.syntax);
    const bool appendNewline =
        frame.core->kind == MethodCoreOperationKind::ConsoleWriteLine ||
        frame.core->kind == MethodCoreOperationKind::ConsoleErrorWriteLine;
    const bool useStandardError =
        frame.core->kind == MethodCoreOperationKind::ConsoleErrorWriteLine ||
        frame.core->kind == MethodCoreOperationKind::ConsoleErrorWrite;
    call.mask = (appendNewline ? 1U : 0U) | (useStandardError ? 2U : 0U);
    frame.consoleOutputCall.emplace(std::move(call));
    return true;
}

bool ProjectOopHirModel::initializeConsoleReadLineCallFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
{
    frame.core = coreBinding(frame.syntax);
    if (frame.addressContext || frame.syntax->children().size() != 1U)
    {
        error = "NativeSharp.Core.Console.ReadLine requires one out status argument";
        return false;
    }
    return true;
}

bool ProjectOopHirModel::initializeEnumToStringCallFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
{
    frame.core = coreBinding(frame.syntax);
    if (frame.addressContext)
    {
        error = "ID 129 enum ToString() has no address";
        return false;
    }
    EnumToStringCallFrameState call;
    call.receiverSyntax = frame.syntax->first() != nullptr
        ? frame.syntax->first()->first() : nullptr;
    if (call.receiverSyntax == nullptr)
    {
        error = "ID 129 enum ToString() has no receiver";
        return false;
    }
    if (memberModel_ == nullptr || nameLookup_ == nullptr)
    {
        error = "ID 129 enum ToString() needs the member model";
        return false;
    }
    std::optional<std::size_t> enumOrdinal;
    if (frame.core->elementTypeKey.starts_with("type:"))
    {
        const std::string_view identity =
            std::string_view(frame.core->elementTypeKey).substr(5);
        for (std::size_t ordinal = 0; ordinal < nameLookup_->namedTypes().size();
             ++ordinal)
        {
            if (nameLookup_->namedTypes()[ordinal].identity.key() == identity)
            {
                enumOrdinal = ordinal;
                break;
            }
        }
    }
    if (!enumOrdinal.has_value())
    {
        error = "ID 129 enum ToString() cannot find the enum type";
        return false;
    }
    for (const ClassMemberSymbol& member : memberModel_->members())
    {
        if (member.isEnumConstant && member.declaringTypeOrdinal == *enumOrdinal)
            call.enumerators.emplace_back(member.enumConstantValue, member.name);
    }
    frame.enumToStringCall.emplace(std::move(call));
    return true;
}

bool ProjectOopHirModel::initializeStructCloneCallFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
{
    frame.core = coreBinding(frame.syntax);
    if (frame.addressContext || !frame.syntax->children().empty())
    {
        error = "NativeSharp 3.0 record Clone() received an invalid operand count";
        return false;
    }
    const ast::AstNode* receiver = frame.syntax->first() != nullptr
        ? frame.syntax->first()->first() : nullptr;
    if (receiver == nullptr)
    {
        error = "NativeSharp 3.0 record Clone() has no receiver";
        return false;
    }
    frame.operandSyntaxes.push_back(receiver);
    return true;
}

bool ProjectOopHirModel::initializeStructAggregateCallFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
{
    frame.core = coreBinding(frame.syntax);
    StructAggregateCallFrameState call;
    call.equals = frame.core->kind == MethodCoreOperationKind::StructEquals;
    if (frame.addressContext ||
        frame.syntax->children().size() != (call.equals ? 1U : 0U))
    {
        error = "C8 struct root member received an invalid operand count";
        return false;
    }
    call.receiverSyntax = frame.syntax->first() != nullptr
        ? frame.syntax->first()->first() : nullptr;
    if (call.receiverSyntax == nullptr)
    {
        error = "C8 struct root member has no receiver";
        return false;
    }
    call.node = makeNode(
        call.equals ? ProjectOopHirNodeKind::AggregateEquals
                    : ProjectOopHirNodeKind::AggregateHash,
        frame.syntax,
        frame.core->resultTypeKey);
    call.node->elementTypeKey = frame.core->elementTypeKey;
    frame.structAggregateCall.emplace(std::move(call));
    return true;
}

bool ProjectOopHirModel::initializeEnvironmentExitCallFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
{
    frame.core = coreBinding(frame.syntax);
    if (frame.addressContext || frame.syntax->children().size() != 1U)
    {
        error = "Stage 53.2 NativeSharp.Core.Environment.Exit requires one int argument";
        return false;
    }
    frame.environmentExitBinding = callBinding(frame.syntax);
    return true;
}
}
