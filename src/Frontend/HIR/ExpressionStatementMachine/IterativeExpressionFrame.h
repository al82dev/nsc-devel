#pragma once

#include "HirExpressionStatementMachineCommon.h"
#include "IterativeExpressionKind.h"
#include "ReceiverFrameState.h"
#include "BoundCallFrameState.h"
#include "NamespaceCallFrameState.h"
#include "EventInvokeFrameState.h"
#include "DelegateInvokeFrameState.h"
#include "CoreOperandCallFrameState.h"
#include "AtomicCoreCallFrameState.h"
#include "MonitorCoreCallFrameState.h"
#include "ConsoleOutputCallFrameState.h"
#include "EnumToStringCallFrameState.h"
#include "StructAggregateCallFrameState.h"
#include "OrdinaryCallFrameState.h"
#include "AllocatorConstructionFrameState.h"
#include "OrdinaryConstructionFrameState.h"
#include "AggregateCrossFrameState.h"
#include "VariableDeclarationFrameState.h"
#include "DeconstructionDeclarationFrameState.h"
#include "InlineAssemblyFrameState.h"
#include "SwitchStatementFrameState.h"
#include "ForStatementFrameState.h"
#include "ForeachStatementFrameState.h"
#include "ExitValueStatementFrameState.h"
#include "CoroutineValueStatementFrameState.h"
#include "UsingStatementFrameState.h"
#include "TryStatementFrameState.h"

namespace nativesharp::frontend::internal
{
struct IterativeExpressionFrame final
{
    IterativeExpressionKind kind = IterativeExpressionKind::BuiltinUnary;
    const ast::AstNode* syntax = nullptr;
    bool addressContext = false;
    std::uint8_t stage = 0;
    std::unique_ptr<ProjectOopHirNode> first;
    std::unique_ptr<ProjectOopHirNode> second;
    std::unique_ptr<ProjectOopHirNode> matchNode;
    std::unique_ptr<ProjectOopHirNode> matchArm;
    std::vector<const ast::AstNode*> operandSyntaxes;
    std::vector<std::unique_ptr<ProjectOopHirNode>> operands;
    const MethodNameBinding* fieldBinding = nullptr;
    std::optional<BoundCallFrameState> boundCall;
    std::optional<NamespaceCallFrameState> namespaceCall;
    std::optional<EventInvokeFrameState> eventInvoke;
    std::optional<DelegateInvokeFrameState> delegateInvoke;
    std::optional<CoreOperandCallFrameState> coreOperandCall;
    std::optional<AtomicCoreCallFrameState> atomicCoreCall;
    std::optional<MonitorCoreCallFrameState> monitorCoreCall;
    std::optional<ConsoleOutputCallFrameState> consoleOutputCall;
    std::optional<EnumToStringCallFrameState> enumToStringCall;
    std::optional<StructAggregateCallFrameState> structAggregateCall;
    std::optional<OrdinaryCallFrameState> ordinaryCall;
    std::optional<AggregateCrossFrameState> aggregateCross;
    std::optional<VariableDeclarationFrameState> variableDeclaration;
    std::optional<DeconstructionDeclarationFrameState>
        deconstructionDeclaration;
    std::optional<InlineAssemblyFrameState> inlineAssembly;
    std::optional<SwitchStatementFrameState> switchStatement;
    std::optional<ForStatementFrameState> forStatement;
    std::optional<ForeachStatementFrameState> foreachStatement;
    std::optional<ExitValueStatementFrameState> exitValueStatement;
    std::optional<CoroutineValueStatementFrameState>
        coroutineValueStatement;
    std::optional<UsingStatementFrameState> usingStatement;
    std::optional<TryStatementFrameState> tryStatement;
    std::optional<OrdinaryConstructionFrameState> ordinaryConstruction;
    std::optional<AllocatorConstructionFrameState> allocatorConstruction;
    const MethodCallBinding* environmentExitBinding = nullptr;
    const MethodDelegateBinding* delegateCreationBinding = nullptr;
    ReceiverFrameState receiverFrame;
    const NamespaceFunctionCallBinding* extensionPropertyBinding = nullptr;
    BoundCallTarget callTarget;
    const MethodRangeSliceBinding* rangeBinding = nullptr;
    const MethodCoreBinding* core = nullptr;
    const MethodVariableArrayIndexBinding* variableArrayIndex = nullptr;
    const MethodResultPropagationBinding* resultPropagation = nullptr;
    const MethodStructuredExitBinding* structuredExit = nullptr;
    const MethodMoveBinding* move = nullptr;
    const MethodLoopExpressionBinding* loopExpressionBinding = nullptr;
    const MethodExpressionTypeBinding* operandType = nullptr;
    const MethodMatchExpressionBinding* matchBinding = nullptr;
    const MethodDeconstructionBinding* deconstructionBinding = nullptr;
    const MethodEventBinding* eventSubscriptionBinding = nullptr;
    const MethodCallBinding* propertySetterBinding = nullptr;
    const NamespaceFunctionCallBinding* extensionSetterBinding = nullptr;
    std::vector<HirTuplePatternFrame> deconstructionLeaves;
    std::unique_ptr<ProjectOopHirNode> deconstructionNode;
    std::unique_ptr<ProjectOopHirNode> setterCombinedValue;
    std::unique_ptr<ProjectOopHirNode> loopBlockNode;
    lexer::TokenKind compoundOperator = lexer::TokenKind::Invalid;
    bool compoundSetter = false;
    std::size_t namespaceFunctionOrdinal = 0U;
    std::optional<std::size_t> assignmentLocalOrdinal;
    std::optional<std::size_t> awaitMemberOrdinal;
    std::vector<std::size_t> awaitCleanupRegionOrdinals;
    std::optional<std::size_t> awaitExceptionTargetRegionOrdinal;
    std::string assignmentResultType;
    lexer::TokenKind assignmentOperation = lexer::TokenKind::Invalid;
    bool assignmentOwnsStorage = false;
    bool materializeReceiverTemporary = false;
    bool baseIsFixedArray = false;
    bool baseIsFlexibleArray = false;
    bool operandAddressContext = false;
    bool nullConditionalCall = false;
    bool flowProvenNonNull = false;
    std::size_t memberOrdinal = 0;
    std::size_t syntheticLocalOrdinal = 0;
    std::size_t armIndex = 0;
    std::size_t cleanupDepth = 0;
    bool cleanupActive = false;
    bool hasOwnedTemporary = false;
    bool statementConditionFailed = false;
    std::string statementConditionError;
    std::vector<language::InterpolationPart> interpolationParts;
    bool arraySegmented = false;
    bool arrayInferred = false;
    std::vector<bool> arraySpreadOperands;
};
}
