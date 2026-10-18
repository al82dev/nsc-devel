#include "HirLoweringSupport.h"

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
namespace
{
bool isComparison(lexer::TokenKind kind) noexcept
{
    return kind == lexer::TokenKind::EqualEqual || kind == lexer::TokenKind::BangEqual ||
        kind == lexer::TokenKind::Less || kind == lexer::TokenKind::LessEqual ||
        kind == lexer::TokenKind::Greater || kind == lexer::TokenKind::GreaterEqual;
}

std::unique_ptr<ProjectOopHirNode> buildInterpolationHandlerExpression(
    const ast::AstNode* expression,
    const MethodCoreBinding& core,
    std::size_t tempOrdinal,
    const std::vector<language::InterpolationPart>& parts,
    std::vector<std::unique_ptr<ProjectOopHirNode>> holes,
    std::string& error)
{
    const std::string& handlerType = core.resultTypeKey;
    auto block = makeNode(ProjectOopHirNodeKind::BlockExpression, expression, handlerType);
    auto declaration = makeNode(
        ProjectOopHirNodeKind::LocalDeclaration, expression, handlerType);
    declaration->localOrdinal = tempOrdinal;
    declaration->stringValue = "handler";
    declaration->children.push_back(makeNode(
        ProjectOopHirNodeKind::DefaultValue, expression, handlerType));
    block->children.push_back(std::move(declaration));

    std::size_t holeIndex = 0;
    for (const language::InterpolationPart& part : parts)
    {
        const bool isHole = part.kind == language::InterpolationPartKind::Hole;
        const std::size_t builderOrdinal =
            isHole ? core.builderMemberOrdinals[1] : core.builderMemberOrdinals[0];
        std::unique_ptr<ProjectOopHirNode> argument;
        if (isHole)
        {
            if (holeIndex >= holes.size())
            {
                error = "interpolation handler hole has no expression";
                return nullptr;
            }
            argument = std::move(holes[holeIndex++]);
        }
        else
        {
            argument = makeNode(
                ProjectOopHirNodeKind::StringLiteral, expression, "string-literal");
            argument->stringValue = part.text;
        }
        auto call = makeNode(ProjectOopHirNodeKind::InstanceCall, expression, "builtin:void");
        call->memberOrdinal = builderOrdinal;
        auto receiver = makeNode(
            ProjectOopHirNodeKind::LocalAddress, expression, handlerType + "*");
        receiver->localOrdinal = tempOrdinal;
        call->children.push_back(std::move(receiver));
        call->children.push_back(std::move(argument));
        auto statement = makeNode(
            ProjectOopHirNodeKind::ExpressionStatement, expression, "builtin:void");
        statement->children.push_back(std::move(call));
        block->children.push_back(std::move(statement));
    }

    auto finish = makeNode(ProjectOopHirNodeKind::InstanceCall, expression, handlerType);
    finish->memberOrdinal = core.builderMemberOrdinals[2];
    auto finishReceiver = makeNode(
        ProjectOopHirNodeKind::LocalAddress, expression, handlerType + "*");
    finishReceiver->localOrdinal = tempOrdinal;
    finish->children.push_back(std::move(finishReceiver));
    block->children.push_back(std::move(finish));
    return block;
}


}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExpression(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        expression, memberOrdinal, addressContext, nullptr, error);
}

// Seeds an expression-statement completion frame and returns the completed HIR
// wrapper after the child expression has been lowered.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExpressionStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerVariableDeclarationStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        nullptr, memberOrdinal, false, statement, error);
}

std::unique_ptr<ProjectOopHirNode>
ProjectOopHirModel::lowerDeconstructionDeclarationStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        nullptr, memberOrdinal, false, statement, error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerInlineAssemblyStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        nullptr, memberOrdinal, false, statement, error);
}

std::unique_ptr<ProjectOopHirNode>
ProjectOopHirModel::lowerStaticAssertStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerIfStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerLoopStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerSwitchStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerForStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerForeachStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExitValueStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    return lowerExpressionWithStatementFrame(
        statement != nullptr ? statement->first() : nullptr,
        memberOrdinal,
        false,
        statement,
        error);
}

// Executes expression lowering with an optional bottom frame that owns the
// statement continuation. Ordinary expression callers pass no such frame.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExpressionWithStatementFrame(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const ast::AstNode* statementContinuation,
    std::string& error)
{
    std::unique_ptr<ProjectOopHirNode> lowered;
    enum class IterativeExpressionKind : std::uint8_t
    {
        ExpressionStatement,
        VariableDeclaration,
        DeconstructionDeclaration,
        InlineAssemblyStatement,
        StaticAssertStatement,
        IfStatement,
        LoopStatement,
        SwitchStatement,
        ForStatement,
        ForeachStatement,
        ExitValueStatement,
        LoopExpression,
        LoopBlock,
        LoopBlockExpressionStatement,
        AwaitExpression,
        OverflowPolicy,
        BuiltinUnary,
        UserUnaryOperator,
        UserBinaryOperator,
        Conversion,
        UserConversion,
        BuiltinBinary,
        CoreBinary,
        Conditional,
        Match,
        NullCoalescing,
        NullCoalescingAssignment,
        DeconstructionAssignment,
        EventSubscription,
        PropertyIndexerSetter,
        NamespaceExtensionSetter,
        NullConditionalFieldAssignment,
        NullConditionalArrayAssignment,
        DirectAssignment,
        CompoundAssignment,
        StringCompoundAssignment,
        ResultPropagation,
        Destruction,
        StackAllocation,
        StorageUnary,
        MoveProjection,
        CoreMember,
        CoreIndex,
        RawIndex,
        PropertyGetter,
        ExtensionPropertyGetter,
        UserIndexerGetter,
        DelegateCreationReceiver,
        NamespaceCall,
        EventInvoke,
        DelegateInvoke,
        AssertionCore,
        IteratorCoreCall,
        CoreOperandCall,
        AtomicCoreCall,
        MonitorCoreCall,
        ConsoleOutputCall,
        ConsoleReadLineCall,
        EnumToStringCall,
        StructCloneCall,
        StructAggregateCall,
        EnvironmentExitCall,
        OrdinaryCall,
        CollectionBuilder,
        ArrayConstruction,
        AggregateCrossLowering,
        OrdinaryConstruction,
        AllocatorConstruction,
        InterpolationHandler,
        InstanceField,
        NullConditionalField,
        RangeSlice,
    };
    enum class BoundCallFrameStage : std::uint8_t
    {
        Receiver,
        Arguments,
        Ready,
    };
    struct ReceiverFrameState final
    {
        const ast::AstNode* syntax = nullptr;
        std::string_view typeKey;
        std::unique_ptr<ProjectOopHirNode> lowered;
        bool materializeTemporary = false;
    };
    struct BoundCallFrameState final
    {
        const MethodCallBinding* binding = nullptr;
        const MethodExpressionTypeBinding* typeBinding = nullptr;
        BoundCallTarget target;
        BoundCallFrameStage stage = BoundCallFrameStage::Ready;
        std::vector<const ast::AstNode*> argumentSyntaxes;
        std::vector<std::unique_ptr<ProjectOopHirNode>> arguments;
        ReceiverFrameState receiver;
        std::size_t nextArgument = 0U;
        bool nullConditional = false;
        bool hasOwnedTemporary = false;
    };
    enum class NamespaceCallFrameStage : std::uint8_t
    {
        Receiver,
        Arguments,
        Ready,
    };
    struct NamespaceCallFrameState final
    {
        const NamespaceFunctionCallBinding* binding = nullptr;
        const NamespaceFunctionSymbol* target = nullptr;
        std::unique_ptr<ProjectOopHirNode> node;
        ProjectOopHirNode* expandedPack = nullptr;
        NamespaceCallFrameStage stage = NamespaceCallFrameStage::Ready;
        std::size_t targetCallableOrdinal = 0U;
        std::size_t receiverParameterOrdinal = 0U;
        std::size_t nextArgument = 0U;
        std::size_t argumentOffset = 0U;
        bool receiverNeedsAddress = false;
        bool hasOwnedTemporary = false;
    };
    enum class EventInvokeFrameStage : std::uint8_t
    {
        Receiver,
        Arguments,
        Ready,
    };
    struct EventInvokeFrameState final
    {
        const MethodEventBinding* binding = nullptr;
        std::unique_ptr<ProjectOopHirNode> node;
        ReceiverFrameState receiver;
        EventInvokeFrameStage stage = EventInvokeFrameStage::Ready;
        std::size_t nextArgument = 0U;
    };
    enum class DelegateInvokeFrameStage : std::uint8_t
    {
        Callee,
        Arguments,
        Ready,
    };
    struct DelegateInvokeFrameState final
    {
        const MethodDelegateBinding* binding = nullptr;
        std::unique_ptr<ProjectOopHirNode> node;
        DelegateInvokeFrameStage stage = DelegateInvokeFrameStage::Ready;
        std::size_t nextArgument = 0U;
    };
    struct CoreOperandCallFrameState final
    {
        std::unique_ptr<ProjectOopHirNode> node;
        const MethodCallBinding* binding = nullptr;
        std::vector<const ast::AstNode*> operandSyntaxes;
        std::vector<bool> operandAddressContexts;
        std::size_t nextOperand = 0U;
        bool appliesConversions = false;
    };
    enum class AtomicCoreCallFrameStage : std::uint8_t
    {
        Location,
        Values,
        Ready,
    };
    struct AtomicCoreCallFrameState final
    {
        const MethodCallBinding* binding = nullptr;
        HirAtomicCallPlan plan{};
        std::unique_ptr<ProjectOopHirNode> node;
        const ast::AstNode* locationSyntax = nullptr;
        AtomicCoreCallFrameStage stage = AtomicCoreCallFrameStage::Location;
        std::size_t argumentOffset = 0U;
        std::size_t nextValue = 0U;
        bool explicitLocation = false;
    };
    enum class MonitorCoreCallFrameStage : std::uint8_t
    {
        Receiver,
        Timeout,
        Ready,
    };
    struct MonitorCoreCallFrameState final
    {
        const MethodCallBinding* binding = nullptr;
        HirMonitorCallPlan plan{};
        std::unique_ptr<ProjectOopHirNode> node;
        std::unique_ptr<ProjectOopHirNode> receiver;
        const ast::AstNode* timeoutSyntax = nullptr;
        MonitorCoreCallFrameStage stage = MonitorCoreCallFrameStage::Receiver;
    };
    struct ConsoleOutputCallFrameState final
    {
        const MethodCallBinding* binding = nullptr;
        std::uint64_t mask = 0U;
        bool hasOwnedTemporary = false;
    };
    struct EnumToStringCallFrameState final
    {
        const ast::AstNode* receiverSyntax = nullptr;
        std::vector<std::pair<std::int64_t, std::string>> enumerators;
    };
    enum class StructAggregateCallFrameStage : std::uint8_t
    {
        Receiver,
        Argument,
        Ready,
    };
    struct StructAggregateCallFrameState final
    {
        const ast::AstNode* receiverSyntax = nullptr;
        std::unique_ptr<ProjectOopHirNode> node;
        StructAggregateCallFrameStage stage = StructAggregateCallFrameStage::Receiver;
        bool equals = false;
    };
    enum class OrdinaryCallFrameStage : std::uint8_t
    {
        Receiver,
        Arguments,
        Ready,
    };
    struct OrdinaryCallFrameState final
    {
        const MethodCallBinding* binding = nullptr;
        std::unique_ptr<ProjectOopHirNode> node;
        ReceiverFrameState receiver;
        OrdinaryCallFrameStage stage = OrdinaryCallFrameStage::Ready;
        std::size_t selectedMemberOrdinal = 0U;
        std::size_t nextArgument = 0U;
        std::size_t conversionOffset = 0U;
        std::optional<std::size_t> expandedPackChildIndex;
        bool forwardedReceiver = false;
        bool hasOwnedTemporary = false;
    };
    enum class AllocatorConstructionFrameStage : std::uint8_t
    {
        Receiver,
        Arguments,
        Ready,
    };
    struct AllocatorConstructionFrameState final
    {
        const ObjectConstructionBinding* binding = nullptr;
        AllocatorConstructionFrameStage stage =
            AllocatorConstructionFrameStage::Receiver;
        std::size_t constructionOrdinal = 0U;
        std::size_t computedSize = 0U;
        std::size_t temporaryOrdinal = 0U;
        std::size_t contextOrdinal = 0U;
        std::optional<std::size_t> allocatorTypeOrdinal;
        bool allocatorIsStruct = false;
        const ast::AstNode* receiverSyntax = nullptr;
        std::vector<const ast::AstNode*> argumentSyntaxes;
        std::unique_ptr<ProjectOopHirNode> receiver;
        std::vector<std::unique_ptr<ProjectOopHirNode>> arguments;
        std::vector<std::size_t> argumentParameterOrdinals;
        std::size_t nextArgument = 0U;
    };
    enum class OrdinaryConstructionFrameStage : std::uint8_t
    {
        Storage,
        Arguments,
        FieldInitializers,
        Ready,
    };
    struct OrdinaryConstructionFrameState final
    {
        const ObjectConstructionBinding* binding = nullptr;
        std::unique_ptr<ProjectOopHirNode> node;
        OrdinaryConstructionFrameStage stage =
            OrdinaryConstructionFrameStage::Ready;
        std::size_t nextArgument = 0U;
        std::optional<std::size_t> expandedPackChildIndex;
        std::vector<std::size_t> fieldMemberOrdinals;
        std::size_t nextFieldInitializer = 0U;
        bool hasOwnedTemporary = false;
    };
    struct AggregateCrossSlot final
    {
        std::string typeKey;
        std::uint64_t offset = 0U;
        std::optional<std::uint64_t> bitOffset;
        std::optional<std::uint64_t> bitWidth;
        std::uint64_t storageSize = 0U;
        std::uint64_t storageAlignment = 1U;
        bool volatileAccess = false;
    };
    struct AggregateCrossStructuralFrame final
    {
        const ast::AstNode* syntax = nullptr;
        std::string targetTypeKey;
        std::uint64_t baseOffset = 0U;
        std::vector<AggregateCrossSlot> slots;
        const MethodAggregateInitializerBinding* binding = nullptr;
        std::size_t entryIndex = 0U;
        bool initialized = false;
    };
    enum class AggregateCrossFramePhase : std::uint8_t
    {
        Advance,
        AwaitWithSource,
        AwaitSpreadSource,
        AwaitValue,
        Ready,
    };
    struct AggregateCrossFrameState final
    {
        std::unique_ptr<ProjectOopHirNode> node;
        std::vector<AggregateCrossStructuralFrame> structuralFrames;
        AggregateCrossFramePhase phase = AggregateCrossFramePhase::Advance;
        const ast::AstNode* pendingSyntax = nullptr;
        AggregateCrossSlot pendingSlot;
        std::vector<std::size_t> pendingTargetOrdinals;
        bool pendingMoveSpread = false;
        std::optional<std::size_t> pendingMoveSourceLocalOrdinal;
        std::optional<std::size_t> pendingMoveSourceParameterOrdinal;
    };
    enum class VariableDeclarationFrameMode : std::uint8_t
    {
        NoChild,
        Initializer,
        VlaSingle,
        VlaExtents,
        Ready,
    };
    struct VariableDeclarationFrameState final
    {
        const MethodLocalSymbol* local = nullptr;
        std::unique_ptr<ProjectOopHirNode> node;
        std::unique_ptr<ProjectOopHirNode> vlaBlock;
        std::vector<std::unique_ptr<ProjectOopHirNode>> allocationCounts;
        VariableDeclarationFrameMode mode =
            VariableDeclarationFrameMode::NoChild;
        std::size_t nextExtent = 0U;
    };
    enum class DeconstructionDeclarationFrameStage : std::uint8_t
    {
        Source,
        Ready,
    };
    struct DeconstructionDeclarationFrameState final
    {
        const MethodDeconstructionBinding* binding = nullptr;
        std::unique_ptr<ProjectOopHirNode> source;
        DeconstructionDeclarationFrameStage stage =
            DeconstructionDeclarationFrameStage::Source;
        bool materializeReceiverTemporary = false;
    };
    struct InlineAssemblyFrameState final
    {
        std::unique_ptr<ProjectOopHirNode> node;
        const ast::AstNode* pendingOperand = nullptr;
        std::size_t nextRecord = 0U;
    };
    struct SwitchStatementFrameState final
    {
        const MethodSwitchBinding* binding = nullptr;
        std::unique_ptr<ProjectOopHirNode> node;
        std::unique_ptr<ProjectOopHirNode> sectionNode;
        std::unique_ptr<ProjectOopHirNode> sectionBody;
        const ast::AstNode* sectionSyntax = nullptr;
        std::size_t nextSyntaxChild = 0U;
        std::size_t nextBindingSection = 0U;
        std::size_t nextSectionChild = 0U;
    };
    struct ForStatementFrameState final
    {
        std::unique_ptr<ProjectOopHirNode> node;
    };
    struct ForeachStatementFrameState final
    {
        std::unique_ptr<ProjectOopHirNode> node;
        const ast::AstNode* chosenCandidate = nullptr;
    };
    struct ExitValueStatementFrameState final
    {
        std::unique_ptr<ProjectOopHirNode> node;
        const MethodLoopBreakBinding* loopBreakBinding = nullptr;
        bool referenceReturn = false;
        bool valueReturningCallable = false;
        std::string declaredTypeKey;
        sema::OwnershipKind declaredOwnership = sema::OwnershipKind::None;
    };
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
    enum class IterativeInputKind : std::uint8_t
    {
        Expression,
        LoopBlock,
    };
    const auto iterativeKind = [&](const ast::AstNode* syntax, bool address)
        -> std::optional<IterativeExpressionKind>
    {
        if (syntax == nullptr) return std::nullopt;
        const MethodDelegateBinding* delegateCreation = delegateBinding(syntax);
        const bool directOwnedReceiver = delegateCreation != nullptr &&
            delegateCreation->ownsReceiver &&
            (delegateCreation->receiverSourceLocalOrdinal.has_value() ||
             delegateCreation->receiverSourceParameterOrdinal.has_value());
        if (delegateCreation != nullptr &&
            delegateCreation->kind == MethodDelegateOperationKind::Create &&
            hasValidDelegateCreationTarget(*delegateCreation) &&
            !delegateCreation->staticTarget && !directOwnedReceiver &&
            delegateCreation->receiverSyntax != nullptr &&
            delegateCreation->receiverSyntax->kind() != ast::AstNodeKind::BaseExpression &&
            !delegateCreation->receiverTypeKey.empty())
        {
            return IterativeExpressionKind::DelegateCreationReceiver;
        }
        if (syntax->kind() == ast::AstNodeKind::CallExpression)
        {
            const NamespaceFunctionCallBinding* namespaceCall =
                namespaceFunctionCallBinding(syntax);
            if (namespaceCall != nullptr &&
                (namespaceCall->forwardedReceiver || !syntax->children().empty()))
            {
                return IterativeExpressionKind::NamespaceCall;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::CallExpression)
        {
            const MethodEventBinding* eventInvoke = eventBinding(syntax);
            const bool iterativeReceiver = eventInvoke != nullptr &&
                !eventInvoke->staticEvent && eventInvoke->receiverSyntax != nullptr &&
                eventInvoke->receiverSyntax->kind() != ast::AstNodeKind::BaseExpression;
            if (eventInvoke != nullptr &&
                eventInvoke->kind == MethodEventOperationKind::Invoke &&
                (iterativeReceiver || !syntax->children().empty()))
            {
                return IterativeExpressionKind::EventInvoke;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::CallExpression)
        {
            const MethodDelegateBinding* delegateInvoke = delegateBinding(syntax);
            const bool lowersCallee = delegateInvoke != nullptr &&
                (delegateInvoke->directLambdaName.empty() ||
                 delegateInvoke->directLambdaUsesEnvironment);
            if (delegateInvoke != nullptr &&
                delegateInvoke->kind == MethodDelegateOperationKind::Invoke &&
                (lowersCallee || !syntax->children().empty()))
            {
                return IterativeExpressionKind::DelegateInvoke;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::CallExpression)
        {
            const MethodCoreBinding* assertionCore = coreBinding(syntax);
            const bool recognized = assertionCore != nullptr &&
                (assertionCore->kind == MethodCoreOperationKind::Assert ||
                 assertionCore->kind == MethodCoreOperationKind::Unreachable ||
                 assertionCore->kind == MethodCoreOperationKind::Assume ||
                 assertionCore->kind == MethodCoreOperationKind::Panic);
            const bool requiresOperandFrame = recognized &&
                (assertionCore->kind == MethodCoreOperationKind::Assert ||
                 assertionCore->kind == MethodCoreOperationKind::Assume ||
                 !syntax->children().empty());
            if (requiresOperandFrame) return IterativeExpressionKind::AssertionCore;
        }
        if (syntax->kind() == ast::AstNodeKind::CallExpression)
        {
            const MethodCoreBinding* iteratorCore = coreBinding(syntax);
            if (iteratorCore != nullptr &&
                (iteratorCore->kind == MethodCoreOperationKind::IteratorMoveNext ||
                 iteratorCore->kind == MethodCoreOperationKind::IteratorGetEnumerator ||
                 iteratorCore->kind == MethodCoreOperationKind::IteratorDrop ||
                 iteratorCore->kind == MethodCoreOperationKind::IteratorMoveNextAsync ||
                 iteratorCore->kind == MethodCoreOperationKind::IteratorCancel))
            {
                return IterativeExpressionKind::IteratorCoreCall;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::CallExpression)
        {
            const MethodCoreBinding* operandCore = coreBinding(syntax);
            if (operandCore != nullptr)
            {
                const bool viewOrSpan =
                    operandCore->kind == MethodCoreOperationKind::StringViewFromString ||
                    operandCore->kind == MethodCoreOperationKind::SpanFromArray ||
                    operandCore->kind == MethodCoreOperationKind::ReadOnlySpanFromArray ||
                    operandCore->kind == MethodCoreOperationKind::SpanFromFixedArray ||
                    operandCore->kind == MethodCoreOperationKind::ReadOnlySpanFromFixedArray;
                if (viewOrSpan) return IterativeExpressionKind::CoreOperandCall;

                const std::optional<HirCoreOperandCallPlan> plan =
                    hirCoreOperandCallPlan(operandCore->kind);
                const MethodCallBinding* binding = callBinding(syntax);
                const bool directZeroOperandLeaf = plan.has_value() &&
                    plan->operandCount == 0U && !address && syntax->children().empty() &&
                    binding != nullptr && binding->selectedMemberOrdinal.has_value() &&
                    !binding->forwardedReceiver;
                if (plan.has_value() && !directZeroOperandLeaf)
                    return IterativeExpressionKind::CoreOperandCall;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::CallExpression)
        {
            const MethodCoreBinding* atomicCore = coreBinding(syntax);
            if (atomicCore != nullptr && hirAtomicCallPlan(atomicCore->kind).has_value())
                return IterativeExpressionKind::AtomicCoreCall;
        }
        if (syntax->kind() == ast::AstNodeKind::CallExpression)
        {
            const MethodCoreBinding* monitorCore = coreBinding(syntax);
            if (monitorCore != nullptr && hirMonitorCallPlan(monitorCore->kind).has_value())
                return IterativeExpressionKind::MonitorCoreCall;
        }
        if (syntax->kind() == ast::AstNodeKind::CallExpression)
        {
            const MethodCoreBinding* core = coreBinding(syntax);
            if (core != nullptr)
            {
                if (core->kind == MethodCoreOperationKind::ConsoleWriteLine ||
                    core->kind == MethodCoreOperationKind::ConsoleWrite ||
                    core->kind == MethodCoreOperationKind::ConsoleErrorWriteLine ||
                    core->kind == MethodCoreOperationKind::ConsoleErrorWrite)
                    return IterativeExpressionKind::ConsoleOutputCall;
                if (core->kind == MethodCoreOperationKind::ConsoleReadLine)
                    return IterativeExpressionKind::ConsoleReadLineCall;
                if (core->kind == MethodCoreOperationKind::EnumToString)
                    return IterativeExpressionKind::EnumToStringCall;
                if (core->kind == MethodCoreOperationKind::StructClone)
                    return IterativeExpressionKind::StructCloneCall;
                if (core->kind == MethodCoreOperationKind::StructEquals ||
                    core->kind == MethodCoreOperationKind::StructHashCode)
                    return IterativeExpressionKind::StructAggregateCall;
                if (core->kind == MethodCoreOperationKind::EnvironmentExit)
                    return IterativeExpressionKind::EnvironmentExitCall;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::CallExpression &&
            coreBinding(syntax) == nullptr &&
            namespaceFunctionCallBinding(syntax) == nullptr &&
            eventBinding(syntax) == nullptr && delegateBinding(syntax) == nullptr)
        {
            const MethodCallBinding* binding = callBinding(syntax);
            const bool explicitReceiver = binding != nullptr &&
                !binding->staticCall && binding->receiverSyntax != nullptr &&
                binding->receiverSyntax->kind() != ast::AstNodeKind::BaseExpression;
            if (binding != nullptr && binding->selectedMemberOrdinal.has_value() &&
                (binding->forwardedReceiver || explicitReceiver ||
                 !syntax->children().empty()))
            {
                return IterativeExpressionKind::OrdinaryCall;
            }
        }
        if (!address &&
            syntax->kind() == ast::AstNodeKind::PlacementConstructionExpression)
        {
            const ObjectConstructionBinding* binding = constructionBinding(syntax);
            if (binding != nullptr &&
                (binding->resolution == ConstructionResolutionKind::ResolvedExplicit ||
                 binding->resolution == ConstructionResolutionKind::ResolvedImplicitDefault) &&
                binding->allocatorSelection &&
                binding->allocatorAllocateMemberOrdinal.has_value())
            {
                return IterativeExpressionKind::AllocatorConstruction;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::ObjectCreationExpression ||
            syntax->kind() == ast::AstNodeKind::PlacementConstructionExpression)
        {
            const ObjectConstructionBinding* binding = constructionBinding(syntax);
            const bool resolved = binding != nullptr &&
                (binding->resolution == ConstructionResolutionKind::ResolvedExplicit ||
                 binding->resolution == ConstructionResolutionKind::ResolvedImplicitDefault);
            const bool completeAllocator = resolved && binding->allocatorSelection &&
                binding->allocatorAllocateMemberOrdinal.has_value();
            if (resolved && !completeAllocator)
                return IterativeExpressionKind::OrdinaryConstruction;
        }
        if (!address && syntax->kind() == ast::AstNodeKind::CollectionExpression &&
            !syntax->children().empty())
        {
            const MethodCoreBinding* core = coreBinding(syntax);
            if (core != nullptr &&
                core->kind == MethodCoreOperationKind::CollectionBuilder)
            {
                return IterativeExpressionKind::CollectionBuilder;
            }
        }
        if (!address && (syntax->kind() == ast::AstNodeKind::NewExpression ||
                         syntax->kind() == ast::AstNodeKind::CollectionExpression))
        {
            const MethodCoreBinding* core = coreBinding(syntax);
            if (core != nullptr &&
                (core->kind == MethodCoreOperationKind::ArrayAllocation ||
                 core->kind == MethodCoreOperationKind::RangeArrayAllocation))
            {
                const bool hasOperand = core->kind == MethodCoreOperationKind::RangeArrayAllocation
                    ? syntax->first() != nullptr || !syntax->children().empty()
                    : syntax->auxiliaryText() == "inferred-array-initializer"
                        ? !syntax->children().empty()
                        : syntax->first() != nullptr || !syntax->children().empty();
                if (hasOperand) return IterativeExpressionKind::ArrayConstruction;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::AwaitExpression)
            return IterativeExpressionKind::AwaitExpression;
        if (syntax->kind() == ast::AstNodeKind::TupleExpression ||
            syntax->kind() == ast::AstNodeKind::AggregateInitializerExpression ||
            syntax->kind() == ast::AstNodeKind::WithExpression)
        {
            return IterativeExpressionKind::AggregateCrossLowering;
        }
        if (syntax->kind() == ast::AstNodeKind::LoopExpression)
            return IterativeExpressionKind::LoopExpression;
        if (!address && syntax->kind() == ast::AstNodeKind::InterpolatedStringExpression &&
            !syntax->children().empty())
        {
            const MethodCoreBinding* core = coreBinding(syntax);
            if (core != nullptr && core->kind == MethodCoreOperationKind::InterpolationHandler)
                return IterativeExpressionKind::InterpolationHandler;
        }
        if (syntax->kind() == ast::AstNodeKind::OverflowPolicyExpression &&
            callBinding(syntax) == nullptr && delegateBinding(syntax) == nullptr &&
            lambdaBinding(syntax) == nullptr)
        {
            return IterativeExpressionKind::OverflowPolicy;
        }
        if (syntax->kind() == ast::AstNodeKind::UnaryExpression &&
            syntax->tokenKind() != lexer::TokenKind::Amp &&
            syntax->tokenKind() != lexer::TokenKind::Star &&
            syntax->tokenKind() != lexer::TokenKind::PlusPlus &&
            syntax->tokenKind() != lexer::TokenKind::MinusMinus &&
            callBinding(syntax) == nullptr && delegateBinding(syntax) == nullptr &&
            lambdaBinding(syntax) == nullptr)
        {
            return IterativeExpressionKind::BuiltinUnary;
        }
        if (syntax->kind() == ast::AstNodeKind::TypeTestExpression ||
            syntax->kind() == ast::AstNodeKind::SafeCastExpression ||
            syntax->kind() == ast::AstNodeKind::CastExpression)
        {
            const MethodRuntimeTypeBinding* runtime = runtimeTypeBinding(syntax);
            if ((syntax->kind() == ast::AstNodeKind::TypeTestExpression ||
                 syntax->kind() == ast::AstNodeKind::SafeCastExpression) && runtime == nullptr)
            {
                return std::nullopt;
            }
            if (syntax->kind() == ast::AstNodeKind::TypeTestExpression ||
                syntax->kind() == ast::AstNodeKind::SafeCastExpression ||
                (runtime != nullptr &&
                 runtime->kind == MethodRuntimeTypeOperationKind::CheckedCast))
            {
                return IterativeExpressionKind::Conversion;
            }
            const MethodCallBinding* conversionCall = callBinding(syntax);
            const bool userDefined = conversionCall != nullptr &&
                conversionCall->resolution == MethodCallResolutionKind::Resolved &&
                conversionCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *conversionCall->selectedMemberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[*conversionCall->selectedMemberOrdinal].isUserConversion;
            return userDefined
                ? IterativeExpressionKind::UserConversion
                : IterativeExpressionKind::Conversion;
        }
        if (syntax->kind() == ast::AstNodeKind::BinaryExpression &&
            coreBinding(syntax) == nullptr && callBinding(syntax) == nullptr &&
            delegateBinding(syntax) == nullptr && lambdaBinding(syntax) == nullptr)
        {
            return IterativeExpressionKind::BuiltinBinary;
        }
        if (!address && syntax->kind() == ast::AstNodeKind::BinaryExpression &&
            syntax->first() != nullptr && syntax->second() != nullptr)
        {
            const MethodCoreBinding* core = coreBinding(syntax);
            if (core != nullptr &&
                (core->kind == MethodCoreOperationKind::StructEquals ||
                 core->kind == MethodCoreOperationKind::StringConcat ||
                 core->kind == MethodCoreOperationKind::StringEquals ||
                 core->kind == MethodCoreOperationKind::PointerArithmetic))
            {
                return IterativeExpressionKind::CoreBinary;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::BinaryExpression &&
            syntax->first() != nullptr && syntax->second() != nullptr)
        {
            const MethodCallBinding* operatorCall = callBinding(syntax);
            const bool userOperator = operatorCall != nullptr &&
                operatorCall->resolution == MethodCallResolutionKind::Resolved &&
                operatorCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                callableSymbol(*operatorCall->selectedMemberOrdinal) != nullptr &&
                callableSymbol(*operatorCall->selectedMemberOrdinal)->isUserOperator;
            if (userOperator) return IterativeExpressionKind::UserBinaryOperator;
        }
        if (syntax->kind() == ast::AstNodeKind::MatchExpression)
            return IterativeExpressionKind::Match;
        if (!address && syntax->kind() == ast::AstNodeKind::ConditionalExpression &&
            coreBinding(syntax) == nullptr && callBinding(syntax) == nullptr &&
            delegateBinding(syntax) == nullptr && lambdaBinding(syntax) == nullptr)
        {
            return IterativeExpressionKind::Conditional;
        }
        if (!address && syntax->kind() == ast::AstNodeKind::NullCoalescingExpression &&
            syntax->first() != nullptr && syntax->second() != nullptr)
        {
            return IterativeExpressionKind::NullCoalescing;
        }
        if (!address && syntax->kind() == ast::AstNodeKind::AssignmentExpression &&
            syntax->tokenKind() == lexer::TokenKind::QuestionQuestionEqual &&
            syntax->first() != nullptr && syntax->second() != nullptr &&
            expressionType(syntax->first()) != nullptr)
        {
            return IterativeExpressionKind::NullCoalescingAssignment;
        }
        if (!address && syntax->kind() == ast::AstNodeKind::AssignmentExpression &&
            syntax->tokenKind() == lexer::TokenKind::Equal && syntax->first() != nullptr &&
            syntax->first()->kind() == ast::AstNodeKind::TupleExpression &&
            syntax->second() != nullptr && methodBodyModel_ != nullptr)
        {
            const MethodDeconstructionBinding* binding =
                methodBodyModel_->deconstructionBinding(syntax);
            const bool iterativeReceiver = binding != nullptr &&
                (!binding->selectedMemberOrdinal.has_value() ||
                 (syntax->second()->kind() != ast::AstNodeKind::BaseExpression &&
                  !binding->sourceTypeKey.empty()));
            if (iterativeReceiver)
                return IterativeExpressionKind::DeconstructionAssignment;
        }
        if (!address && syntax->kind() == ast::AstNodeKind::AssignmentExpression &&
            syntax->second() != nullptr)
        {
            const MethodEventBinding* binding = eventBinding(syntax);
            if (binding != nullptr &&
                (binding->kind == MethodEventOperationKind::Add ||
                 binding->kind == MethodEventOperationKind::Remove))
            {
                return IterativeExpressionKind::EventSubscription;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::AssignmentExpression &&
            syntax->second() != nullptr)
        {
            const MethodCallBinding* setterCall = callBinding(syntax);
            if (setterCall != nullptr &&
                setterCall->resolution == MethodCallResolutionKind::Resolved &&
                setterCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *setterCall->selectedMemberOrdinal < memberModel_->members().size())
            {
                const ClassMemberSymbol& setter =
                    memberModel_->members()[*setterCall->selectedMemberOrdinal];
                if ((setter.isPropertyAccessor || setter.isIndexerAccessor) && setter.isSetter)
                    return IterativeExpressionKind::PropertyIndexerSetter;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::AssignmentExpression &&
            syntax->second() != nullptr)
        {
            const NamespaceFunctionCallBinding* setter = namespaceFunctionCallBinding(syntax);
            if (setter != nullptr &&
                setter->resolution == MethodCallResolutionKind::Resolved &&
                setter->selectedFunctionOrdinal.has_value() &&
                namespaceFunctionModel_ != nullptr &&
                *setter->selectedFunctionOrdinal < namespaceFunctionModel_->functions().size())
            {
                return IterativeExpressionKind::NamespaceExtensionSetter;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::AssignmentExpression &&
            syntax->second() != nullptr && syntax->first() != nullptr)
        {
            if (syntax->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression)
            {
                const MethodNameBinding* binding = nameBinding(syntax->first());
                if (binding != nullptr && binding->candidateMemberOrdinals.size() == 1U &&
                    binding->kind == MethodNameBindingKind::ExplicitObjectField)
                {
                    return IterativeExpressionKind::NullConditionalFieldAssignment;
                }
            }
            if (syntax->tokenKind() == lexer::TokenKind::Equal &&
                syntax->first()->kind() == ast::AstNodeKind::NullConditionalIndexExpression)
            {
                const MethodCoreBinding* binding = coreBinding(syntax->first());
                if (binding != nullptr &&
                    binding->kind == MethodCoreOperationKind::NullConditionalArrayElement)
                {
                    return IterativeExpressionKind::NullConditionalArrayAssignment;
                }
            }
        }
        if (syntax->kind() == ast::AstNodeKind::AssignmentExpression &&
            syntax->first() != nullptr && syntax->second() != nullptr &&
            syntax->tokenKind() != lexer::TokenKind::QuestionQuestionEqual &&
            syntax->first()->kind() != ast::AstNodeKind::TupleExpression &&
            syntax->first()->kind() != ast::AstNodeKind::NullConditionalMemberExpression &&
            syntax->first()->kind() != ast::AstNodeKind::NullConditionalIndexExpression &&
            eventBinding(syntax) == nullptr && callBinding(syntax) == nullptr &&
            namespaceFunctionCallBinding(syntax) == nullptr &&
            (methodBodyModel_ == nullptr || methodBodyModel_->deconstructionBinding(syntax) == nullptr))
        {
            if (syntax->tokenKind() == lexer::TokenKind::Equal)
                return IterativeExpressionKind::DirectAssignment;
            const MethodExpressionTypeBinding* assignmentType = expressionType(syntax);
            if (syntax->tokenKind() == lexer::TokenKind::PlusEqual &&
                assignmentType != nullptr && assignmentType->typeKey == "builtin:string")
            {
                return IterativeExpressionKind::StringCompoundAssignment;
            }
            return IterativeExpressionKind::CompoundAssignment;
        }
        if (!address && syntax->kind() == ast::AstNodeKind::ResultPropagationExpression)
        {
            const MethodResultPropagationBinding* binding = resultPropagationBinding(syntax);
            const MethodStructuredExitBinding* exit = structuredExitBinding(syntax);
            if (binding != nullptr && binding->operandSyntax != nullptr && exit != nullptr &&
                exit->kind == MethodStructuredExitKind::Return)
            {
                return IterativeExpressionKind::ResultPropagation;
            }
        }
        if ((syntax->kind() == ast::AstNodeKind::DeleteExpression ||
             syntax->kind() == ast::AstNodeKind::DestroyExpression) &&
            syntax->first() != nullptr)
        {
            const MethodExpressionTypeBinding* operandType = expressionType(syntax->first());
            const bool validDeleteArray = syntax->kind() != ast::AstNodeKind::DeleteExpression ||
                !syntax->flag() ||
                (operandType != nullptr &&
                 ProjectTypeConversion::isNativeArray(operandType->typeKey));
            if (operandType != nullptr && validDeleteArray)
                return IterativeExpressionKind::Destruction;
        }
        if (!address && syntax->kind() == ast::AstNodeKind::StackallocExpression &&
            syntax->first() != nullptr)
        {
            const MethodCoreBinding* core = coreBinding(syntax);
            if (core != nullptr && core->kind == MethodCoreOperationKind::StackAllocation)
                return IterativeExpressionKind::StackAllocation;
        }
        if ((syntax->kind() == ast::AstNodeKind::UnaryExpression ||
             syntax->kind() == ast::AstNodeKind::PostfixExpression) &&
            syntax->first() != nullptr)
        {
            const MethodCallBinding* operatorCall = callBinding(syntax);
            const bool userOperator = operatorCall != nullptr &&
                operatorCall->resolution == MethodCallResolutionKind::Resolved &&
                operatorCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                callableSymbol(*operatorCall->selectedMemberOrdinal) != nullptr &&
                callableSymbol(*operatorCall->selectedMemberOrdinal)->isUserOperator;
            if (userOperator)
            {
                return IterativeExpressionKind::UserUnaryOperator;
            }
            if (syntax->tokenKind() == lexer::TokenKind::PlusPlus ||
                syntax->tokenKind() == lexer::TokenKind::MinusMinus)
            {
                if (!address && expressionType(syntax->first()) != nullptr)
                    return IterativeExpressionKind::StorageUnary;
            }
            else if (syntax->tokenKind() == lexer::TokenKind::Amp && !address)
            {
                const MethodNameBinding* target = nameBinding(syntax->first());
                const bool bitField = target != nullptr &&
                    !target->candidateMemberOrdinals.empty() && memberModel_ != nullptr &&
                    target->candidateMemberOrdinals.front() < memberModel_->members().size() &&
                    memberModel_->members()[target->candidateMemberOrdinals.front()].isBitField;
                if (!bitField) return IterativeExpressionKind::StorageUnary;
            }
            else if (syntax->tokenKind() == lexer::TokenKind::Star)
            {
                return IterativeExpressionKind::StorageUnary;
            }
        }
        if (!address && syntax->kind() == ast::AstNodeKind::MoveExpression)
        {
            const MethodMoveBinding* move = moveBinding(syntax);
            const bool directProjection = move != nullptr &&
                (move->sourceIsArrayElement || move->sourceIsOwnedField) &&
                syntax->first() != nullptr;
            const bool iteratorProjection = move != nullptr && move->sourceIsIteratorCurrent &&
                syntax->first() != nullptr && syntax->first()->first() != nullptr;
            if (directProjection || iteratorProjection)
                return IterativeExpressionKind::MoveProjection;
        }
        if ((syntax->kind() == ast::AstNodeKind::IdentifierExpression ||
             syntax->kind() == ast::AstNodeKind::MemberExpression) &&
            syntax->first() != nullptr)
        {
            const MethodCoreBinding* core = coreBinding(syntax);
            if (core != nullptr &&
                (core->kind == MethodCoreOperationKind::ArrayLength ||
                 core->kind == MethodCoreOperationKind::StringLength) && !address)
            {
                return IterativeExpressionKind::CoreMember;
            }
            if (core != nullptr && core->kind == MethodCoreOperationKind::IteratorCurrent)
            {
                const bool itemByReference = core->resultTypeKey.starts_with("ref:") ||
                    core->resultTypeKey.starts_with("refmut:");
                if (!address || itemByReference)
                    return IterativeExpressionKind::CoreMember;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::IdentifierExpression ||
            syntax->kind() == ast::AstNodeKind::MemberExpression ||
            syntax->kind() == ast::AstNodeKind::NullConditionalMemberExpression)
        {
            const MethodCallBinding* getter = accessorGetterBinding(syntax);
            if (getter != nullptr && !getter->staticCall && memberModel_ != nullptr &&
                getter->selectedMemberOrdinal.has_value() &&
                *getter->selectedMemberOrdinal < memberModel_->members().size())
            {
                const ClassMemberSymbol& accessor =
                    memberModel_->members()[*getter->selectedMemberOrdinal];
                const bool nullConditional = syntax->kind() ==
                    ast::AstNodeKind::NullConditionalMemberExpression;
                const bool addressAllowed = !address ||
                    (!nullConditional && accessor.isReferenceReturn);
                if (accessor.isPropertyAccessor && !accessor.isIndexerAccessor &&
                    accessor.isGetter && addressAllowed && getter->receiverSyntax != nullptr &&
                    getter->receiverSyntax->kind() != ast::AstNodeKind::BaseExpression &&
                    !getter->receiverTypeKey.empty())
                {
                    return IterativeExpressionKind::PropertyGetter;
                }
            }
        }
        if (syntax->kind() == ast::AstNodeKind::IdentifierExpression ||
            syntax->kind() == ast::AstNodeKind::MemberExpression)
        {
            const NamespaceFunctionCallBinding* extension =
                namespaceFunctionCallBinding(syntax);
            if (extension != nullptr && extensionPropertyReceiverAddress(*extension).has_value())
                return IterativeExpressionKind::ExtensionPropertyGetter;
        }
        if (!address && syntax->kind() == ast::AstNodeKind::NullConditionalMemberExpression &&
            accessorGetterBinding(syntax) == nullptr)
        {
            const MethodNameBinding* binding = nameBinding(syntax);
            if (binding != nullptr &&
                binding->kind == MethodNameBindingKind::ExplicitObjectField &&
                binding->candidateMemberOrdinals.size() == 1U && syntax->first() != nullptr)
            {
                return IterativeExpressionKind::NullConditionalField;
            }
        }
        if (!address &&
            (syntax->kind() == ast::AstNodeKind::IndexExpression ||
             syntax->kind() == ast::AstNodeKind::NullConditionalIndexExpression))
        {
            const MethodRangeSliceBinding* range = rangeSliceBinding(syntax);
            if (range != nullptr && memberModel_ != nullptr &&
                range->lengthMemberOrdinal < memberModel_->members().size() &&
                range->sliceMemberOrdinal < memberModel_->members().size() &&
                range->receiverSyntax != nullptr &&
                range->receiverSyntax->kind() != ast::AstNodeKind::BaseExpression &&
                !range->receiverTypeKey.empty())
            {
                return IterativeExpressionKind::RangeSlice;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::IndexExpression ||
            syntax->kind() == ast::AstNodeKind::NullConditionalIndexExpression)
        {
            const MethodCoreBinding* core = coreBinding(syntax);
            const bool ordinaryElement = core != nullptr &&
                (core->kind == MethodCoreOperationKind::ArrayElement ||
                 core->kind == MethodCoreOperationKind::RangeArrayElement ||
                 core->kind == MethodCoreOperationKind::StringElement);
            const bool nullConditionalElement = core != nullptr &&
                core->kind == MethodCoreOperationKind::NullConditionalArrayElement;
            const bool validAddress =
                !(core != nullptr && core->kind == MethodCoreOperationKind::StringElement && address) &&
                !(nullConditionalElement && address);
            const ast::AstNode* firstIndex = !syntax->children().empty()
                ? syntax->children().front() : syntax->second();
            const auto validIndexSyntax = [](const ast::AstNode* index) noexcept
            {
                return index != nullptr &&
                    (index->kind() != ast::AstNodeKind::FromEndExpression ||
                     index->first() != nullptr);
            };
            const bool validIndices = syntax->children().empty()
                ? validIndexSyntax(firstIndex)
                : std::all_of(
                    syntax->children().begin(), syntax->children().end(), validIndexSyntax);
            if ((ordinaryElement || nullConditionalElement) && validAddress &&
                syntax->first() != nullptr && validIndices)
            {
                return IterativeExpressionKind::CoreIndex;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::IndexExpression ||
            syntax->kind() == ast::AstNodeKind::NullConditionalIndexExpression)
        {
            const MethodCoreBinding* core = coreBinding(syntax);
            if (core != nullptr && core->kind == MethodCoreOperationKind::RawElement)
            {
                const bool hasIndices =
                    !syntax->children().empty() || syntax->second() != nullptr;
                const bool validIndices = syntax->children().empty()
                    ? syntax->second() != nullptr
                    : std::all_of(
                        syntax->children().begin(),
                        syntax->children().end(),
                        [](const ast::AstNode* index) noexcept { return index != nullptr; });
                const MethodVariableArrayIndexBinding* variableArrayIndex =
                    methodBodyModel_ != nullptr
                        ? methodBodyModel_->variableArrayIndexBinding(syntax)
                        : nullptr;
                const std::size_t indexCount = syntax->children().empty()
                    ? (syntax->second() != nullptr ? 1U : 0U)
                    : syntax->children().size();
                const bool validVariableRank = variableArrayIndex == nullptr ||
                    indexCount == variableArrayIndex->extentLocalOrdinals.size();
                if (syntax->first() != nullptr && hasIndices && validIndices && validVariableRank)
                    return IterativeExpressionKind::RawIndex;
            }
        }
        if (syntax->kind() == ast::AstNodeKind::IndexExpression ||
            syntax->kind() == ast::AstNodeKind::NullConditionalIndexExpression)
        {
            const MethodCallBinding* getter = callBinding(syntax);
            const MethodCoreBinding* core = coreBinding(syntax);
            const bool nullConditional = core != nullptr &&
                core->kind == MethodCoreOperationKind::NullConditionalUserIndex;
            if (getter != nullptr && !getter->staticCall &&
                getter->resolution == MethodCallResolutionKind::Resolved &&
                getter->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *getter->selectedMemberOrdinal < memberModel_->members().size())
            {
                const ClassMemberSymbol& accessor =
                    memberModel_->members()[*getter->selectedMemberOrdinal];
                const bool ordinary = syntax->kind() == ast::AstNodeKind::IndexExpression &&
                    core == nullptr;
                const bool addressAllowed = !address ||
                    (!nullConditional && accessor.isReferenceReturn);
                if ((ordinary || nullConditional) && accessor.isIndexerAccessor &&
                    accessor.isGetter && addressAllowed && getter->receiverSyntax != nullptr &&
                    getter->receiverSyntax->kind() != ast::AstNodeKind::BaseExpression &&
                    !getter->receiverTypeKey.empty())
                {
                    return IterativeExpressionKind::UserIndexerGetter;
                }
            }
        }
        if (syntax->kind() == ast::AstNodeKind::MemberExpression &&
            coreBinding(syntax) == nullptr && accessorGetterBinding(syntax) == nullptr &&
            callBinding(syntax) == nullptr &&
            namespaceFunctionCallBinding(syntax) == nullptr)
        {
            const MethodLambdaValueBinding* lambdaValue = lambdaValueBinding(syntax);
            const bool activeLambdaValue = currentLambda_ != nullptr && lambdaValue != nullptr &&
                lambdaValue->lambdaOrdinal == currentLambda_->lambdaOrdinal;
            const MethodNameBinding* binding = nameBinding(syntax);
            if (!activeLambdaValue && binding != nullptr &&
                binding->kind == MethodNameBindingKind::ExplicitObjectField &&
                binding->receiverSyntax != nullptr && !binding->receiverTypeKey.empty() &&
                !binding->candidateMemberOrdinals.empty())
            {
                return IterativeExpressionKind::InstanceField;
            }
        }
        return std::nullopt;
    };

    const auto applyExpressionBoundaryPostconditions = [&]
        (std::unique_ptr<ProjectOopHirNode>& child,
         const ast::AstNode* childSyntax,
         std::size_t childMemberOrdinal) -> bool
    {
        const ClassMemberSymbol* activeMember = memberModel_ != nullptr &&
                childMemberOrdinal < memberModel_->members().size()
            ? &memberModel_->members()[childMemberOrdinal]
            : nullptr;
        const sema::EffectContext effects(activeMember != nullptr
            ? activeMember->effects
            : sema::EffectSet{});
        const auto directlyAllocatesHeap = [](ProjectOopHirNodeKind kind) noexcept
        {
            switch (kind)
            {
                case ProjectOopHirNodeKind::ArrayAllocation:
                case ProjectOopHirNodeKind::RangeArrayAllocation:
                case ProjectOopHirNodeKind::RawAllocation:
                case ProjectOopHirNodeKind::StringConcat:
                case ProjectOopHirNodeKind::StringSubstring:
                case ProjectOopHirNodeKind::StringFromCharArray:
                case ProjectOopHirNodeKind::DelegateCreate:
                case ProjectOopHirNodeKind::LambdaCreate:
                case ProjectOopHirNodeKind::EventAdd:
                    return true;
                default:
                    return false;
            }
        };
        const ProjectOopHirNode* loweredChild = child.get();
        if (child != nullptr && effects.allows(sema::Effect::NoHeap) &&
            loweredChild != nullptr && directlyAllocatesHeap(loweredChild->kind))
        {
            error = "NS3030: noheap callable '" + activeMember->name +
                "' cannot contain direct heap allocation '" +
                std::string(projectOopHirNodeKindName(loweredChild->kind)) + "'";
            child.reset();
        }
        if (child != nullptr && effects.allows(sema::Effect::NoHeap))
        {
            const ClassMemberSymbol& caller = *activeMember;
            const MethodCallBinding* call = callBinding(childSyntax);
            if (call != nullptr &&
                call->resolution == MethodCallResolutionKind::Resolved &&
                call->selectedMemberOrdinal.has_value() &&
                *call->selectedMemberOrdinal < memberModel_->members().size())
            {
                const ClassMemberSymbol& callee =
                    memberModel_->members()[*call->selectedMemberOrdinal];
                if (!callee.isNoHeap)
                {
                    error = "NS3030: noheap callable '" + caller.signatureKey +
                        "' cannot call non-noheap callable '" +
                        callee.signatureKey + "'";
                    child.reset();
                }
            }
            if (child != nullptr && loweredChild != nullptr &&
                (loweredChild->kind == ProjectOopHirNodeKind::DelegateInvoke ||
                 loweredChild->kind == ProjectOopHirNodeKind::EventInvoke))
            {
                error = "NS3030: noheap callable '" + caller.signatureKey +
                    "' cannot invoke an indirect target without a noheap contract";
                child.reset();
            }
            if (child != nullptr && loweredChild != nullptr &&
                (loweredChild->kind == ProjectOopHirNodeKind::PlacementConstruction ||
                 loweredChild->kind == ProjectOopHirNodeKind::ValueConstruction))
            {
                const ObjectConstructionBinding* binding =
                    constructionBinding(childSyntax);
                if (binding == nullptr ||
                    !binding->selectedConstructorMemberOrdinal.has_value() ||
                    *binding->selectedConstructorMemberOrdinal >=
                        memberModel_->members().size())
                {
                    error = "NS3030: noheap callable '" + caller.signatureKey +
                        "' cannot invoke an implicit constructor without a noheap contract";
                    child.reset();
                }
                else
                {
                    const ClassMemberSymbol& constructor =
                        memberModel_->members()[
                            *binding->selectedConstructorMemberOrdinal];
                    if (!constructor.isNoHeap)
                    {
                        error = "NS3030: noheap callable '" +
                            caller.signatureKey +
                            "' cannot call non-noheap constructor '" +
                            constructor.signatureKey + "'";
                        child.reset();
                    }
                }
            }
        }
        if (child == nullptr && !error.empty() &&
            !lastErrorRange_.isValid() && childSyntax != nullptr)
        {
            lastErrorRange_ = childSyntax->range();
        }
        return child != nullptr;
    };

    const auto initializeSwitchStatementFrame =
        [&](IterativeExpressionFrame& frame, const ast::AstNode* statement) -> bool
    {
        const MethodSwitchBinding* binding = switchBinding(statement);
        if (binding == nullptr)
        {
            error = "Stage 34 OOP HIR reached a switch statement with no semantic binding";
            return false;
        }
        frame.kind = IterativeExpressionKind::SwitchStatement;
        frame.syntax = statement;
        frame.switchStatement.emplace();
        frame.switchStatement->binding = binding;
        frame.switchStatement->node = makeNode(
            ProjectOopHirNodeKind::Switch, statement, "builtin:void");
        return true;
    };
    const auto initializeForStatementFrame =
        [&](IterativeExpressionFrame& frame, const ast::AstNode* statement)
    {
        frame.kind = IterativeExpressionKind::ForStatement;
        frame.syntax = statement;
        frame.forStatement.emplace();
        frame.forStatement->node = makeNode(
            ProjectOopHirNodeKind::For, statement, "builtin:void");
        frame.cleanupDepth = activeHirCleanupRegions_.size();
        frame.cleanupActive = true;
    };
    const auto initializeForeachStatementFrame =
        [&](IterativeExpressionFrame& frame, const ast::AstNode* statement) -> bool
    {
        const MethodLocalSymbol* sourceLocal = localSymbol(statement->first());
        if (sourceLocal == nullptr)
        {
            error = "Stage 34 foreach source local is missing its Stage 27 semantic binding";
            return false;
        }
        const bool isArray = sourceLocal->typeKey.size() > 2U &&
            sourceLocal->typeKey.compare(
                sourceLocal->typeKey.size() - 2U, 2U, "[]") == 0;
        const bool isSpan =
            sourceLocal->typeKey.starts_with("type:NativeSharp.Core.Span<") ||
            sourceLocal->typeKey.starts_with("type:NativeSharp.Core.Span__g") ||
            sourceLocal->typeKey.find("::NativeSharp.Core.Span__g") != std::string::npos ||
            sourceLocal->typeKey.starts_with("type:NativeSharp.Core.ReadOnlySpan<") ||
            sourceLocal->typeKey.starts_with("type:NativeSharp.Core.ReadOnlySpan__g") ||
            sourceLocal->typeKey.find("::NativeSharp.Core.ReadOnlySpan__g") != std::string::npos;

        frame.kind = IterativeExpressionKind::ForeachStatement;
        frame.syntax = statement;
        frame.foreachStatement.emplace();
        frame.foreachStatement->node = makeNode(
            ProjectOopHirNodeKind::Block, statement, "builtin:void");
        frame.foreachStatement->chosenCandidate = isArray
            ? statement->second()
            : (isSpan && statement->children().size() > 1U
                ? statement->children()[1]
                : statement->third());
        return true;
    };
    const auto initializeExitValueStatementFrame =
        [&](IterativeExpressionFrame& frame, const ast::AstNode* statement) -> bool
    {
        frame.kind = IterativeExpressionKind::ExitValueStatement;
        frame.syntax = statement;
        frame.exitValueStatement.emplace();
        ExitValueStatementFrameState& state = *frame.exitValueStatement;

        switch (statement->kind())
        {
            case ast::AstNodeKind::BreakStatement:
                state.node = makeNode(
                    ProjectOopHirNodeKind::Break, statement, "builtin:void");
                applyStructuredExitBinding(*state.node, statement);
                state.node->stringValue = statement->text();
                state.loopBreakBinding = methodBodyModel_ != nullptr
                    ? methodBodyModel_->loopBreakBinding(statement) : nullptr;
                if (state.loopBreakBinding == nullptr)
                {
                    error = "T07-F05 break value has no loop-expression semantic binding";
                    return false;
                }
                frame.addressContext = false;
                return true;

            case ast::AstNodeKind::ReturnStatement:
                state.node = makeNode(
                    ProjectOopHirNodeKind::Return, statement, "builtin:void");
                applyStructuredExitBinding(*state.node, statement);
                if (memberModel_ != nullptr &&
                    frame.memberOrdinal < memberModel_->members().size())
                {
                    const ClassMemberSymbol& member =
                        memberModel_->members()[frame.memberOrdinal];
                    state.referenceReturn = member.isReferenceReturn;
                    state.valueReturningCallable =
                        member.kind == ClassMemberKind::Method;
                    state.declaredTypeKey = member.declaredTypeKey;
                    state.declaredOwnership = member.declaredOwnership;
                }
                else
                {
                    const std::size_t callableBase = memberModel_ != nullptr
                        ? memberModel_->members().size() : 0U;
                    const std::size_t functionOrdinal = frame.memberOrdinal >= callableBase
                        ? frame.memberOrdinal - callableBase
                        : std::numeric_limits<std::size_t>::max();
                    if (namespaceFunctionModel_ == nullptr ||
                        functionOrdinal >= namespaceFunctionModel_->functions().size())
                    {
                        error = "Stage 34 return refers to invalid callable";
                        return false;
                    }
                    const NamespaceFunctionSymbol& function =
                        namespaceFunctionModel_->functions()[functionOrdinal];
                    state.referenceReturn = function.isReferenceReturn;
                    state.valueReturningCallable = true;
                    state.declaredTypeKey = function.declaredTypeKey;
                    state.declaredOwnership = function.declaredOwnership;
                }
                frame.addressContext = state.referenceReturn;
                return true;

            case ast::AstNodeKind::ThrowStatement:
            {
                const MethodExceptionBinding* binding = exceptionBinding(statement);
                if (binding == nullptr)
                {
                    error = "Stage 45.6 throw statement has no Stage 45.2 exception binding";
                    return false;
                }
                state.node = makeNode(
                    binding->kind == MethodExceptionOperationKind::Rethrow
                        ? ProjectOopHirNodeKind::Rethrow
                        : ProjectOopHirNodeKind::Throw,
                    statement,
                    binding->exceptionTypeKey.empty()
                        ? "builtin:void" : binding->exceptionTypeKey);
                state.node->exceptionTypeOrdinal = binding->exceptionTypeOrdinal;
                state.node->cleanupContinuationKind =
                    MethodCleanupContinuationKind::Exception;

                const std::size_t retainedDepth = activeHirExceptionHandlers_.empty()
                    ? 0U
                    : activeHirExceptionHandlers_.back().retainedCleanupDepth;
                for (std::size_t index = activeHirCleanupRegions_.size();
                    index > retainedDepth; --index)
                {
                    state.node->cleanupRegionOrdinals.push_back(
                        activeHirCleanupRegions_[index - 1U]);
                }
                if (!activeHirExceptionHandlers_.empty())
                {
                    state.node->exceptionTargetRegionOrdinal =
                        activeHirExceptionHandlers_.back().hirRegionOrdinal;
                }
                frame.addressContext = false;
                return true;
            }

            default:
                error = "non-value exit statement reached iterative HIR exit lowering";
                return false;
        }
    };

    const std::optional<IterativeExpressionKind> rootKind =
        iterativeKind(expression, addressContext);
    if (rootKind.has_value() || statementContinuation != nullptr)
    {
        std::vector<IterativeExpressionFrame> frames;
        if (statementContinuation != nullptr)
        {
            IterativeExpressionFrame frame;
            if (statementContinuation->kind() ==
                ast::AstNodeKind::VariableDeclarationStatement)
            {
                frame.kind = IterativeExpressionKind::VariableDeclaration;
            }
            else if (statementContinuation->kind() ==
                ast::AstNodeKind::DeconstructionDeclarationStatement)
            {
                frame.kind = IterativeExpressionKind::DeconstructionDeclaration;
            }
            else if (statementContinuation->kind() ==
                ast::AstNodeKind::InlineAssemblyStatement)
            {
                frame.kind = IterativeExpressionKind::InlineAssemblyStatement;
            }
            else if (statementContinuation->kind() ==
                ast::AstNodeKind::StaticAssertStatement)
            {
                frame.kind = IterativeExpressionKind::StaticAssertStatement;
            }
            else if (statementContinuation->kind() ==
                ast::AstNodeKind::IfStatement)
            {
                frame.kind = IterativeExpressionKind::IfStatement;
                frame.loopBlockNode = makeNode(
                    ProjectOopHirNodeKind::If,
                    statementContinuation,
                    "builtin:void");
            }
            else if (statementContinuation->kind() ==
                    ast::AstNodeKind::WhileStatement ||
                statementContinuation->kind() == ast::AstNodeKind::DoWhileStatement)
            {
                frame.kind = IterativeExpressionKind::LoopStatement;
                frame.loopBlockNode = makeNode(
                    statementContinuation->kind() == ast::AstNodeKind::DoWhileStatement
                        ? ProjectOopHirNodeKind::DoWhile
                        : ProjectOopHirNodeKind::While,
                    statementContinuation,
                    "builtin:void");
            }
            else if (statementContinuation->kind() == ast::AstNodeKind::SwitchStatement)
            {
                if (!initializeSwitchStatementFrame(frame, statementContinuation))
                    return nullptr;
            }
            else if (statementContinuation->kind() == ast::AstNodeKind::ForStatement)
            {
                initializeForStatementFrame(frame, statementContinuation);
            }
            else if (statementContinuation->kind() == ast::AstNodeKind::ForeachStatement)
            {
                if (!initializeForeachStatementFrame(frame, statementContinuation))
                    return nullptr;
            }
            else if ((statementContinuation->kind() == ast::AstNodeKind::BreakStatement ||
                      statementContinuation->kind() == ast::AstNodeKind::ReturnStatement ||
                      statementContinuation->kind() == ast::AstNodeKind::ThrowStatement) &&
                statementContinuation->first() != nullptr)
            {
                frame.memberOrdinal = memberOrdinal;
                if (!initializeExitValueStatementFrame(frame, statementContinuation))
                    return nullptr;
                addressContext = frame.addressContext;
            }
            else
            {
                frame.kind = IterativeExpressionKind::ExpressionStatement;
            }
            frame.syntax = statementContinuation;
            frame.memberOrdinal = memberOrdinal;
            frames.push_back(std::move(frame));
        }
        const ast::AstNode* current = expression;
        bool currentAddressContext = addressContext;
        std::size_t currentMemberOrdinal = memberOrdinal;
        IterativeInputKind currentInputKind = IterativeInputKind::Expression;
        const std::size_t rootCleanupDepth = activeHirCleanupRegions_.size();
        const auto rollbackMatchCleanups = [&]()
        {
            // A failed If condition must still lower its then block to preserve
            // the historical diagnostic ordering.  Its continuation owns the
            // precise descendant unwind, so do not discard enclosing cleanup
            // state before that continuation has had a chance to run.
            for (auto frame = frames.rbegin(); frame != frames.rend(); ++frame)
            {
                if ((frame->kind == IterativeExpressionKind::IfStatement ||
                     frame->kind == IterativeExpressionKind::LoopStatement) &&
                    frame->stage == 0U)
                {
                    return;
                }
            }
            bool loopBlockActive = false;
            for (auto frame = frames.rbegin(); frame != frames.rend(); ++frame)
            {
                if (frame->kind == IterativeExpressionKind::Match && frame->cleanupActive)
                    activeHirCleanupRegions_.resize(frame->cleanupDepth);
                if (frame->kind == IterativeExpressionKind::SwitchStatement &&
                    frame->cleanupActive)
                {
                    activeHirCleanupRegions_.resize(frame->cleanupDepth);
                }
                if (frame->kind == IterativeExpressionKind::ForStatement &&
                    frame->cleanupActive)
                {
                    activeHirCleanupRegions_.resize(frame->cleanupDepth);
                }
                if (frame->kind == IterativeExpressionKind::LoopBlock)
                    loopBlockActive = true;
            }
            if (loopBlockActive) activeHirCleanupRegions_.resize(rootCleanupDepth);
        };
        const auto resumePendingConditionStatementAfterFailure = [&]() -> bool
        {
            for (std::size_t index = frames.size(); index > 0U; --index)
            {
                IterativeExpressionFrame& candidate = frames[index - 1U];
                if ((candidate.kind != IterativeExpressionKind::IfStatement &&
                     candidate.kind != IterativeExpressionKind::LoopStatement) ||
                    candidate.stage != 0U || candidate.syntax == nullptr)
                {
                    continue;
                }

                std::size_t retainedCleanupDepth = activeHirCleanupRegions_.size();
                for (std::size_t child = index; child < frames.size(); ++child)
                {
                    if (frames[child].kind == IterativeExpressionKind::LoopBlock)
                    {
                        retainedCleanupDepth = std::min(
                            retainedCleanupDepth, frames[child].cleanupDepth);
                    }
                    if (frames[child].kind == IterativeExpressionKind::Match &&
                        frames[child].cleanupActive)
                    {
                        retainedCleanupDepth = std::min(
                            retainedCleanupDepth, frames[child].cleanupDepth);
                    }
                }
                activeHirCleanupRegions_.resize(retainedCleanupDepth);
                frames.resize(index);

                IterativeExpressionFrame& pendingStatement = frames.back();
                pendingStatement.statementConditionFailed = true;
                pendingStatement.statementConditionError = std::move(error);
                pendingStatement.stage = 1U;
                current = pendingStatement.syntax->second();
                currentMemberOrdinal = pendingStatement.memberOrdinal;
                currentAddressContext = false;
                currentInputKind = IterativeInputKind::LoopBlock;
                lowered.reset();
                error.clear();
                return true;
            }
            return false;
        };
        const auto isBoundCallKind = [](IterativeExpressionKind kind) noexcept
        {
            return kind == IterativeExpressionKind::UserUnaryOperator ||
                kind == IterativeExpressionKind::UserBinaryOperator ||
                kind == IterativeExpressionKind::UserConversion ||
                kind == IterativeExpressionKind::PropertyGetter ||
                kind == IterativeExpressionKind::UserIndexerGetter;
        };
        const auto initializeReceiverFrame = [&](ReceiverFrameState& receiver,
                                                 const ast::AstNode* receiverSyntax,
                                                 std::string_view receiverTypeKey)
            -> std::optional<bool>
        {
            if (receiverTypeKey.empty())
            {
                error = "Stage 40 receiver has no semantic class/interface/struct type";
                return std::nullopt;
            }
            receiver.syntax = receiverSyntax;
            receiver.typeKey = receiverTypeKey;
            if (receiverSyntax != nullptr &&
                receiverSyntax->kind() != ast::AstNodeKind::BaseExpression)
            {
                receiver.materializeTemporary =
                    isStructValueType(receiverTypeKey) &&
                    !isNaturallyAddressable(receiverSyntax);
                return false;
            }
            receiver.lowered = lowerImplicitReceiver(
                receiverSyntax, receiverTypeKey, error);
            if (receiver.lowered == nullptr) return std::nullopt;
            return true;
        };
        const auto acceptReceiverChild = [&](ReceiverFrameState& receiver,
                                             std::unique_ptr<ProjectOopHirNode> child)
        {
            if (receiver.materializeTemporary)
            {
                auto temporary = makeNode(
                    ProjectOopHirNodeKind::TemporaryAddress,
                    receiver.syntax,
                    std::string(receiver.typeKey));
                temporary->elementTypeKey = receiver.typeKey;
                temporary->children.push_back(std::move(child));
                child = std::move(temporary);
            }
            receiver.lowered = std::move(child);
        };
        const auto initializeBoundCallFrame = [&](IterativeExpressionFrame& frame,
                                                   const MethodCallBinding* binding,
                                                   std::vector<const ast::AstNode*> arguments,
                                                   bool nullConditional) -> bool
        {
            if (binding == nullptr ||
                binding->resolution != MethodCallResolutionKind::Resolved ||
                !binding->selectedMemberOrdinal.has_value())
            {
                error = "Stage 38 requires a resolved synthetic call binding";
                return false;
            }
            BoundCallFrameState call;
            call.binding = binding;
            call.typeBinding = expressionType(frame.syntax);
            call.target = resolveBoundCallTarget(*binding);
            call.argumentSyntaxes = std::move(arguments);
            call.arguments.reserve(call.argumentSyntaxes.size());
            call.nullConditional = nullConditional;
            if (binding->staticCall)
            {
                call.stage = call.argumentSyntaxes.empty()
                    ? BoundCallFrameStage::Ready
                    : BoundCallFrameStage::Arguments;
                frame.boundCall.emplace(std::move(call));
                return true;
            }
            const std::optional<bool> receiverReady = initializeReceiverFrame(
                call.receiver,
                call.target.receiverSyntax,
                call.target.receiverTypeKey);
            if (!receiverReady.has_value()) return false;
            call.stage = *receiverReady
                ? (call.argumentSyntaxes.empty()
                    ? BoundCallFrameStage::Ready
                    : BoundCallFrameStage::Arguments)
                : BoundCallFrameStage::Receiver;
            frame.boundCall.emplace(std::move(call));
            return true;
        };
        struct BoundCallChild final
        {
            const ast::AstNode* syntax = nullptr;
            bool addressContext = false;
        };
        const auto nextBoundCallChild = [&](const BoundCallFrameState& call)
            -> std::optional<BoundCallChild>
        {
            if (call.stage == BoundCallFrameStage::Receiver)
            {
                return BoundCallChild{
                    call.receiver.syntax,
                    isStructValueType(call.receiver.typeKey) &&
                        !call.receiver.materializeTemporary};
            }
            if (call.stage != BoundCallFrameStage::Arguments ||
                call.nextArgument >= call.argumentSyntaxes.size())
            {
                return std::nullopt;
            }
            const ClassMemberSymbol* target = callableSymbol(call.target.memberOrdinal);
            const bool address = target != nullptr && target->isUserOperator &&
                boundCallArgumentNeedsAddress(call.target.memberOrdinal, call.nextArgument);
            return BoundCallChild{call.argumentSyntaxes[call.nextArgument], address};
        };
        const auto acceptBoundCallChild = [&](BoundCallFrameState& call,
                                              std::unique_ptr<ProjectOopHirNode> child)
        {
            if (call.stage == BoundCallFrameStage::Receiver)
            {
                acceptReceiverChild(call.receiver, std::move(child));
                call.stage = call.argumentSyntaxes.empty()
                    ? BoundCallFrameStage::Ready
                    : BoundCallFrameStage::Arguments;
                return;
            }
            const std::size_t index = call.nextArgument++;
            call.arguments.push_back(prepareBoundCallArgument(
                *call.binding,
                call.target.memberOrdinal,
                index,
                call.argumentSyntaxes[index],
                std::move(child),
                call.hasOwnedTemporary));
            if (call.nextArgument == call.argumentSyntaxes.size())
                call.stage = BoundCallFrameStage::Ready;
        };
        const auto validateOrdinaryCallMember = [&](const OrdinaryCallFrameState& call) -> bool
        {
            if (memberModel_ == nullptr ||
                call.selectedMemberOrdinal >= memberModel_->members().size())
            {
                error = "NSC-LIMIT-020 selected call has no callable metadata";
                return false;
            }
            return true;
        };
        const auto initializeOrdinaryCallFrame = [&](IterativeExpressionFrame& frame) -> bool
        {
            const MethodCallBinding* binding = callBinding(frame.syntax);
            if (binding == nullptr || !binding->selectedMemberOrdinal.has_value())
            {
                error = "Stage 34 requires a resolved Stage 27 method call";
                return false;
            }
            OrdinaryCallFrameState call;
            call.binding = binding;
            call.selectedMemberOrdinal = *binding->selectedMemberOrdinal;
            call.forwardedReceiver = binding->forwardedReceiver;
            call.receiver.syntax = binding->receiverSyntax;
            call.receiver.typeKey = binding->receiverTypeKey;
            bool interfaceDispatch = binding->interfaceCall;
            if (interfaceDispatch && !binding->forwardedReceiver && interfaceModel_ != nullptr &&
                memberModel_ != nullptr && binding->receiverTypeOrdinal.has_value() &&
                call.receiver.syntax != nullptr &&
                (call.receiver.syntax->kind() == ast::AstNodeKind::CastExpression ||
                 call.receiver.syntax->kind() == ast::AstNodeKind::SafeCastExpression) &&
                call.receiver.syntax->first() != nullptr &&
                (call.receiver.syntax->first()->kind() == ast::AstNodeKind::NewExpression ||
                 call.receiver.syntax->first()->kind() ==
                    ast::AstNodeKind::ObjectCreationExpression))
            {
                const MethodExpressionTypeBinding* concreteType =
                    expressionType(call.receiver.syntax->first());
                const std::optional<std::size_t> concreteTypeOrdinal = concreteType != nullptr
                    ? interfaceModel_->typeOrdinalForTypeKey(concreteType->typeKey)
                    : std::nullopt;
                const ClassInterfaceTablePlan* plan = concreteTypeOrdinal.has_value()
                    ? interfaceModel_->tablePlan(
                        *concreteTypeOrdinal, *binding->receiverTypeOrdinal)
                    : nullptr;
                if (plan != nullptr)
                {
                    const auto contract = std::find(
                        plan->interfaceMethodMemberOrdinals.begin(),
                        plan->interfaceMethodMemberOrdinals.end(),
                        call.selectedMemberOrdinal);
                    if (contract != plan->interfaceMethodMemberOrdinals.end())
                    {
                        const std::size_t slot = static_cast<std::size_t>(
                            contract - plan->interfaceMethodMemberOrdinals.begin());
                        if (slot < plan->implementationMemberOrdinals.size() &&
                            plan->implementationMemberOrdinals[slot].has_value())
                        {
                            call.selectedMemberOrdinal =
                                *plan->implementationMemberOrdinals[slot];
                            call.receiver.syntax = call.receiver.syntax->first();
                            call.receiver.typeKey = concreteType->typeKey;
                            interfaceDispatch = false;
                        }
                    }
                }
            }
            call.node = makeNode(
                binding->staticCall ? ProjectOopHirNodeKind::StaticCall
                                    : ProjectOopHirNodeKind::InstanceCall,
                frame.syntax,
                binding->resultTypeKey);
            const bool writtenNullConditional = frame.syntax->first() != nullptr &&
                frame.syntax->first()->kind() ==
                    ast::AstNodeKind::NullConditionalMemberExpression;
            const MethodExpressionTypeBinding* receiverFlow =
                expressionType(call.receiver.syntax);
            const bool provenNonNull = receiverFlow != nullptr &&
                receiverFlow->flowProvenNonNull;
            call.node->nullConditionalCall = writtenNullConditional && !provenNonNull;
            if (writtenNullConditional && provenNonNull)
                call.node->stringValue = "flow-nonnull";
            const MethodExpressionTypeBinding* callType = expressionType(frame.syntax);
            call.node->booleanValue = callType != nullptr &&
                callType->flowActiveVariantCase.has_value();
            call.node->memberOrdinal = call.selectedMemberOrdinal;
            call.node->baseReceiver = binding->baseReceiver;
            call.node->interfaceDispatch = interfaceDispatch;
            if (interfaceDispatch) call.node->targetTypeOrdinal = binding->receiverTypeOrdinal;

            if (binding->forwardedReceiver)
            {
                call.stage = OrdinaryCallFrameStage::Receiver;
            }
            else if (!binding->staticCall)
            {
                const std::optional<bool> receiverReady = initializeReceiverFrame(
                    call.receiver, call.receiver.syntax, call.receiver.typeKey);
                if (!receiverReady.has_value()) return false;
                if (*receiverReady)
                {
                    call.node->children.push_back(std::move(call.receiver.lowered));
                    if (!validateOrdinaryCallMember(call)) return false;
                    call.stage = frame.syntax->children().empty()
                        ? OrdinaryCallFrameStage::Ready
                        : OrdinaryCallFrameStage::Arguments;
                }
                else
                {
                    call.stage = OrdinaryCallFrameStage::Receiver;
                }
            }
            else
            {
                if (!validateOrdinaryCallMember(call)) return false;
                call.stage = frame.syntax->children().empty()
                    ? OrdinaryCallFrameStage::Ready
                    : OrdinaryCallFrameStage::Arguments;
            }
            frame.ordinaryCall.emplace(std::move(call));
            return true;
        };
        struct OrdinaryCallChild final
        {
            const ast::AstNode* syntax = nullptr;
            bool addressContext = false;
        };
        const auto nextOrdinaryCallChild = [&](const OrdinaryCallFrameState& call)
            -> std::optional<OrdinaryCallChild>
        {
            if (call.stage == OrdinaryCallFrameStage::Receiver)
            {
                const bool address = !call.forwardedReceiver &&
                    isStructValueType(call.receiver.typeKey) &&
                    !call.receiver.materializeTemporary;
                return OrdinaryCallChild{call.receiver.syntax, address};
            }
            if (call.stage != OrdinaryCallFrameStage::Arguments ||
                call.nextArgument >= call.node->syntax->children().size())
            {
                return std::nullopt;
            }
            return OrdinaryCallChild{
                call.node->syntax->children()[call.nextArgument], false};
        };
        const auto acceptOrdinaryCallChild = [&](OrdinaryCallFrameState& call,
                                                  std::unique_ptr<ProjectOopHirNode> child) -> bool
        {
            if (call.stage == OrdinaryCallFrameStage::Receiver)
            {
                if (call.forwardedReceiver)
                {
                    if (!call.binding->selectedConversions.empty() &&
                        call.binding->selectedConversions.front().kind !=
                            ProjectConversionKind::Identity)
                    {
                        child = wrapImplicitConversion(
                            std::move(child),
                            call.binding->selectedConversions.front(),
                            call.binding->receiverSyntax);
                    }
                    call.node->children.push_back(std::move(child));
                    call.node->argumentParameterOrdinals.push_back(0U);
                    call.conversionOffset = 1U;
                }
                else
                {
                    acceptReceiverChild(call.receiver, std::move(child));
                    call.node->children.push_back(std::move(call.receiver.lowered));
                }
                if (!validateOrdinaryCallMember(call)) return false;
                call.stage = call.node->syntax->children().empty()
                    ? OrdinaryCallFrameStage::Ready
                    : OrdinaryCallFrameStage::Arguments;
                return true;
            }

            const std::size_t index = call.nextArgument++;
            const std::size_t selectedIndex = index + call.conversionOffset;
            if (selectedIndex >= call.binding->selectedArgumentParameterOrdinals.size())
            {
                error = "named call argument has no selected parameter ordinal";
                return false;
            }
            const std::size_t parameterIndex =
                call.binding->selectedArgumentParameterOrdinals[selectedIndex];
            const ast::AstNode* argumentSyntax = call.node->syntax->children()[index];
            if (selectedIndex < call.binding->selectedConversions.size() &&
                call.binding->selectedConversions[selectedIndex].kind !=
                    ProjectConversionKind::Identity)
            {
                child = wrapImplicitConversion(
                    std::move(child),
                    call.binding->selectedConversions[selectedIndex],
                    argumentSyntax);
            }
            const ClassMemberSymbol& selectedMember =
                memberModel_->members()[call.selectedMemberOrdinal];
            const bool packElement =
                selectedIndex < call.binding->selectedParameterPackElements.size() &&
                call.binding->selectedParameterPackElements[selectedIndex];
            if (packElement)
            {
                if (parameterIndex >= selectedMember.parameters.size())
                {
                    error = "T01-F02 method parameter pack has no selected parameter";
                    return false;
                }
                const std::string& elementType =
                    selectedMember.parameters[parameterIndex].parameterPackElementTypeKey;
                if (elementType.empty())
                {
                    error = "T01-F02 method parameter pack lost its element type";
                    return false;
                }
                if (!call.expandedPackChildIndex.has_value())
                {
                    auto pack = makeNode(
                        ProjectOopHirNodeKind::ParameterPack,
                        argumentSyntax,
                        selectedMember.parameters[parameterIndex].typeKey);
                    pack->elementTypeKey = elementType;
                    call.expandedPackChildIndex = call.node->children.size();
                    call.node->children.push_back(std::move(pack));
                    call.node->argumentParameterOrdinals.push_back(parameterIndex);
                }
                call.node->children[*call.expandedPackChildIndex]->children.push_back(
                    std::move(child));
            }
            else
            {
                if (parameterIndex < selectedMember.parameters.size())
                {
                    child = wrapCallArgumentBorrow(
                        std::move(child), argumentSyntax, call.selectedMemberOrdinal,
                        parameterIndex, call.hasOwnedTemporary);
                }
                call.node->children.push_back(std::move(child));
                call.node->argumentParameterOrdinals.push_back(parameterIndex);
            }
            if (call.nextArgument == call.node->syntax->children().size())
                call.stage = OrdinaryCallFrameStage::Ready;
            return true;
        };
        const auto finishOrdinaryCall = [&](IterativeExpressionFrame& frame)
            -> std::unique_ptr<ProjectOopHirNode>
        {
            OrdinaryCallFrameState& call = *frame.ordinaryCall;
            const ClassMemberSymbol& selectedMember =
                memberModel_->members()[call.selectedMemberOrdinal];
            for (std::size_t index = 0;
                index < call.binding->selectedDefaultArguments.size(); ++index)
            {
                if (index >= call.binding->selectedDefaultArgumentParameterOrdinals.size())
                {
                    error = "selected default argument has no parameter ordinal";
                    return nullptr;
                }
                const std::size_t parameterIndex =
                    call.binding->selectedDefaultArgumentParameterOrdinals[index];
                if (parameterIndex >= selectedMember.parameters.size())
                {
                    error = "NSC-LIMIT-020 selected default argument has no matching parameter";
                    return nullptr;
                }
                auto argument = lowerDefaultArgument(
                    call.binding->selectedDefaultArguments[index],
                    selectedMember.parameters[parameterIndex].typeKey,
                    error);
                if (argument == nullptr) return nullptr;
                call.node->children.push_back(std::move(argument));
                call.node->argumentParameterOrdinals.push_back(parameterIndex);
            }
            if (call.binding->selectedSynthesizesEmptyParameterPack)
            {
                if (!call.binding->selectedParameterPackOrdinal.has_value() ||
                    *call.binding->selectedParameterPackOrdinal >=
                        selectedMember.parameters.size())
                {
                    error = "T01-F02 empty method parameter pack has no parameter";
                    return nullptr;
                }
                const std::size_t parameterIndex =
                    *call.binding->selectedParameterPackOrdinal;
                const std::string& elementType =
                    selectedMember.parameters[parameterIndex].parameterPackElementTypeKey;
                if (elementType.empty())
                {
                    error = "T01-F02 empty method parameter pack lost its element type";
                    return nullptr;
                }
                auto pack = makeNode(
                    ProjectOopHirNodeKind::ParameterPack,
                    frame.syntax,
                    selectedMember.parameters[parameterIndex].typeKey);
                pack->elementTypeKey = elementType;
                call.node->children.push_back(std::move(pack));
                call.node->argumentParameterOrdinals.push_back(parameterIndex);
            }
            if (frame.addressContext && !selectedMember.isReferenceReturn)
            {
                error = "call result is not addressable unless the selected method returns ref";
                return nullptr;
            }
            call.node->referenceResult = selectedMember.isReferenceReturn;
            auto loweredCall = wrapFullExpressionCleanup(
                std::move(call.node), frame.syntax, call.hasOwnedTemporary);
            if (!selectedMember.isReferenceReturn || frame.addressContext)
                return loweredCall;
            auto load = makeNode(
                ProjectOopHirNodeKind::PointerLoad,
                frame.syntax,
                selectedMember.declaredTypeKey);
            load->children.push_back(std::move(loweredCall));
            return load;
        };
        const auto prepareAggregateCrossStructuralFrame =
            [&](AggregateCrossStructuralFrame& structural) -> bool
        {
            if (const auto tupleElements =
                    ProjectTypeConversion::tupleElementTypeKeys(
                        structural.targetTypeKey);
                tupleElements.has_value())
            {
                if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
                {
                    error = "tuple initializer requires the object layout model";
                    return false;
                }
                const auto layout = objectLayoutModel_->tupleLayoutForTypeKey(
                    *nameLookup_, structural.targetTypeKey);
                if (!layout.has_value() ||
                    layout->offsets.size() != tupleElements->size())
                {
                    error = "tuple initializer has no structural layout for '" +
                        structural.targetTypeKey + "'";
                    return false;
                }
                for (std::size_t index = 0U; index < tupleElements->size(); ++index)
                {
                    const auto elementLayout =
                        objectLayoutModel_->valueLayoutForTypeKey(
                            *nameLookup_, (*tupleElements)[index]);
                    if (!elementLayout.has_value())
                    {
                        error = "tuple initializer cannot size element " +
                            std::to_string(index);
                        return false;
                    }
                    structural.slots.push_back(AggregateCrossSlot{
                        (*tupleElements)[index],
                        static_cast<std::uint64_t>(layout->offsets[index]),
                        std::nullopt,
                        std::nullopt,
                        0U,
                        elementLayout->second,
                        false});
                }
            }
            else if (const auto elementTypeKey =
                    ProjectMemberModel::fixedArrayElementTypeKey(
                        structural.targetTypeKey);
                elementTypeKey.has_value())
            {
                const auto count =
                    ProjectMemberModel::fixedArrayCount(structural.targetTypeKey);
                if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
                {
                    error = "C7 aggregate initializer requires the object layout model";
                    return false;
                }
                const auto layout = objectLayoutModel_->valueLayoutForTypeKey(
                    *nameLookup_, *elementTypeKey);
                if (!count.has_value() || !layout.has_value() || layout->first == 0U)
                {
                    error =
                        "C7 aggregate initializer cannot size the element type of '" +
                        structural.targetTypeKey + "'";
                    return false;
                }
                for (std::uint64_t index = 0U; index < *count; ++index)
                {
                    structural.slots.push_back(AggregateCrossSlot{
                        *elementTypeKey,
                        index * layout->first,
                        std::nullopt,
                        std::nullopt,
                        0U,
                        layout->second,
                        false});
                }
            }
            else
            {
                if (objectLayoutModel_ == nullptr || inheritanceModel_ == nullptr)
                {
                    error = "C7 aggregate initializer requires the object layout model";
                    return false;
                }
                const auto typeOrdinal =
                    inheritanceModel_->typeOrdinalForTypeKey(structural.targetTypeKey);
                const StructValueLayout* layout = typeOrdinal.has_value()
                    ? objectLayoutModel_->structLayoutForType(*typeOrdinal) : nullptr;
                if (layout == nullptr)
                {
                    error = "C7 aggregate initializer has no value layout for '" +
                        structural.targetTypeKey + "'";
                    return false;
                }
                for (const ObjectFieldLayout& field : layout->fields)
                {
                    const ClassMemberSymbol* member = memberModel_ != nullptr &&
                            field.memberOrdinal < memberModel_->members().size()
                        ? &memberModel_->members()[field.memberOrdinal] : nullptr;
                    if (member != nullptr && member->isUnnamedBitField) continue;
                    structural.slots.push_back(AggregateCrossSlot{
                        field.typeKey,
                        field.offset,
                        field.bitOffset,
                        field.bitWidth,
                        field.bitWidth.has_value() ? field.size : 0U,
                        field.alignment,
                        member != nullptr && member->isVolatile});
                    if (layout->isUnion) break;
                }
            }

            structural.binding = methodBodyModel_ != nullptr
                ? methodBodyModel_->aggregateInitializerBinding(structural.syntax)
                : nullptr;
            if (structural.binding == nullptr ||
                structural.binding->entries.size() !=
                    structural.syntax->children().size())
            {
                error = "T01-F05 aggregate initializer has no semantic element mapping";
                return false;
            }
            structural.initialized = true;
            return true;
        };
        const auto advanceAggregateCrossFrame =
            [&](AggregateCrossFrameState& aggregate) -> bool
        {
            while (!aggregate.structuralFrames.empty())
            {
                AggregateCrossStructuralFrame& structural =
                    aggregate.structuralFrames.back();
                if (!structural.initialized)
                {
                    if (!prepareAggregateCrossStructuralFrame(structural)) return false;
                    if (structural.syntax->kind() == ast::AstNodeKind::WithExpression)
                    {
                        if (structural.syntax->first() == nullptr)
                        {
                            error = "T01-F07 with expression has no source value";
                            return false;
                        }
                        aggregate.pendingSyntax = structural.syntax->first();
                        aggregate.phase = AggregateCrossFramePhase::AwaitWithSource;
                        return true;
                    }
                }

                if (structural.entryIndex >= structural.binding->entries.size())
                {
                    aggregate.structuralFrames.pop_back();
                    continue;
                }
                const MethodAggregateInitializerEntryBinding& entry =
                    structural.binding->entries[structural.entryIndex++];
                const ast::AstNode* element = entry.syntax;
                if (entry.spread)
                {
                    if (element == nullptr ||
                        element->kind() != ast::AstNodeKind::SpreadElementExpression ||
                        element->first() == nullptr)
                    {
                        error = "T01-F05 aggregate spread has no source expression";
                        return false;
                    }
                    const ast::AstNode* sourceSyntax = element->first();
                    if (entry.moveSpread &&
                        sourceSyntax->kind() == ast::AstNodeKind::MoveExpression)
                    {
                        sourceSyntax = sourceSyntax->first();
                    }
                    aggregate.pendingSyntax = sourceSyntax;
                    aggregate.pendingTargetOrdinals = entry.targetElementOrdinals;
                    aggregate.pendingMoveSpread = entry.moveSpread;
                    aggregate.pendingMoveSourceLocalOrdinal =
                        entry.moveSourceLocalOrdinal;
                    aggregate.pendingMoveSourceParameterOrdinal =
                        entry.moveSourceParameterOrdinal;
                    aggregate.phase = AggregateCrossFramePhase::AwaitSpreadSource;
                    return true;
                }
                if (entry.targetElementOrdinals.size() != 1U ||
                    entry.targetElementOrdinals.front() >= structural.slots.size())
                {
                    error = "T01-F05 aggregate value has no unique target element";
                    return false;
                }
                const AggregateCrossSlot target =
                    structural.slots[entry.targetElementOrdinals.front()];
                const std::uint64_t slotOffset =
                    structural.baseOffset + target.offset;
                if (element->kind() ==
                    ast::AstNodeKind::AggregateInitializerExpression)
                {
                    AggregateCrossStructuralFrame child;
                    child.syntax = element;
                    child.targetTypeKey = target.typeKey;
                    child.baseOffset = slotOffset;
                    aggregate.structuralFrames.push_back(std::move(child));
                    continue;
                }
                aggregate.pendingSyntax = element;
                aggregate.pendingSlot = target;
                aggregate.pendingSlot.offset = slotOffset;
                aggregate.phase = AggregateCrossFramePhase::AwaitValue;
                return true;
            }
            aggregate.phase = AggregateCrossFramePhase::Ready;
            aggregate.pendingSyntax = nullptr;
            return true;
        };
        const auto initializeAggregateCrossFrame =
            [&](IterativeExpressionFrame& frame) -> bool
        {
            const MethodExpressionTypeBinding* targetType =
                expressionType(frame.syntax);
            if (targetType == nullptr)
            {
                error = "aggregate/with initializer has no bound target type";
                return false;
            }
            AggregateCrossFrameState aggregate;
            aggregate.node = makeNode(
                ProjectOopHirNodeKind::AggregateInitializer,
                frame.syntax,
                targetType->typeKey);
            AggregateCrossStructuralFrame root;
            root.syntax = frame.syntax;
            root.targetTypeKey = targetType->typeKey;
            aggregate.structuralFrames.push_back(std::move(root));
            if (!advanceAggregateCrossFrame(aggregate)) return false;
            frame.aggregateCross.emplace(std::move(aggregate));
            return true;
        };
        struct AggregateCrossChild final
        {
            const ast::AstNode* syntax = nullptr;
            bool addressContext = false;
        };
        const auto nextAggregateCrossChild =
            [&](const AggregateCrossFrameState& aggregate)
                -> std::optional<AggregateCrossChild>
        {
            if (aggregate.phase == AggregateCrossFramePhase::AwaitWithSource ||
                aggregate.phase == AggregateCrossFramePhase::AwaitValue)
            {
                return AggregateCrossChild{aggregate.pendingSyntax, false};
            }
            if (aggregate.phase == AggregateCrossFramePhase::AwaitSpreadSource)
            {
                return AggregateCrossChild{
                    aggregate.pendingSyntax,
                    aggregate.pendingMoveSpread};
            }
            return std::nullopt;
        };
        const auto acceptAggregateCrossChild =
            [&](AggregateCrossFrameState& aggregate,
                std::unique_ptr<ProjectOopHirNode> child) -> bool
        {
            if (aggregate.structuralFrames.empty())
            {
                error = "Stage 34 aggregate frame lost its structural owner";
                return false;
            }
            const AggregateCrossStructuralFrame& structural =
                aggregate.structuralFrames.back();
            if (aggregate.phase == AggregateCrossFramePhase::AwaitWithSource)
            {
                auto spread = makeNode(
                    ProjectOopHirNodeKind::AggregateSpread,
                    structural.syntax,
                    structural.targetTypeKey);
                spread->children.push_back(std::move(child));
                for (const AggregateCrossSlot& target : structural.slots)
                {
                    if (target.bitWidth.has_value())
                    {
                        error =
                            "T01-F07 with expressions over bit fields require explicit copy semantics";
                        return false;
                    }
                    auto descriptor = makeNode(
                        ProjectOopHirNodeKind::AggregateElement,
                        structural.syntax,
                        target.typeKey);
                    descriptor->integerValue =
                        structural.baseOffset + target.offset;
                    descriptor->secondaryIntegerValue = target.offset;
                    descriptor->bitStorageAlignment = target.storageAlignment;
                    descriptor->volatileAccess = target.volatileAccess;
                    spread->children.push_back(std::move(descriptor));
                }
                aggregate.node->children.push_back(std::move(spread));
            }
            else if (aggregate.phase ==
                AggregateCrossFramePhase::AwaitSpreadSource)
            {
                auto spread = makeNode(
                    ProjectOopHirNodeKind::AggregateSpread,
                    structural.binding->entries[structural.entryIndex - 1U].syntax,
                    structural.targetTypeKey);
                spread->booleanValue = aggregate.pendingMoveSpread;
                spread->localOrdinal = aggregate.pendingMoveSourceLocalOrdinal;
                spread->parameterOrdinal =
                    aggregate.pendingMoveSourceParameterOrdinal;
                spread->children.push_back(std::move(child));
                for (const std::size_t targetOrdinal :
                    aggregate.pendingTargetOrdinals)
                {
                    if (targetOrdinal >= structural.slots.size())
                    {
                        error = "T01-F05 aggregate spread target is out of range";
                        return false;
                    }
                    const AggregateCrossSlot& target =
                        structural.slots[targetOrdinal];
                    auto descriptor = makeNode(
                        ProjectOopHirNodeKind::AggregateElement,
                        structural.binding->entries[
                            structural.entryIndex - 1U].syntax,
                        target.typeKey);
                    descriptor->integerValue =
                        structural.baseOffset + target.offset;
                    descriptor->secondaryIntegerValue = target.offset;
                    descriptor->bitOffset = target.bitOffset;
                    descriptor->bitWidth = target.bitWidth;
                    descriptor->bitStorageSize = target.storageSize;
                    descriptor->bitStorageAlignment = target.storageAlignment;
                    descriptor->volatileAccess = target.volatileAccess;
                    if (target.bitWidth.has_value())
                    {
                        error =
                            "T01-F05 spread initialization of bit fields requires explicit named values";
                        return false;
                    }
                    spread->children.push_back(std::move(descriptor));
                }
                aggregate.node->children.push_back(std::move(spread));
            }
            else if (aggregate.phase == AggregateCrossFramePhase::AwaitValue)
            {
                const MethodExpressionTypeBinding* valueType =
                    expressionType(aggregate.pendingSyntax);
                const ProjectImplicitConversion conversion = classifyImplicit(
                    child->typeKey,
                    aggregate.pendingSlot.typeKey,
                    valueType != nullptr ? valueType->integerConstant : std::nullopt);
                if (conversion.exists() &&
                    conversion.kind != ProjectConversionKind::Identity)
                {
                    child = wrapImplicitConversion(
                        std::move(child), conversion, aggregate.pendingSyntax);
                }
                auto slot = makeNode(
                    ProjectOopHirNodeKind::AggregateElement,
                    aggregate.pendingSyntax,
                    aggregate.pendingSlot.typeKey);
                slot->integerValue = aggregate.pendingSlot.offset;
                slot->bitOffset = aggregate.pendingSlot.bitOffset;
                slot->bitWidth = aggregate.pendingSlot.bitWidth;
                slot->bitStorageSize = aggregate.pendingSlot.storageSize;
                slot->bitStorageAlignment =
                    aggregate.pendingSlot.storageAlignment;
                slot->volatileAccess = aggregate.pendingSlot.volatileAccess;
                slot->children.push_back(std::move(child));
                aggregate.node->children.push_back(std::move(slot));
            }
            else
            {
                error = "Stage 34 aggregate frame accepted an unexpected child";
                return false;
            }
            aggregate.phase = AggregateCrossFramePhase::Advance;
            aggregate.pendingSyntax = nullptr;
            aggregate.pendingTargetOrdinals.clear();
            return advanceAggregateCrossFrame(aggregate);
        };
        const auto finishAggregateCrossFrame =
            [&](IterativeExpressionFrame& frame)
                -> std::unique_ptr<ProjectOopHirNode>
        {
            return std::move(frame.aggregateCross->node);
        };
        const auto prepareOrdinaryConstructionTail =
            [&](OrdinaryConstructionFrameState& construction) -> bool
        {
            const ObjectConstructionBinding& binding = *construction.binding;
            if (!binding.selectedDefaultArguments.empty())
            {
                if (!binding.selectedConstructorMemberOrdinal.has_value() ||
                    memberModel_ == nullptr ||
                    *binding.selectedConstructorMemberOrdinal >=
                        memberModel_->members().size())
                {
                    error = "NSC-LIMIT-020 selected construction has no callable metadata";
                    return false;
                }
                const ClassMemberSymbol& selectedConstructor =
                    memberModel_->members()[*binding.selectedConstructorMemberOrdinal];
                for (std::size_t index = 0;
                    index < binding.selectedDefaultArguments.size(); ++index)
                {
                    if (index >=
                        binding.selectedDefaultArgumentParameterOrdinals.size())
                    {
                        error = "selected construction default has no parameter ordinal";
                        return false;
                    }
                    const std::size_t parameterIndex =
                        binding.selectedDefaultArgumentParameterOrdinals[index];
                    if (parameterIndex >= selectedConstructor.parameters.size())
                    {
                        error = "NSC-LIMIT-020 construction default has no parameter";
                        return false;
                    }
                    auto argument = lowerDefaultArgument(
                        binding.selectedDefaultArguments[index],
                        selectedConstructor.parameters[parameterIndex].typeKey,
                        error);
                    if (argument == nullptr) return false;
                    construction.node->children.push_back(std::move(argument));
                    construction.node->argumentParameterOrdinals.push_back(parameterIndex);
                }
            }
            if (binding.selectedSynthesizesEmptyParameterPack)
            {
                if (!binding.selectedConstructorMemberOrdinal.has_value() ||
                    memberModel_ == nullptr ||
                    *binding.selectedConstructorMemberOrdinal >=
                        memberModel_->members().size() ||
                    !binding.selectedParameterPackOrdinal.has_value())
                {
                    error =
                        "T01-F02 empty construction parameter pack has no constructor metadata";
                    return false;
                }
                const ClassMemberSymbol& constructor =
                    memberModel_->members()[*binding.selectedConstructorMemberOrdinal];
                const std::size_t parameterIndex = *binding.selectedParameterPackOrdinal;
                if (parameterIndex >= constructor.parameters.size())
                {
                    error = "T01-F02 empty construction parameter pack has no parameter";
                    return false;
                }
                const std::string& elementType =
                    constructor.parameters[parameterIndex].parameterPackElementTypeKey;
                if (elementType.empty())
                {
                    error =
                        "T01-F02 empty construction parameter pack lost its element type";
                    return false;
                }
                auto pack = makeNode(
                    ProjectOopHirNodeKind::ParameterPack,
                    construction.node->syntax,
                    constructor.parameters[parameterIndex].typeKey);
                pack->elementTypeKey = elementType;
                construction.node->children.push_back(std::move(pack));
                construction.node->argumentParameterOrdinals.push_back(parameterIndex);
            }
            construction.node->argumentCount =
                construction.node->argumentParameterOrdinals.size();

            if (binding.selectedImplicitDefault && memberModel_ != nullptr)
            {
                for (const std::size_t typeOrdinal : implicitInitializerTypes(
                         binding.targetTypeOrdinal,
                         binding.implicitDefaultBaseConstructorMemberOrdinal))
                {
                    for (const std::size_t memberOrdinal :
                         memberModel_->membersOfType(typeOrdinal))
                    {
                        if (memberOrdinal >= memberModel_->members().size()) continue;
                        const ClassMemberSymbol& field =
                            memberModel_->members()[memberOrdinal];
                        if (field.kind == ClassMemberKind::Field && !field.isStatic &&
                            field.syntax != nullptr && field.syntax->first() != nullptr)
                        {
                            construction.fieldMemberOrdinals.push_back(memberOrdinal);
                        }
                    }
                }
            }
            construction.stage = construction.fieldMemberOrdinals.empty()
                ? OrdinaryConstructionFrameStage::Ready
                : OrdinaryConstructionFrameStage::FieldInitializers;
            return true;
        };
        const auto initializeOrdinaryConstructionFrame =
            [&](IterativeExpressionFrame& frame) -> bool
        {
            const ObjectConstructionBinding* binding = constructionBinding(frame.syntax);
            if (binding == nullptr || constructionModel_ == nullptr ||
                (binding->resolution != ConstructionResolutionKind::ResolvedExplicit &&
                 binding->resolution != ConstructionResolutionKind::ResolvedImplicitDefault))
            {
                error = "Stage 34 requires a resolved Stage 28 object construction";
                return false;
            }
            if (binding->allocatorSelection &&
                binding->allocatorAllocateMemberOrdinal.has_value())
            {
                error = "Stage 34 ordinary construction captured allocator construction";
                return false;
            }

            const bool placement = frame.syntax->kind() ==
                ast::AstNodeKind::PlacementConstructionExpression;
            OrdinaryConstructionFrameState construction;
            construction.binding = binding;
            construction.node = makeNode(
                placement
                    ? ProjectOopHirNodeKind::PlacementConstruction
                    : binding->targetIsStruct
                    ? ProjectOopHirNodeKind::ValueConstruction
                    : ProjectOopHirNodeKind::ObjectConstruction,
                frame.syntax,
                placement && binding->targetIsStruct
                    ? binding->targetTypeKey + "*"
                    : binding->targetTypeKey);
            construction.node->constructionOrdinal = static_cast<std::size_t>(
                binding - constructionModel_->constructions().data());
            if (placement)
            {
                construction.node->objectLifetimeOwnership = sema::OwnershipKind::Owned;
                construction.node->storageOwnership = sema::OwnershipKind::Borrowed;
            }
            else if (!binding->targetIsStruct)
            {
                construction.node->objectLifetimeOwnership = sema::OwnershipKind::Owned;
                construction.node->storageOwnership = sema::OwnershipKind::Owned;
            }
            construction.node->targetTypeOrdinal = binding->targetTypeOrdinal;
            construction.node->memberOrdinal =
                binding->selectedConstructorMemberOrdinal.has_value()
                ? binding->selectedConstructorMemberOrdinal
                : binding->implicitDefaultBaseConstructorMemberOrdinal;
            construction.node->argumentCount = 0U;
            if (placement)
            {
                construction.stage = OrdinaryConstructionFrameStage::Storage;
            }
            else if (!frame.syntax->children().empty())
            {
                construction.stage = OrdinaryConstructionFrameStage::Arguments;
            }
            else if (!prepareOrdinaryConstructionTail(construction))
            {
                return false;
            }
            frame.ordinaryConstruction.emplace(std::move(construction));
            return true;
        };
        struct OrdinaryConstructionChild final
        {
            const ast::AstNode* syntax = nullptr;
        };
        const auto nextOrdinaryConstructionChild =
            [&](const OrdinaryConstructionFrameState& construction)
                -> std::optional<OrdinaryConstructionChild>
        {
            if (construction.stage == OrdinaryConstructionFrameStage::Storage)
            {
                return OrdinaryConstructionChild{
                    construction.node->syntax->first()};
            }
            if (construction.stage == OrdinaryConstructionFrameStage::Arguments &&
                construction.nextArgument <
                    construction.node->syntax->children().size())
            {
                return OrdinaryConstructionChild{
                    construction.node->syntax->children()[construction.nextArgument]};
            }
            if (construction.stage ==
                    OrdinaryConstructionFrameStage::FieldInitializers &&
                construction.nextFieldInitializer <
                    construction.fieldMemberOrdinals.size() && memberModel_ != nullptr)
            {
                const std::size_t memberOrdinal =
                    construction.fieldMemberOrdinals[
                        construction.nextFieldInitializer];
                if (memberOrdinal < memberModel_->members().size())
                {
                    const ClassMemberSymbol& field =
                        memberModel_->members()[memberOrdinal];
                    return OrdinaryConstructionChild{field.syntax->first()};
                }
            }
            return std::nullopt;
        };
        const auto acceptOrdinaryConstructionChild =
            [&](OrdinaryConstructionFrameState& construction,
                std::unique_ptr<ProjectOopHirNode> child) -> bool
        {
            if (construction.stage == OrdinaryConstructionFrameStage::Storage)
            {
                construction.node->children.push_back(std::move(child));
                if (!construction.node->syntax->children().empty())
                {
                    construction.stage = OrdinaryConstructionFrameStage::Arguments;
                    return true;
                }
                return prepareOrdinaryConstructionTail(construction);
            }
            if (construction.stage == OrdinaryConstructionFrameStage::Arguments)
            {
                const std::size_t index = construction.nextArgument++;
                const ast::AstNode* argumentSyntax =
                    construction.node->syntax->children()[index];
                if (index < construction.binding->selectedConversions.size() &&
                    construction.binding->selectedConversions[index].kind !=
                        ProjectConversionKind::Identity)
                {
                    child = wrapImplicitConversion(
                        std::move(child),
                        construction.binding->selectedConversions[index],
                        argumentSyntax);
                }
                if (index >=
                    construction.binding->selectedArgumentParameterOrdinals.size())
                {
                    error = "named construction argument has no selected parameter ordinal";
                    return false;
                }
                const std::size_t parameterIndex =
                    construction.binding->selectedArgumentParameterOrdinals[index];
                const bool packElement =
                    index < construction.binding->selectedParameterPackElements.size() &&
                    construction.binding->selectedParameterPackElements[index];
                if (packElement)
                {
                    if (!construction.binding->selectedConstructorMemberOrdinal.has_value() ||
                        memberModel_ == nullptr ||
                        *construction.binding->selectedConstructorMemberOrdinal >=
                            memberModel_->members().size())
                    {
                        error =
                            "T01-F02 construction parameter pack has no constructor metadata";
                        return false;
                    }
                    const ClassMemberSymbol& constructor =
                        memberModel_->members()[
                            *construction.binding->selectedConstructorMemberOrdinal];
                    if (parameterIndex >= constructor.parameters.size())
                    {
                        error = "T01-F02 construction parameter pack has no parameter";
                        return false;
                    }
                    const std::string& elementType =
                        constructor.parameters[parameterIndex].parameterPackElementTypeKey;
                    if (elementType.empty())
                    {
                        error =
                            "T01-F02 construction parameter pack lost its element type";
                        return false;
                    }
                    if (!construction.expandedPackChildIndex.has_value())
                    {
                        auto pack = makeNode(
                            ProjectOopHirNodeKind::ParameterPack,
                            argumentSyntax,
                            constructor.parameters[parameterIndex].typeKey);
                        pack->elementTypeKey = elementType;
                        construction.expandedPackChildIndex =
                            construction.node->children.size();
                        construction.node->children.push_back(std::move(pack));
                        construction.node->argumentParameterOrdinals.push_back(
                            parameterIndex);
                    }
                    construction.node
                        ->children[*construction.expandedPackChildIndex]
                        ->children.push_back(std::move(child));
                }
                else
                {
                    if (std::ranges::find(
                            construction.binding
                                ->selectedFullExpressionBorrowArgumentOrdinals,
                            index) != construction.binding
                                ->selectedFullExpressionBorrowArgumentOrdinals.end())
                    {
                        child = wrapFreshOwnedTemporaryBorrow(
                            std::move(child), argumentSyntax,
                            construction.hasOwnedTemporary);
                    }
                    construction.node->children.push_back(std::move(child));
                    construction.node->argumentParameterOrdinals.push_back(
                        parameterIndex);
                }
                if (construction.nextArgument ==
                    construction.node->syntax->children().size())
                {
                    return prepareOrdinaryConstructionTail(construction);
                }
                return true;
            }
            if (construction.stage ==
                OrdinaryConstructionFrameStage::FieldInitializers)
            {
                if (memberModel_ == nullptr || inheritanceModel_ == nullptr ||
                    construction.nextFieldInitializer >=
                        construction.fieldMemberOrdinals.size())
                {
                    error = "Stage 37 cannot lower an invalid instance field initializer";
                    return false;
                }
                const std::size_t fieldMemberOrdinal =
                    construction.fieldMemberOrdinals[
                        construction.nextFieldInitializer++];
                if (fieldMemberOrdinal >= memberModel_->members().size())
                {
                    error = "Stage 37 cannot lower an invalid instance field initializer";
                    return false;
                }
                const ClassMemberSymbol& field =
                    memberModel_->members()[fieldMemberOrdinal];
                if (field.kind != ClassMemberKind::Field || field.isStatic ||
                    field.syntax == nullptr || field.syntax->first() == nullptr)
                {
                    error =
                        "Stage 37 instance initializer helper received a non-instance field";
                    return false;
                }
                const MethodExpressionTypeBinding* sourceType =
                    expressionType(field.syntax->first());
                const ProjectImplicitConversion conversion = classifyImplicit(
                    child->typeKey,
                    field.declaredTypeKey,
                    sourceType != nullptr
                        ? sourceType->integerConstant
                        : std::nullopt);
                if (!conversion.exists())
                {
                    error =
                        "Stage 37 field initializer lost its validated implicit conversion";
                    return false;
                }
                if (conversion.kind != ProjectConversionKind::Identity)
                {
                    child = wrapImplicitConversion(
                        std::move(child), conversion, field.syntax->first());
                }
                auto target = makeNode(
                    ProjectOopHirNodeKind::FieldAddress,
                    field.syntax,
                    field.declaredTypeKey);
                target->memberOrdinal = fieldMemberOrdinal;
                target->volatileAccess = field.isVolatile;
                target->children.push_back(makeNode(
                    ProjectOopHirNodeKind::ThisValue,
                    field.syntax,
                    inheritanceModel_->typeKeyForOrdinal(
                        field.declaringTypeOrdinal)));
                auto assignment = makeNode(
                    ProjectOopHirNodeKind::Assignment,
                    field.syntax,
                    field.declaredTypeKey);
                assignment->operation = lexer::TokenKind::Equal;
                assignment->children.push_back(std::move(target));
                assignment->children.push_back(std::move(child));
                construction.node->children.push_back(std::move(assignment));
                if (construction.nextFieldInitializer ==
                    construction.fieldMemberOrdinals.size())
                {
                    construction.stage = OrdinaryConstructionFrameStage::Ready;
                }
                return true;
            }
            error = "Stage 34 ordinary-construction frame accepted an unexpected child";
            return false;
        };
        const auto finishOrdinaryConstructionFrame =
            [&](IterativeExpressionFrame& frame)
                -> std::unique_ptr<ProjectOopHirNode>
        {
            OrdinaryConstructionFrameState& construction =
                *frame.ordinaryConstruction;
            return wrapFullExpressionCleanup(
                std::move(construction.node),
                frame.syntax,
                construction.hasOwnedTemporary);
        };
        const auto initializeAllocatorConstructionFrame =
            [&](IterativeExpressionFrame& frame) -> bool
        {
            const ObjectConstructionBinding* binding = constructionBinding(frame.syntax);
            if (binding == nullptr ||
                (binding->resolution != ConstructionResolutionKind::ResolvedExplicit &&
                 binding->resolution != ConstructionResolutionKind::ResolvedImplicitDefault) ||
                !binding->allocatorSelection ||
                !binding->allocatorAllocateMemberOrdinal.has_value() ||
                constructionModel_ == nullptr)
            {
                error = "Stage 34 requires a resolved Stage 28 object construction";
                return false;
            }

            AllocatorConstructionFrameState construction;
            construction.binding = binding;
            construction.constructionOrdinal = static_cast<std::size_t>(
                binding - constructionModel_->constructions().data());
            if (objectLayoutModel_ != nullptr)
            {
                if (binding->targetIsStruct)
                {
                    const auto* layout = objectLayoutModel_->structLayoutForType(
                        binding->targetTypeOrdinal);
                    if (layout != nullptr) construction.computedSize = layout->size;
                }
                else
                {
                    const auto* layout = objectLayoutModel_->layoutForType(
                        binding->targetTypeOrdinal);
                    if (layout != nullptr) construction.computedSize = layout->size;
                }
            }
            const bool trailingConstruction =
                frame.syntax->auxiliaryText() == "trailing";
            if (trailingConstruction && !frame.syntax->children().empty())
            {
                const MethodExpressionTypeBinding* capacityType =
                    expressionType(frame.syntax->children().back());
                if (capacityType != nullptr && capacityType->integerConstant.has_value())
                {
                    construction.computedSize += static_cast<std::size_t>(
                        capacityType->integerConstant->value);
                }
            }
            construction.temporaryOrdinal = nextSyntheticLocalOrdinal_++;
            construction.contextOrdinal = nextSyntheticLocalOrdinal_++;
            construction.receiverSyntax = frame.syntax->first();

            if (inheritanceModel_ != nullptr && nameLookup_ != nullptr)
            {
                construction.allocatorTypeOrdinal =
                    inheritanceModel_->typeOrdinalForTypeKey(binding->allocatorTypeKey);
                if (construction.allocatorTypeOrdinal.has_value() &&
                    *construction.allocatorTypeOrdinal < nameLookup_->namedTypes().size())
                {
                    construction.allocatorIsStruct =
                        nameLookup_->namedTypes()[*construction.allocatorTypeOrdinal].kind ==
                        NamedTypeDeclarationKind::Struct;
                }
            }

            const std::size_t argumentCount = frame.syntax->children().size() -
                (trailingConstruction ? 1U : 0U);
            construction.argumentSyntaxes.assign(
                frame.syntax->children().begin(),
                frame.syntax->children().begin() + argumentCount);
            construction.arguments.reserve(argumentCount);
            construction.argumentParameterOrdinals.reserve(argumentCount);
            frame.allocatorConstruction.emplace(std::move(construction));
            return true;
        };
        struct AllocatorConstructionChild final
        {
            const ast::AstNode* syntax = nullptr;
            bool addressContext = false;
        };
        const auto nextAllocatorConstructionChild =
            [](const AllocatorConstructionFrameState& construction)
                -> std::optional<AllocatorConstructionChild>
        {
            if (construction.stage == AllocatorConstructionFrameStage::Receiver)
            {
                return AllocatorConstructionChild{
                    construction.receiverSyntax,
                    construction.allocatorIsStruct};
            }
            if (construction.stage != AllocatorConstructionFrameStage::Arguments ||
                construction.nextArgument >= construction.argumentSyntaxes.size())
            {
                return std::nullopt;
            }
            return AllocatorConstructionChild{
                construction.argumentSyntaxes[construction.nextArgument], false};
        };
        const auto acceptAllocatorConstructionChild =
            [&](AllocatorConstructionFrameState& construction,
                std::unique_ptr<ProjectOopHirNode> child) -> bool
        {
            if (construction.stage == AllocatorConstructionFrameStage::Receiver)
            {
                construction.receiver = std::move(child);
                construction.stage = construction.argumentSyntaxes.empty()
                    ? AllocatorConstructionFrameStage::Ready
                    : AllocatorConstructionFrameStage::Arguments;
                return true;
            }

            const std::size_t index = construction.nextArgument++;
            if (index < construction.binding->selectedConversions.size() &&
                construction.binding->selectedConversions[index].kind !=
                    ProjectConversionKind::Identity)
            {
                child = wrapImplicitConversion(
                    std::move(child),
                    construction.binding->selectedConversions[index],
                    construction.argumentSyntaxes[index]);
            }
            if (index >=
                construction.binding->selectedArgumentParameterOrdinals.size())
            {
                error = "named construction argument has no selected parameter ordinal";
                return false;
            }
            construction.arguments.push_back(std::move(child));
            construction.argumentParameterOrdinals.push_back(
                construction.binding->selectedArgumentParameterOrdinals[index]);
            if (construction.nextArgument == construction.argumentSyntaxes.size())
                construction.stage = AllocatorConstructionFrameStage::Ready;
            return true;
        };
        const auto finishAllocatorConstructionFrame =
            [&](IterativeExpressionFrame& frame) -> std::unique_ptr<ProjectOopHirNode>
        {
            AllocatorConstructionFrameState& construction =
                *frame.allocatorConstruction;
            return finishAllocatorConstructionExpression(
                frame.syntax,
                *construction.binding,
                construction.constructionOrdinal,
                construction.computedSize,
                construction.temporaryOrdinal,
                construction.contextOrdinal,
                construction.allocatorTypeOrdinal,
                std::move(construction.receiver),
                std::move(construction.arguments),
                std::move(construction.argumentParameterOrdinals),
                error);
        };
        const auto initializeNamespaceCallFrame = [&](IterativeExpressionFrame& frame) -> bool
        {
            const NamespaceFunctionCallBinding* binding =
                namespaceFunctionCallBinding(frame.syntax);
            if (binding == nullptr ||
                binding->resolution != MethodCallResolutionKind::Resolved ||
                !binding->selectedFunctionOrdinal.has_value() ||
                namespaceFunctionModel_ == nullptr ||
                *binding->selectedFunctionOrdinal >= namespaceFunctionModel_->functions().size())
            {
                error = "namespace function call reached HIR without a resolved target";
                return false;
            }
            NamespaceCallFrameState call;
            call.binding = binding;
            const std::size_t functionOrdinal = *binding->selectedFunctionOrdinal;
            call.target = &namespaceFunctionModel_->functions()[functionOrdinal];
            if (frame.addressContext && !call.target->isReferenceReturn)
            {
                error = "namespace function call result is not addressable unless it returns ref";
                return false;
            }
            call.node = makeNode(
                ProjectOopHirNodeKind::StaticCall,
                frame.syntax,
                binding->resultTypeKey);
            call.node->namespaceFunctionOrdinal = functionOrdinal;
            call.node->stringValue = call.target->mangledName;
            call.targetCallableOrdinal =
                (memberModel_ != nullptr ? memberModel_->members().size() : 0U) +
                functionOrdinal;
            if (binding->forwardedReceiver)
            {
                if (binding->receiverSyntax == nullptr || call.target->parameters.empty() ||
                    binding->selectedArgumentParameterOrdinals.empty())
                {
                    error = "extension call lost its synthetic receiver contract";
                    return false;
                }
                call.receiverParameterOrdinal =
                    binding->selectedArgumentParameterOrdinals.front();
                if (call.receiverParameterOrdinal >= call.target->parameters.size())
                {
                    error = "extension receiver has no matching parameter";
                    return false;
                }
                call.receiverNeedsAddress =
                    call.target->parameters[call.receiverParameterOrdinal].passKind !=
                    ParameterPassKind::Value;
                call.argumentOffset = 1U;
                call.stage = NamespaceCallFrameStage::Receiver;
            }
            else
            {
                call.stage = frame.syntax->children().empty()
                    ? NamespaceCallFrameStage::Ready
                    : NamespaceCallFrameStage::Arguments;
            }
            frame.namespaceCall.emplace(std::move(call));
            return true;
        };
        struct NamespaceCallChild final
        {
            const ast::AstNode* syntax = nullptr;
            bool addressContext = false;
        };
        const auto nextNamespaceCallChild = [&](const NamespaceCallFrameState& call)
            -> std::optional<NamespaceCallChild>
        {
            if (call.stage == NamespaceCallFrameStage::Receiver)
            {
                return NamespaceCallChild{
                    call.binding->receiverSyntax,
                    call.receiverNeedsAddress};
            }
            if (call.stage != NamespaceCallFrameStage::Arguments ||
                call.nextArgument >= call.node->syntax->children().size())
            {
                return std::nullopt;
            }
            return NamespaceCallChild{
                call.node->syntax->children()[call.nextArgument],
                false};
        };
        const auto acceptNamespaceCallChild = [&](NamespaceCallFrameState& call,
                                                   std::unique_ptr<ProjectOopHirNode> child)
            -> bool
        {
            if (call.stage == NamespaceCallFrameStage::Receiver)
            {
                if (!call.receiverNeedsAddress && !call.binding->selectedConversions.empty() &&
                    call.binding->selectedConversions.front().kind !=
                        ProjectConversionKind::Identity)
                {
                    child = wrapImplicitConversion(
                        std::move(child),
                        call.binding->selectedConversions.front(),
                        call.binding->receiverSyntax);
                }
                child = wrapCallArgumentBorrow(
                    std::move(child),
                    call.binding->receiverSyntax,
                    call.targetCallableOrdinal,
                    call.receiverParameterOrdinal,
                    call.hasOwnedTemporary);
                call.node->children.push_back(std::move(child));
                call.node->argumentParameterOrdinals.push_back(
                    call.receiverParameterOrdinal);
                call.node->argumentTypeKeys.push_back(
                    call.target->parameters[call.receiverParameterOrdinal].typeKey);
                call.stage = call.node->syntax->children().empty()
                    ? NamespaceCallFrameStage::Ready
                    : NamespaceCallFrameStage::Arguments;
                return true;
            }

            const std::size_t index = call.nextArgument++;
            const std::size_t selectedIndex = index + call.argumentOffset;
            if (selectedIndex >= call.binding->selectedArgumentParameterOrdinals.size())
            {
                error = "namespace function argument has no selected parameter ordinal";
                return false;
            }
            const std::size_t parameterOrdinal =
                call.binding->selectedArgumentParameterOrdinals[selectedIndex];
            if (parameterOrdinal >= call.target->parameters.size())
            {
                error = "namespace function argument has no matching parameter";
                return false;
            }
            const ast::AstNode* argumentSyntax = call.node->syntax->children()[index];
            if (selectedIndex < call.binding->selectedConversions.size() &&
                call.binding->selectedConversions[selectedIndex].kind !=
                    ProjectConversionKind::Identity)
            {
                child = wrapImplicitConversion(
                    std::move(child),
                    call.binding->selectedConversions[selectedIndex],
                    argumentSyntax);
            }
            const bool packElement =
                selectedIndex < call.binding->selectedParameterPackElements.size() &&
                call.binding->selectedParameterPackElements[selectedIndex];
            if (packElement)
            {
                const std::string& elementType =
                    call.target->parameters[parameterOrdinal].parameterPackElementTypeKey;
                if (elementType.empty())
                {
                    error = "T01-F02 namespace parameter pack lost its element type";
                    return false;
                }
                if (call.expandedPack == nullptr)
                {
                    auto pack = makeNode(
                        ProjectOopHirNodeKind::ParameterPack,
                        argumentSyntax,
                        call.target->parameters[parameterOrdinal].typeKey);
                    pack->elementTypeKey = elementType;
                    call.expandedPack = pack.get();
                    call.node->children.push_back(std::move(pack));
                    call.node->argumentParameterOrdinals.push_back(parameterOrdinal);
                    call.node->argumentTypeKeys.push_back(
                        call.target->parameters[parameterOrdinal].typeKey);
                }
                call.expandedPack->children.push_back(std::move(child));
            }
            else
            {
                child = wrapCallArgumentBorrow(
                    std::move(child),
                    argumentSyntax,
                    call.targetCallableOrdinal,
                    parameterOrdinal,
                    call.hasOwnedTemporary);
                call.node->children.push_back(std::move(child));
                call.node->argumentParameterOrdinals.push_back(parameterOrdinal);
                call.node->argumentTypeKeys.push_back(
                    call.target->parameters[parameterOrdinal].typeKey);
            }
            if (call.nextArgument == call.node->syntax->children().size())
                call.stage = NamespaceCallFrameStage::Ready;
            return true;
        };
        const auto initializeEventInvokeFrame = [&](IterativeExpressionFrame& frame) -> bool
        {
            const MethodEventBinding* binding = eventBinding(frame.syntax);
            if (frame.addressContext)
            {
                error = "Stage 42 event invocation is not addressable";
                return false;
            }
            if (binding == nullptr || binding->kind != MethodEventOperationKind::Invoke ||
                memberModel_ == nullptr ||
                binding->backingMemberOrdinal >= memberModel_->members().size())
            {
                error = "Stage 42 event binding has invalid backing member";
                return false;
            }
            EventInvokeFrameState call;
            call.binding = binding;
            call.node = makeNode(
                ProjectOopHirNodeKind::EventInvoke,
                frame.syntax,
                binding->resultTypeKey);
            call.node->elementTypeKey = binding->delegateTypeKey;
            call.node->memberOrdinal = binding->backingMemberOrdinal;
            if (binding->staticEvent)
            {
                auto backing = lowerEventBackingField(
                    frame.syntax, frame.memberOrdinal, error, binding, false);
                if (backing == nullptr) return false;
                call.node->children.push_back(std::move(backing));
                call.stage = frame.syntax->children().empty()
                    ? EventInvokeFrameStage::Ready
                    : EventInvokeFrameStage::Arguments;
            }
            else
            {
                const std::optional<bool> receiverReady = initializeReceiverFrame(
                    call.receiver,
                    binding->receiverSyntax,
                    binding->receiverTypeKey);
                if (!receiverReady.has_value()) return false;
                if (*receiverReady)
                {
                    auto backing = lowerEventBackingField(
                        frame.syntax,
                        frame.memberOrdinal,
                        error,
                        binding,
                        false,
                        std::move(call.receiver.lowered));
                    if (backing == nullptr) return false;
                    call.node->children.push_back(std::move(backing));
                    call.stage = frame.syntax->children().empty()
                        ? EventInvokeFrameStage::Ready
                        : EventInvokeFrameStage::Arguments;
                }
                else
                {
                    call.stage = EventInvokeFrameStage::Receiver;
                }
            }
            frame.eventInvoke.emplace(std::move(call));
            return true;
        };
        struct EventInvokeChild final
        {
            const ast::AstNode* syntax = nullptr;
            bool addressContext = false;
        };
        const auto nextEventInvokeChild = [&](const EventInvokeFrameState& call)
            -> std::optional<EventInvokeChild>
        {
            if (call.stage == EventInvokeFrameStage::Receiver)
            {
                return EventInvokeChild{
                    call.receiver.syntax,
                    isStructValueType(call.receiver.typeKey) &&
                        !call.receiver.materializeTemporary};
            }
            if (call.stage != EventInvokeFrameStage::Arguments ||
                call.nextArgument >= call.node->syntax->children().size())
            {
                return std::nullopt;
            }
            return EventInvokeChild{
                call.node->syntax->children()[call.nextArgument],
                false};
        };
        const auto acceptEventInvokeChild = [&](IterativeExpressionFrame& frame,
                                                std::unique_ptr<ProjectOopHirNode> child)
            -> bool
        {
            EventInvokeFrameState& call = *frame.eventInvoke;
            if (call.stage == EventInvokeFrameStage::Receiver)
            {
                acceptReceiverChild(call.receiver, std::move(child));
                auto backing = lowerEventBackingField(
                    frame.syntax,
                    frame.memberOrdinal,
                    error,
                    call.binding,
                    false,
                    std::move(call.receiver.lowered));
                if (backing == nullptr) return false;
                call.node->children.push_back(std::move(backing));
                call.stage = frame.syntax->children().empty()
                    ? EventInvokeFrameStage::Ready
                    : EventInvokeFrameStage::Arguments;
                return true;
            }
            const std::size_t index = call.nextArgument++;
            const ast::AstNode* argumentSyntax = call.node->syntax->children()[index];
            if (index < call.binding->argumentConversions.size())
            {
                const ProjectImplicitConversion& conversion =
                    call.binding->argumentConversions[index];
                call.node->argumentTypeKeys.push_back(conversion.targetTypeKey);
                if (conversion.kind != ProjectConversionKind::Identity)
                {
                    child = wrapImplicitConversion(
                        std::move(child), conversion, argumentSyntax);
                }
            }
            call.node->children.push_back(std::move(child));
            if (call.nextArgument == call.node->syntax->children().size())
                call.stage = EventInvokeFrameStage::Ready;
            return true;
        };
        const auto initializeDelegateInvokeFrame = [&](IterativeExpressionFrame& frame) -> bool
        {
            const MethodDelegateBinding* binding = delegateBinding(frame.syntax);
            if (frame.addressContext)
            {
                error = "Stage 42 delegate invocation is not addressable";
                return false;
            }
            if (binding == nullptr || binding->kind != MethodDelegateOperationKind::Invoke)
            {
                error = "Stage 34 delegate invocation lost its semantic binding";
                return false;
            }
            DelegateInvokeFrameState call;
            call.binding = binding;
            call.node = makeNode(
                ProjectOopHirNodeKind::DelegateInvoke,
                frame.syntax,
                binding->resultTypeKey);
            call.node->elementTypeKey = binding->delegateTypeKey;
            call.node->stringValue = binding->directLambdaName;
            call.node->booleanValue = binding->directLambdaUsesEnvironment;
            const bool lowersCallee =
                binding->directLambdaName.empty() || binding->directLambdaUsesEnvironment;
            call.stage = lowersCallee
                ? DelegateInvokeFrameStage::Callee
                : frame.syntax->children().empty()
                    ? DelegateInvokeFrameStage::Ready
                    : DelegateInvokeFrameStage::Arguments;
            frame.delegateInvoke.emplace(std::move(call));
            return true;
        };
        struct DelegateInvokeChild final
        {
            const ast::AstNode* syntax = nullptr;
        };
        const auto nextDelegateInvokeChild = [&](const IterativeExpressionFrame& frame)
            -> std::optional<DelegateInvokeChild>
        {
            const DelegateInvokeFrameState& call = *frame.delegateInvoke;
            if (call.stage == DelegateInvokeFrameStage::Callee)
                return DelegateInvokeChild{frame.syntax->first()};
            if (call.stage != DelegateInvokeFrameStage::Arguments ||
                call.nextArgument >= frame.syntax->children().size())
            {
                return std::nullopt;
            }
            return DelegateInvokeChild{
                frame.syntax->children()[call.nextArgument]};
        };
        const auto acceptDelegateInvokeChild = [&](IterativeExpressionFrame& frame,
                                                   std::unique_ptr<ProjectOopHirNode> child)
        {
            DelegateInvokeFrameState& call = *frame.delegateInvoke;
            if (call.stage == DelegateInvokeFrameStage::Callee)
            {
                call.node->children.push_back(std::move(child));
                call.stage = frame.syntax->children().empty()
                    ? DelegateInvokeFrameStage::Ready
                    : DelegateInvokeFrameStage::Arguments;
                return;
            }
            const std::size_t index = call.nextArgument++;
            const ast::AstNode* argumentSyntax = frame.syntax->children()[index];
            if (index < call.binding->argumentConversions.size())
            {
                const ProjectImplicitConversion& conversion =
                    call.binding->argumentConversions[index];
                call.node->argumentTypeKeys.push_back(conversion.targetTypeKey);
                if (conversion.kind != ProjectConversionKind::Identity)
                {
                    child = wrapImplicitConversion(
                        std::move(child), conversion, argumentSyntax);
                }
            }
            call.node->children.push_back(std::move(child));
            if (call.nextArgument == frame.syntax->children().size())
                call.stage = DelegateInvokeFrameStage::Ready;
        };
        const auto initializeAssertionCoreFrame = [&](IterativeExpressionFrame& frame) -> bool
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
        };
        const auto initializeIteratorCoreCallFrame = [&](IterativeExpressionFrame& frame) -> bool
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
        };
        const auto initializeCoreOperandCallFrame = [&](IterativeExpressionFrame& frame) -> bool
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
            }
            else
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
            }
            frame.coreOperandCall.emplace(std::move(call));
            return true;
        };
        struct CoreOperandCallChild final
        {
            const ast::AstNode* syntax = nullptr;
            bool addressContext = false;
        };
        const auto nextCoreOperandCallChild = [](const CoreOperandCallFrameState& call)
            -> std::optional<CoreOperandCallChild>
        {
            if (call.nextOperand >= call.operandSyntaxes.size()) return std::nullopt;
            return CoreOperandCallChild{
                call.operandSyntaxes[call.nextOperand],
                call.operandAddressContexts[call.nextOperand]};
        };
        const auto acceptCoreOperandCallChild = [&](IterativeExpressionFrame& frame,
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
        };
        const auto initializeAtomicCoreCallFrame = [&](IterativeExpressionFrame& frame) -> bool
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
        };
        struct AtomicCoreCallChild final
        {
            const ast::AstNode* syntax = nullptr;
        };
        const auto nextAtomicCoreCallChild = [](const IterativeExpressionFrame& frame)
            -> std::optional<AtomicCoreCallChild>
        {
            const AtomicCoreCallFrameState& call = *frame.atomicCoreCall;
            if (call.stage == AtomicCoreCallFrameStage::Location)
                return AtomicCoreCallChild{call.locationSyntax};
            if (call.stage == AtomicCoreCallFrameStage::Values &&
                call.nextValue < call.plan.valueArgumentCount)
            {
                return AtomicCoreCallChild{
                    frame.syntax->children()[call.argumentOffset + call.nextValue]};
            }
            return std::nullopt;
        };
        const auto acceptAtomicCoreCallChild = [&](IterativeExpressionFrame& frame,
                                                   std::unique_ptr<ProjectOopHirNode> child)
            -> bool
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
                call.node = makeNode(call.plan.hirKind, frame.syntax, frame.core->resultTypeKey);
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
        };
        const auto initializeMonitorCoreCallFrame = [&](IterativeExpressionFrame& frame) -> bool
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
        };
        struct MonitorCoreCallChild final
        {
            const ast::AstNode* syntax = nullptr;
        };
        const auto nextMonitorCoreCallChild = [](const MonitorCoreCallFrameState& call)
            -> std::optional<MonitorCoreCallChild>
        {
            if (call.stage == MonitorCoreCallFrameStage::Receiver)
                return MonitorCoreCallChild{call.binding->receiverSyntax};
            if (call.stage == MonitorCoreCallFrameStage::Timeout)
                return MonitorCoreCallChild{call.timeoutSyntax};
            return std::nullopt;
        };
        const auto acceptMonitorCoreCallChild = [&](IterativeExpressionFrame& frame,
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
                call.node = makeNode(call.plan.hirKind, frame.syntax, frame.core->resultTypeKey);
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
            call.node = makeNode(call.plan.hirKind, frame.syntax, frame.core->resultTypeKey);
            call.node->memberOrdinal = *frame.core->implicitFieldMemberOrdinal;
            call.node->secondaryMemberOrdinal =
                *frame.core->secondaryImplicitFieldMemberOrdinal;
            call.node->children.push_back(std::move(call.receiver));
            call.node->children.push_back(std::move(child));
            call.stage = MonitorCoreCallFrameStage::Ready;
        };
        const auto initializeConsoleOutputCallFrame = [&](IterativeExpressionFrame& frame) -> bool
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
        };
        const auto initializeConsoleReadLineCallFrame = [&](IterativeExpressionFrame& frame) -> bool
        {
            frame.core = coreBinding(frame.syntax);
            if (frame.addressContext || frame.syntax->children().size() != 1U)
            {
                error = "NativeSharp.Core.Console.ReadLine requires one out status argument";
                return false;
            }
            return true;
        };
        const auto initializeEnumToStringCallFrame = [&](IterativeExpressionFrame& frame) -> bool
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
        };
        const auto initializeStructCloneCallFrame = [&](IterativeExpressionFrame& frame) -> bool
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
        };
        const auto initializeStructAggregateCallFrame = [&](IterativeExpressionFrame& frame) -> bool
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
        };
        const auto initializeEnvironmentExitCallFrame = [&](IterativeExpressionFrame& frame) -> bool
        {
            frame.core = coreBinding(frame.syntax);
            if (frame.addressContext || frame.syntax->children().size() != 1U)
            {
                error = "Stage 53.2 NativeSharp.Core.Environment.Exit requires one int argument";
                return false;
            }
            frame.environmentExitBinding = callBinding(frame.syntax);
            return true;
        };
        const auto prepareSetterReceiver = [&](IterativeExpressionFrame& frame)
            -> std::optional<bool>
        {
            const MethodCallBinding& binding = *frame.propertySetterBinding;
            if (binding.staticCall) return true;
            if (frame.callTarget.receiverTypeKey.empty())
            {
                error = "Stage 40 receiver has no semantic class/interface/struct type";
                return std::nullopt;
            }
            const ast::AstNode* receiver = frame.callTarget.receiverSyntax;
            if (receiver != nullptr && receiver->kind() != ast::AstNodeKind::BaseExpression)
            {
                frame.materializeReceiverTemporary =
                    isStructValueType(frame.callTarget.receiverTypeKey) &&
                    !isNaturallyAddressable(receiver);
                return false;
            }
            if (currentLambda_ != nullptr)
            {
                const std::optional<std::size_t> capture = currentLambdaThisCapture();
                if (!capture.has_value())
                {
                    error = "Stage 43 implicit instance receiver has no captured this";
                    return std::nullopt;
                }
                frame.first = makeNode(
                    isStructValueType(frame.callTarget.receiverTypeKey)
                        ? ProjectOopHirNodeKind::ClosureCaptureAddress
                        : ProjectOopHirNodeKind::ClosureCaptureValue,
                    receiver,
                    std::string(frame.callTarget.receiverTypeKey));
                frame.first->integerValue = *capture;
            }
            else
            {
                frame.first = makeNode(
                    ProjectOopHirNodeKind::ThisValue,
                    receiver,
                    std::string(frame.callTarget.receiverTypeKey));
            }
            return true;
        };
        const auto initializeVariableDeclarationFrame =
            [&](IterativeExpressionFrame& frame) -> bool
        {
            VariableDeclarationFrameState variable;
            variable.local = localSymbol(frame.syntax);
            if (variable.local == nullptr)
            {
                error = "Stage 34 cannot map local declaration to Stage 27 local symbol";
                return false;
            }
            const MethodLocalSymbol& local = *variable.local;
            variable.node = makeNode(
                ProjectOopHirNodeKind::LocalDeclaration,
                frame.syntax,
                local.typeKey);
            variable.node->stringValue = local.name;
            variable.node->referenceLocal = local.isReference;
            variable.node->referenceMutable = local.referenceMutable;
            variable.node->staticLocal = local.isStatic;
            variable.node->pinnedStorage = local.isPinned;
            variable.node->ownership = local.ownership;
            variable.node->automaticDestruction =
                local.isRaii || local.isScope || local.requiresValueDrop;
            variable.node->objectLifetimeOwnership = local.objectLifetimeOwnership;
            variable.node->storageOwnership = local.storageOwnership;
            variable.node->localOrdinal = static_cast<std::size_t>(
                variable.local - methodBodyModel_->locals().data());
            if (!local.isStatic &&
                (local.isRaii || local.isScope || local.requiresValueDrop))
            {
                const MethodCleanupRegion* region = cleanupRegion(frame.syntax);
                if (region == nullptr)
                {
                    error = "drop-bearing local is missing its lexical cleanup region";
                    return false;
                }
                variable.node->cleanupRegionOrdinal = cleanupRegionOrdinal(region);
                if (!variable.node->cleanupRegionOrdinal.has_value())
                {
                    error = "drop-bearing local cleanup region has no stable ordinal";
                    return false;
                }
            }
            if (local.isVariableLengthArray)
            {
                if (local.variableLengthArrayCountSyntax == nullptr ||
                    local.variableLengthArrayElementTypeKey.empty())
                {
                    error =
                        "NativeSharp 2.0 VLA local lost its element type or runtime extent";
                    return false;
                }
                variable.node->variableLengthArray = true;
                variable.node->variableLengthArrayRank = std::max<std::size_t>(
                    1U, local.variableLengthArrayExtentSyntaxes.size());
                variable.node->elementTypeKey =
                    local.variableLengthArrayElementTypeKey;
                if (local.variableLengthArrayExtentSyntaxes.size() > 1U)
                {
                    if (local.variableLengthArrayExtentLocalOrdinals.size() !=
                        local.variableLengthArrayExtentSyntaxes.size())
                    {
                        error = "multidimensional VLA lost its captured extent locals";
                        return false;
                    }
                    variable.vlaBlock = makeNode(
                        ProjectOopHirNodeKind::Block,
                        frame.syntax,
                        "builtin:void");
                    variable.mode = VariableDeclarationFrameMode::VlaExtents;
                }
                else
                {
                    variable.mode = VariableDeclarationFrameMode::VlaSingle;
                }
            }
            else if (frame.syntax->first() != nullptr)
            {
                variable.mode = VariableDeclarationFrameMode::Initializer;
            }
            else
            {
                variable.mode = VariableDeclarationFrameMode::NoChild;
            }
            frame.variableDeclaration.emplace(std::move(variable));
            return true;
        };
        struct VariableDeclarationChild final
        {
            const ast::AstNode* syntax = nullptr;
            bool addressContext = false;
        };
        const auto nextVariableDeclarationChild =
            [&](const IterativeExpressionFrame& frame)
                -> std::optional<VariableDeclarationChild>
        {
            const VariableDeclarationFrameState& variable =
                *frame.variableDeclaration;
            if (variable.mode == VariableDeclarationFrameMode::Initializer)
                return VariableDeclarationChild{
                    frame.syntax->first(), variable.local->isReference};
            if (variable.mode == VariableDeclarationFrameMode::VlaSingle)
                return VariableDeclarationChild{
                    variable.local->variableLengthArrayCountSyntax, false};
            if (variable.mode == VariableDeclarationFrameMode::VlaExtents)
                return VariableDeclarationChild{
                    variable.local->variableLengthArrayExtentSyntaxes[
                        variable.nextExtent],
                    false};
            return std::nullopt;
        };
        const auto completeVariableAllocation =
            [&](IterativeExpressionFrame& frame)
        {
            VariableDeclarationFrameState& variable =
                *frame.variableDeclaration;
            auto allocation = makeNode(
                ProjectOopHirNodeKind::StackAllocation,
                frame.syntax,
                variable.local->typeKey);
            allocation->elementTypeKey =
                variable.local->variableLengthArrayElementTypeKey;
            for (auto& count : variable.allocationCounts)
                allocation->children.push_back(std::move(count));
            variable.node->children.push_back(std::move(allocation));
            if (variable.vlaBlock != nullptr)
                variable.vlaBlock->children.push_back(std::move(variable.node));
            variable.mode = VariableDeclarationFrameMode::Ready;
        };
        const auto acceptVariableDeclarationChild =
            [&](IterativeExpressionFrame& frame,
                std::unique_ptr<ProjectOopHirNode> child) -> bool
        {
            VariableDeclarationFrameState& variable =
                *frame.variableDeclaration;
            const MethodLocalSymbol& local = *variable.local;
            if (variable.mode == VariableDeclarationFrameMode::Initializer)
            {
                variable.node->placementConstructed =
                    child->kind == ProjectOopHirNodeKind::PlacementConstruction;
                const ProjectImplicitConversion conversion = local.isReference
                    ? ProjectImplicitConversion{
                        ProjectConversionKind::Identity, 0, local.typeKey,
                        local.typeKey, std::nullopt}
                    : classifyImplicit(
                        child->typeKey,
                        local.typeKey,
                        expressionType(frame.syntax->first()) != nullptr
                            ? expressionType(frame.syntax->first())->integerConstant
                            : std::nullopt);
                if (conversion.exists() &&
                    conversion.kind != ProjectConversionKind::Identity)
                {
                    child = wrapImplicitConversion(
                        std::move(child), conversion, frame.syntax->first());
                }
                if (local.ownership == sema::OwnershipKind::Borrowed &&
                    local.localBorrowBindingOrdinal.has_value() &&
                    *local.localBorrowBindingOrdinal <
                        methodBodyModel_->localBorrowBindings().size())
                {
                    const MethodLocalBorrowBinding& borrow =
                        methodBodyModel_->localBorrowBindings()[
                            *local.localBorrowBindingOrdinal];
                    auto borrowed = makeNode(
                        ProjectOopHirNodeKind::BorrowValue,
                        frame.syntax->first(),
                        local.typeKey);
                    borrowed->localBorrowSourceKind = borrow.sourceKind;
                    borrowed->localOrdinal = borrow.sourceLocalOrdinal;
                    borrowed->parameterOrdinal = borrow.sourceParameterOrdinal;
                    borrowed->children.push_back(std::move(child));
                    child = std::move(borrowed);
                }
                variable.node->children.push_back(std::move(child));
                variable.mode = VariableDeclarationFrameMode::Ready;
                return true;
            }
            if (variable.mode == VariableDeclarationFrameMode::VlaSingle)
            {
                variable.allocationCounts.push_back(std::move(child));
                completeVariableAllocation(frame);
                return true;
            }
            if (variable.mode == VariableDeclarationFrameMode::VlaExtents)
            {
                const std::size_t dimension = variable.nextExtent;
                const std::size_t extentOrdinal =
                    local.variableLengthArrayExtentLocalOrdinals[dimension];
                const std::string& extentType =
                    methodBodyModel_->locals()[extentOrdinal].typeKey;
                auto extentDeclaration = makeNode(
                    ProjectOopHirNodeKind::LocalDeclaration,
                    local.variableLengthArrayExtentSyntaxes[dimension],
                    extentType);
                extentDeclaration->stringValue =
                    methodBodyModel_->locals()[extentOrdinal].name;
                extentDeclaration->debugDisplayName =
                    local.name + ".Length[" + std::to_string(dimension) + "]";
                extentDeclaration->localOrdinal = extentOrdinal;
                extentDeclaration->children.push_back(std::move(child));
                variable.vlaBlock->children.push_back(
                    std::move(extentDeclaration));
                auto extentValue = makeNode(
                    ProjectOopHirNodeKind::LocalValue,
                    local.variableLengthArrayExtentSyntaxes[dimension],
                    extentType);
                extentValue->localOrdinal = extentOrdinal;
                variable.allocationCounts.push_back(std::move(extentValue));
                ++variable.nextExtent;
                if (variable.nextExtent ==
                    local.variableLengthArrayExtentSyntaxes.size())
                {
                    completeVariableAllocation(frame);
                }
                return true;
            }
            error = "Stage 34 variable declaration frame accepted an unexpected child";
            return false;
        };
        const auto finishVariableDeclarationFrame =
            [&](IterativeExpressionFrame& frame)
                -> std::unique_ptr<ProjectOopHirNode>
        {
            VariableDeclarationFrameState& variable =
                *frame.variableDeclaration;
            if (variable.mode == VariableDeclarationFrameMode::NoChild)
                variable.mode = VariableDeclarationFrameMode::Ready;
            return variable.vlaBlock != nullptr
                ? std::move(variable.vlaBlock)
                : std::move(variable.node);
        };
        struct DeconstructionDeclarationChild final
        {
            const ast::AstNode* syntax = nullptr;
            bool addressContext = false;
        };
        const auto initializeDeconstructionDeclarationFrame =
            [&](IterativeExpressionFrame& frame) -> bool
        {
            if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
            {
                error = "T02-F03 deconstruction declaration requires value layouts";
                return false;
            }
            DeconstructionDeclarationFrameState state;
            state.binding = methodBodyModel_ != nullptr
                ? methodBodyModel_->deconstructionBinding(frame.syntax)
                : nullptr;
            if (state.binding == nullptr)
            {
                error = "T02-F03 deconstruction declaration has no semantic binding";
                return false;
            }
            if (state.binding->selectedMemberOrdinal.has_value())
            {
                const ast::AstNode* receiverSyntax = frame.syntax->first();
                if (receiverSyntax == nullptr ||
                    receiverSyntax->kind() == ast::AstNodeKind::BaseExpression)
                {
                    state.source = lowerImplicitReceiver(
                        receiverSyntax, state.binding->sourceTypeKey, error);
                    if (state.source == nullptr) return false;
                    state.stage = DeconstructionDeclarationFrameStage::Ready;
                }
                else
                {
                    if (state.binding->sourceTypeKey.empty())
                    {
                        error = "Stage 40 receiver has no semantic class/interface/struct type";
                        return false;
                    }
                    state.materializeReceiverTemporary =
                        isStructValueType(state.binding->sourceTypeKey) &&
                        !isNaturallyAddressable(receiverSyntax);
                }
            }
            frame.deconstructionDeclaration.emplace(std::move(state));
            return true;
        };
        const auto nextDeconstructionDeclarationChild =
            [&](const IterativeExpressionFrame& frame)
                -> std::optional<DeconstructionDeclarationChild>
        {
            const DeconstructionDeclarationFrameState& state =
                *frame.deconstructionDeclaration;
            if (state.stage != DeconstructionDeclarationFrameStage::Source)
                return std::nullopt;
            return DeconstructionDeclarationChild{
                frame.syntax->first(),
                state.binding->selectedMemberOrdinal.has_value() &&
                    isStructValueType(state.binding->sourceTypeKey) &&
                    !state.materializeReceiverTemporary};
        };
        const auto acceptDeconstructionDeclarationChild =
            [&](IterativeExpressionFrame& frame,
                std::unique_ptr<ProjectOopHirNode> child) -> bool
        {
            DeconstructionDeclarationFrameState& state =
                *frame.deconstructionDeclaration;
            if (state.stage != DeconstructionDeclarationFrameStage::Source)
            {
                error = "Stage 34 deconstruction declaration frame accepted an unexpected child";
                return false;
            }
            if (state.materializeReceiverTemporary)
            {
                auto address = makeNode(
                    ProjectOopHirNodeKind::TemporaryAddress,
                    frame.syntax->first(),
                    state.binding->sourceTypeKey);
                address->elementTypeKey = state.binding->sourceTypeKey;
                address->children.push_back(std::move(child));
                child = std::move(address);
            }
            state.source = std::move(child);
            state.stage = DeconstructionDeclarationFrameStage::Ready;
            return true;
        };
        const auto finishDeconstructionDeclarationFrame =
            [&](IterativeExpressionFrame& frame)
                -> std::unique_ptr<ProjectOopHirNode>
        {
            DeconstructionDeclarationFrameState& state =
                *frame.deconstructionDeclaration;
            if (state.source == nullptr)
            {
                error = "T02-F03 deconstruction has no semantic source binding";
                return nullptr;
            }
            if (state.binding->selectedMemberOrdinal.has_value())
            {
                auto call = makeNode(
                    ProjectOopHirNodeKind::InstanceCall,
                    frame.syntax->first(),
                    state.binding->tupleTypeKey);
                call->memberOrdinal = *state.binding->selectedMemberOrdinal;
                call->children.push_back(std::move(state.source));
                state.source = std::move(call);
            }
            const std::optional<std::vector<std::string>> elements =
                ProjectTypeConversion::tupleElementTypeKeys(
                    state.binding->tupleTypeKey);
            const std::optional<language::AggregateLayout> layout =
                objectLayoutModel_->tupleLayoutForTypeKey(
                    *nameLookup_, state.binding->tupleTypeKey);
            if (!elements.has_value() || !layout.has_value() ||
                elements->size() != frame.syntax->children().size() ||
                layout->offsets.size() != elements->size())
            {
                error = "T02-F03 deconstruction declaration lost its tuple shape or layout";
                return nullptr;
            }
            auto block = makeNode(
                ProjectOopHirNodeKind::Block, frame.syntax, "builtin:void");
            auto deconstruct = makeNode(
                ProjectOopHirNodeKind::TupleDeconstruct,
                frame.syntax,
                state.binding->tupleTypeKey);
            deconstruct->children.push_back(std::move(state.source));
            const auto expandBindingTuple = [&] (
                const ast::AstNode* bindingSyntax,
                const std::string& typeKey,
                std::uint64_t byteOffset,
                std::vector<HirTuplePatternFrame>& children) -> bool
            {
                const std::optional<std::vector<std::string>> nestedElements =
                    ProjectTypeConversion::tupleElementTypeKeys(typeKey);
                const std::optional<language::AggregateLayout> nestedLayout =
                    objectLayoutModel_->tupleLayoutForTypeKey(*nameLookup_, typeKey);
                if (!nestedElements.has_value() || !nestedLayout.has_value() ||
                    nestedElements->size() != bindingSyntax->children().size() ||
                    nestedLayout->offsets.size() != nestedElements->size())
                {
                    error = "T02-F03 nested declaration pattern lost its tuple layout";
                    return false;
                }
                children.reserve(nestedElements->size());
                for (std::size_t index = 0; index < nestedElements->size(); ++index)
                {
                    children.push_back(HirTuplePatternFrame{
                        bindingSyntax->children()[index],
                        (*nestedElements)[index],
                        byteOffset + nestedLayout->offsets[index]});
                }
                return true;
            };
            const auto appendBindingLeaf = [&] (
                const ast::AstNode* bindingSyntax,
                const std::string& typeKey,
                std::uint64_t byteOffset) -> bool
            {
                if (bindingSyntax->kind() == ast::AstNodeKind::IdentifierExpression &&
                    bindingSyntax->text() == "_")
                {
                    return true;
                }
                const MethodLocalSymbol* local = localSymbol(bindingSyntax);
                if (local == nullptr)
                {
                    error = "T02-F03 deconstruction binding has no local symbol";
                    return false;
                }
                const std::size_t localOrdinal = static_cast<std::size_t>(
                    local - methodBodyModel_->locals().data());
                auto declaration = makeNode(
                    ProjectOopHirNodeKind::LocalDeclaration,
                    bindingSyntax,
                    local->typeKey);
                declaration->stringValue = local->name;
                declaration->localOrdinal = localOrdinal;
                block->children.push_back(std::move(declaration));

                auto address = makeNode(
                    ProjectOopHirNodeKind::LocalAddress,
                    bindingSyntax,
                    local->typeKey);
                address->localOrdinal = localOrdinal;
                auto element = makeNode(
                    ProjectOopHirNodeKind::TupleDeconstructElement,
                    bindingSyntax,
                    local->typeKey);
                element->elementTypeKey = typeKey;
                element->integerValue = byteOffset;
                element->conversion = ProjectTypeConversion::classifyImplicit(
                    typeKey, local->typeKey);
                element->children.push_back(std::move(address));
                deconstruct->children.push_back(std::move(element));
                return true;
            };
            for (std::size_t index = 0;
                 index < frame.syntax->children().size();
                 ++index)
            {
                if (!visitHirTuplePatternLeaves(
                        frame.syntax->children()[index],
                        (*elements)[index],
                        static_cast<std::uint64_t>(layout->offsets[index]),
                        expandBindingTuple,
                        appendBindingLeaf))
                {
                    return nullptr;
                }
            }
            block->children.push_back(std::move(deconstruct));
            return block;
        };
        struct InlineAssemblyOperandChild final
        {
            const ast::AstNode* syntax = nullptr;
            bool addressContext = false;
        };
        const auto initializeInlineAssemblyFrame =
            [&](IterativeExpressionFrame& frame)
        {
            InlineAssemblyFrameState state;
            state.node = makeNode(
                ProjectOopHirNodeKind::InlineAssembly,
                frame.syntax,
                "builtin:void");
            state.node->stringValue = frame.syntax->text();
            frame.inlineAssembly.emplace(std::move(state));
        };
        const auto advanceInlineAssemblyFrame =
            [&](IterativeExpressionFrame& frame)
                -> std::optional<InlineAssemblyOperandChild>
        {
            InlineAssemblyFrameState& state = *frame.inlineAssembly;
            while (state.nextRecord < frame.syntax->children().size())
            {
                const ast::AstNode* record =
                    frame.syntax->children()[state.nextRecord++];
                if (record == nullptr)
                {
                    error = "integrated assembly contains a null contract record";
                    return std::nullopt;
                }
                if (record->kind() == ast::AstNodeKind::InlineAssemblyLine)
                {
                    auto line = makeNode(
                        ProjectOopHirNodeKind::InlineAssemblyLine,
                        record,
                        "builtin:void");
                    line->stringValue = record->text();
                    line->elementTypeKey = record->auxiliaryText();
                    state.node->children.push_back(std::move(line));
                    continue;
                }
                if (record->kind() == ast::AstNodeKind::InlineAssemblyClobber)
                {
                    auto clobber = makeNode(
                        ProjectOopHirNodeKind::InlineAssemblyClobber,
                        record,
                        "builtin:void");
                    clobber->stringValue = record->text();
                    clobber->elementTypeKey = record->auxiliaryText();
                    clobber->integerValue = record->integerValue();
                    clobber->booleanValue = record->flag();
                    state.node->children.push_back(std::move(clobber));
                    continue;
                }
                const bool input = record->kind() ==
                    ast::AstNodeKind::InlineAssemblyInputOperand;
                const bool output = record->kind() ==
                    ast::AstNodeKind::InlineAssemblyOutputOperand;
                if (!input && !output)
                {
                    error = "integrated assembly contains an unknown contract record";
                    return std::nullopt;
                }
                if (record->first() == nullptr)
                {
                    error =
                        "integrated assembly operand lost its NativeSharp expression";
                    return std::nullopt;
                }
                state.pendingOperand = record;
                return InlineAssemblyOperandChild{
                    record->first(),
                    output || (input && record->auxiliaryText() == "m")};
            }
            return std::nullopt;
        };
        const auto acceptInlineAssemblyOperand =
            [&](IterativeExpressionFrame& frame,
                std::unique_ptr<ProjectOopHirNode> child) -> bool
        {
            InlineAssemblyFrameState& state = *frame.inlineAssembly;
            const ast::AstNode* record = state.pendingOperand;
            const MethodExpressionTypeBinding* operandType =
                record != nullptr ? expressionType(record->first()) : nullptr;
            if (operandType == nullptr || operandType->typeKey.empty())
            {
                error =
                    "integrated assembly operand lost its resolved NativeSharp type";
                return false;
            }
            const bool input = record->kind() ==
                ast::AstNodeKind::InlineAssemblyInputOperand;
            auto operand = makeNode(
                input ? ProjectOopHirNodeKind::InlineAssemblyInput
                      : ProjectOopHirNodeKind::InlineAssemblyOutput,
                record,
                operandType->typeKey);
            operand->stringValue = record->auxiliaryText();
            operand->integerValue = record->integerValue();
            operand->booleanValue = record->flag();
            operand->children.push_back(std::move(child));
            state.node->children.push_back(std::move(operand));
            state.pendingOperand = nullptr;
            return true;
        };
        const auto finishInlineAssemblyFrame =
            [&](IterativeExpressionFrame& frame)
                -> std::unique_ptr<ProjectOopHirNode>
        {
            return std::move(frame.inlineAssembly->node);
        };
        const auto activateLocalCleanup =
            [&](const std::unique_ptr<ProjectOopHirNode>& node)
        {
            const ProjectOopHirNode* declaration = node.get();
            if (declaration != nullptr &&
                declaration->kind == ProjectOopHirNodeKind::Block &&
                declaration->syntax != nullptr &&
                declaration->syntax->kind() ==
                    ast::AstNodeKind::VariableDeclarationStatement &&
                !declaration->children.empty())
            {
                declaration = declaration->children.back().get();
            }
            if (declaration != nullptr &&
                declaration->kind == ProjectOopHirNodeKind::LocalDeclaration &&
                declaration->cleanupRegionOrdinal.has_value())
            {
                activeHirCleanupRegions_.push_back(
                    *declaration->cleanupRegionOrdinal);
            }
        };

        enum class LoopBlockAdvance : std::uint8_t
        {
            Descend,
            Complete,
            Error,
        };
        const auto activateDirectStatementCleanup =
            [&](const std::unique_ptr<ProjectOopHirNode>& node)
        {
            if (node != nullptr &&
                node->kind == ProjectOopHirNodeKind::LocalDeclaration &&
                node->cleanupRegionOrdinal.has_value())
            {
                activeHirCleanupRegions_.push_back(
                    *node->cleanupRegionOrdinal);
            }
        };
        const auto isExitValueStatement = [](const ast::AstNode* statement) noexcept
        {
            return statement != nullptr && statement->first() != nullptr &&
                (statement->kind() == ast::AstNodeKind::BreakStatement ||
                 statement->kind() == ast::AstNodeKind::ReturnStatement ||
                 statement->kind() == ast::AstNodeKind::ThrowStatement);
        };
        const auto beginExitValueStatement =
            [&](const ast::AstNode* statement, std::size_t statementMemberOrdinal) -> bool
        {
            IterativeExpressionFrame exitFrame;
            exitFrame.memberOrdinal = statementMemberOrdinal;
            if (!initializeExitValueStatementFrame(exitFrame, statement)) return false;
            const bool valueAddressContext = exitFrame.addressContext;
            frames.push_back(std::move(exitFrame));
            current = statement->first();
            currentMemberOrdinal = statementMemberOrdinal;
            currentAddressContext = valueAddressContext;
            currentInputKind = IterativeInputKind::Expression;
            return true;
        };
        std::function<LoopBlockAdvance(std::size_t)> advanceForStatement;
        std::function<LoopBlockAdvance(std::size_t)> advanceForeachStatement;
        const auto advanceSwitchStatement =
            [&](std::size_t frameIndex) -> LoopBlockAdvance
        {
            while (true)
            {
                IterativeExpressionFrame& frame = frames[frameIndex];
                SwitchStatementFrameState& state = *frame.switchStatement;
                if (state.sectionSyntax == nullptr)
                {
                    while (state.nextSyntaxChild < frame.syntax->children().size())
                    {
                        const ast::AstNode* candidate =
                            frame.syntax->children()[state.nextSyntaxChild++];
                        if (candidate == nullptr ||
                            candidate->kind() != ast::AstNodeKind::SwitchSection)
                        {
                            continue;
                        }
                        if (state.nextBindingSection >= state.binding->sections.size())
                        {
                            error = "Stage 34 OOP HIR found more switch sections than its semantic binding";
                            return LoopBlockAdvance::Error;
                        }
                        const MethodSwitchSectionBinding& sectionBinding =
                            state.binding->sections[state.nextBindingSection++];
                        state.sectionSyntax = candidate;
                        state.nextSectionChild = 0U;
                        state.sectionNode = makeNode(
                            ProjectOopHirNodeKind::SwitchSection,
                            candidate,
                            "builtin:void");
                        state.sectionNode->switchLabels = sectionBinding.labelValues;
                        state.sectionNode->switchHasDefault = sectionBinding.hasDefault;
                        state.sectionBody = makeNode(
                            ProjectOopHirNodeKind::Block,
                            candidate,
                            "builtin:void");
                        frame.cleanupDepth = activeHirCleanupRegions_.size();
                        frame.cleanupActive = true;
                        break;
                    }
                    if (state.sectionSyntax == nullptr)
                    {
                        lowered = std::move(state.node);
                        frames.pop_back();
                        return LoopBlockAdvance::Complete;
                    }
                }

                const ast::AstNode* statement = nullptr;
                while (state.nextSectionChild < state.sectionSyntax->children().size())
                {
                    const ast::AstNode* candidate =
                        state.sectionSyntax->children()[state.nextSectionChild++];
                    if (candidate == nullptr ||
                        candidate->kind() == ast::AstNodeKind::SwitchLabel)
                    {
                        continue;
                    }
                    statement = candidate;
                    break;
                }
                if (statement == nullptr)
                {
                    activeHirCleanupRegions_.resize(frame.cleanupDepth);
                    frame.cleanupActive = false;
                    state.sectionNode->children.push_back(
                        std::move(state.sectionBody));
                    state.node->children.push_back(std::move(state.sectionNode));
                    state.sectionSyntax = nullptr;
                    continue;
                }

                frame.stage = 2U;
                if (statement->kind() == ast::AstNodeKind::BlockStatement)
                {
                    current = statement;
                    currentMemberOrdinal = frame.memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::LoopBlock;
                    return LoopBlockAdvance::Descend;
                }
                if (statement->kind() == ast::AstNodeKind::ExpressionStatement)
                {
                    IterativeExpressionFrame statementFrame;
                    statementFrame.kind =
                        IterativeExpressionKind::LoopBlockExpressionStatement;
                    statementFrame.syntax = statement;
                    statementFrame.memberOrdinal = frame.memberOrdinal;
                    frames.push_back(std::move(statementFrame));
                    current = statement->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement->kind() == ast::AstNodeKind::VariableDeclarationStatement)
                {
                    IterativeExpressionFrame variableFrame;
                    variableFrame.kind = IterativeExpressionKind::VariableDeclaration;
                    variableFrame.syntax = statement;
                    variableFrame.memberOrdinal = frame.memberOrdinal;
                    if (!initializeVariableDeclarationFrame(variableFrame))
                        return LoopBlockAdvance::Error;
                    const auto child = nextVariableDeclarationChild(variableFrame);
                    if (!child.has_value())
                    {
                        auto node = finishVariableDeclarationFrame(variableFrame);
                        activateDirectStatementCleanup(node);
                        state.sectionBody->children.push_back(std::move(node));
                        frame.stage = 1U;
                        continue;
                    }
                    const std::size_t childMemberOrdinal = variableFrame.memberOrdinal;
                    frames.push_back(std::move(variableFrame));
                    current = child->syntax;
                    currentMemberOrdinal = childMemberOrdinal;
                    currentAddressContext = child->addressContext;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement->kind() ==
                    ast::AstNodeKind::DeconstructionDeclarationStatement)
                {
                    IterativeExpressionFrame declarationFrame;
                    declarationFrame.kind =
                        IterativeExpressionKind::DeconstructionDeclaration;
                    declarationFrame.syntax = statement;
                    declarationFrame.memberOrdinal = frame.memberOrdinal;
                    if (!initializeDeconstructionDeclarationFrame(declarationFrame))
                        return LoopBlockAdvance::Error;
                    const auto child =
                        nextDeconstructionDeclarationChild(declarationFrame);
                    if (!child.has_value())
                    {
                        auto node =
                            finishDeconstructionDeclarationFrame(declarationFrame);
                        if (node == nullptr) return LoopBlockAdvance::Error;
                        state.sectionBody->children.push_back(std::move(node));
                        frame.stage = 1U;
                        continue;
                    }
                    const std::size_t childMemberOrdinal = declarationFrame.memberOrdinal;
                    frames.push_back(std::move(declarationFrame));
                    current = child->syntax;
                    currentMemberOrdinal = childMemberOrdinal;
                    currentAddressContext = child->addressContext;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement->kind() == ast::AstNodeKind::InlineAssemblyStatement)
                {
                    IterativeExpressionFrame assemblyFrame;
                    assemblyFrame.kind = IterativeExpressionKind::InlineAssemblyStatement;
                    assemblyFrame.syntax = statement;
                    assemblyFrame.memberOrdinal = frame.memberOrdinal;
                    initializeInlineAssemblyFrame(assemblyFrame);
                    const auto child = advanceInlineAssemblyFrame(assemblyFrame);
                    if (!error.empty()) return LoopBlockAdvance::Error;
                    if (!child.has_value())
                    {
                        state.sectionBody->children.push_back(
                            finishInlineAssemblyFrame(assemblyFrame));
                        frame.stage = 1U;
                        continue;
                    }
                    const std::size_t childMemberOrdinal = assemblyFrame.memberOrdinal;
                    frames.push_back(std::move(assemblyFrame));
                    current = child->syntax;
                    currentMemberOrdinal = childMemberOrdinal;
                    currentAddressContext = child->addressContext;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement->kind() == ast::AstNodeKind::StaticAssertStatement)
                {
                    IterativeExpressionFrame assertionFrame;
                    assertionFrame.kind = IterativeExpressionKind::StaticAssertStatement;
                    assertionFrame.syntax = statement;
                    assertionFrame.memberOrdinal = frame.memberOrdinal;
                    frames.push_back(std::move(assertionFrame));
                    current = statement->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement->kind() == ast::AstNodeKind::IfStatement)
                {
                    IterativeExpressionFrame ifFrame;
                    ifFrame.kind = IterativeExpressionKind::IfStatement;
                    ifFrame.syntax = statement;
                    ifFrame.memberOrdinal = frame.memberOrdinal;
                    ifFrame.loopBlockNode = makeNode(
                        ProjectOopHirNodeKind::If, statement, "builtin:void");
                    frames.push_back(std::move(ifFrame));
                    current = statement->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement->kind() == ast::AstNodeKind::WhileStatement ||
                    statement->kind() == ast::AstNodeKind::DoWhileStatement)
                {
                    IterativeExpressionFrame loopFrame;
                    loopFrame.kind = IterativeExpressionKind::LoopStatement;
                    loopFrame.syntax = statement;
                    loopFrame.memberOrdinal = frame.memberOrdinal;
                    loopFrame.loopBlockNode = makeNode(
                        statement->kind() == ast::AstNodeKind::DoWhileStatement
                            ? ProjectOopHirNodeKind::DoWhile
                            : ProjectOopHirNodeKind::While,
                        statement,
                        "builtin:void");
                    frames.push_back(std::move(loopFrame));
                    current = statement->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement->kind() == ast::AstNodeKind::SwitchStatement)
                {
                    IterativeExpressionFrame switchFrame;
                    switchFrame.memberOrdinal = frame.memberOrdinal;
                    if (!initializeSwitchStatementFrame(switchFrame, statement))
                        return LoopBlockAdvance::Error;
                    frames.push_back(std::move(switchFrame));
                    current = statement->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement->kind() == ast::AstNodeKind::ForStatement)
                {
                    IterativeExpressionFrame forFrame;
                    forFrame.memberOrdinal = frame.memberOrdinal;
                    initializeForStatementFrame(forFrame, statement);
                    frames.push_back(std::move(forFrame));
                    return advanceForStatement(frames.size() - 1U);
                }
                if (statement->kind() == ast::AstNodeKind::ForeachStatement)
                {
                    IterativeExpressionFrame foreachFrame;
                    foreachFrame.memberOrdinal = frame.memberOrdinal;
                    if (!initializeForeachStatementFrame(foreachFrame, statement))
                        return LoopBlockAdvance::Error;
                    frames.push_back(std::move(foreachFrame));
                    return advanceForeachStatement(frames.size() - 1U);
                }
                if (isExitValueStatement(statement))
                {
                    if (!beginExitValueStatement(statement, frame.memberOrdinal))
                        return LoopBlockAdvance::Error;
                    return LoopBlockAdvance::Descend;
                }

                auto node = lowerStatement(statement, frame.memberOrdinal, error);
                if (node == nullptr) return LoopBlockAdvance::Error;
                activateDirectStatementCleanup(node);
                state.sectionBody->children.push_back(std::move(node));
                frame.stage = 1U;
            }
        };
        advanceForStatement =
            [&](std::size_t frameIndex) -> LoopBlockAdvance
        {
            while (true)
            {
                IterativeExpressionFrame& frame = frames[frameIndex];
                ForStatementFrameState& state = *frame.forStatement;
                if (frame.stage == 0U)
                {
                    frame.stage = 1U;
                    const ast::AstNode* init = frame.syntax->first();
                    if (init == nullptr)
                    {
                        state.node->children.push_back(nullptr);
                        continue;
                    }
                    if (init->kind() == ast::AstNodeKind::BlockStatement)
                    {
                        current = init;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        currentInputKind = IterativeInputKind::LoopBlock;
                        return LoopBlockAdvance::Descend;
                    }
                    if (init->kind() == ast::AstNodeKind::VariableDeclarationStatement)
                    {
                        IterativeExpressionFrame variableFrame;
                        variableFrame.kind = IterativeExpressionKind::VariableDeclaration;
                        variableFrame.syntax = init;
                        variableFrame.memberOrdinal = frame.memberOrdinal;
                        if (!initializeVariableDeclarationFrame(variableFrame))
                            return LoopBlockAdvance::Error;
                        const auto child = nextVariableDeclarationChild(variableFrame);
                        if (!child.has_value())
                        {
                            auto node = finishVariableDeclarationFrame(variableFrame);
                            activateDirectStatementCleanup(node);
                            state.node->children.push_back(std::move(node));
                            continue;
                        }
                        const std::size_t childMemberOrdinal = variableFrame.memberOrdinal;
                        frames.push_back(std::move(variableFrame));
                        current = child->syntax;
                        currentMemberOrdinal = childMemberOrdinal;
                        currentAddressContext = child->addressContext;
                        currentInputKind = IterativeInputKind::Expression;
                        return LoopBlockAdvance::Descend;
                    }
                    current = init;
                    currentMemberOrdinal = frame.memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (frame.stage == 1U)
                {
                    frame.stage = 2U;
                    const ast::AstNode* condition = frame.syntax->second();
                    if (condition == nullptr)
                    {
                        state.node->children.push_back(nullptr);
                        continue;
                    }
                    current = condition;
                    currentMemberOrdinal = frame.memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (frame.stage == 2U)
                {
                    frame.stage = 3U;
                    const ast::AstNode* increment = frame.syntax->third();
                    if (increment == nullptr)
                    {
                        state.node->children.push_back(nullptr);
                        continue;
                    }
                    current = increment;
                    currentMemberOrdinal = frame.memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = increment->kind() == ast::AstNodeKind::BlockStatement
                        ? IterativeInputKind::LoopBlock
                        : IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (frame.stage == 3U)
                {
                    if (frame.syntax->children().empty())
                    {
                        error = "Stage 34 for statement is missing a body";
                        return LoopBlockAdvance::Error;
                    }
                    frame.stage = 4U;
                    current = frame.syntax->children().front();
                    currentMemberOrdinal = frame.memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::LoopBlock;
                    return LoopBlockAdvance::Descend;
                }
                return LoopBlockAdvance::Complete;
            }
        };
        advanceForeachStatement =
            [&](std::size_t frameIndex) -> LoopBlockAdvance
        {
            while (true)
            {
                IterativeExpressionFrame& frame = frames[frameIndex];
                ForeachStatementFrameState& state = *frame.foreachStatement;
                if (frame.stage != 0U) return LoopBlockAdvance::Complete;

                frame.stage = 1U;
                const ast::AstNode* source = frame.syntax->first();
                if (source != nullptr && source->kind() == ast::AstNodeKind::BlockStatement)
                {
                    current = source;
                    currentMemberOrdinal = frame.memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::LoopBlock;
                    return LoopBlockAdvance::Descend;
                }
                if (source != nullptr && source->kind() == ast::AstNodeKind::ExpressionStatement)
                {
                    IterativeExpressionFrame sourceFrame;
                    sourceFrame.kind = IterativeExpressionKind::LoopBlockExpressionStatement;
                    sourceFrame.syntax = source;
                    sourceFrame.memberOrdinal = frame.memberOrdinal;
                    frames.push_back(std::move(sourceFrame));
                    current = source->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (source != nullptr &&
                    source->kind() == ast::AstNodeKind::VariableDeclarationStatement)
                {
                    IterativeExpressionFrame sourceFrame;
                    sourceFrame.kind = IterativeExpressionKind::VariableDeclaration;
                    sourceFrame.syntax = source;
                    sourceFrame.memberOrdinal = frame.memberOrdinal;
                    if (!initializeVariableDeclarationFrame(sourceFrame))
                        return LoopBlockAdvance::Error;
                    const auto child = nextVariableDeclarationChild(sourceFrame);
                    if (child.has_value())
                    {
                        const std::size_t sourceMemberOrdinal = sourceFrame.memberOrdinal;
                        frames.push_back(std::move(sourceFrame));
                        current = child->syntax;
                        currentMemberOrdinal = sourceMemberOrdinal;
                        currentAddressContext = child->addressContext;
                        currentInputKind = IterativeInputKind::Expression;
                        return LoopBlockAdvance::Descend;
                    }
                    state.node->children.push_back(
                        finishVariableDeclarationFrame(sourceFrame));
                }
                else if (source != nullptr &&
                    source->kind() == ast::AstNodeKind::DeconstructionDeclarationStatement)
                {
                    IterativeExpressionFrame sourceFrame;
                    sourceFrame.kind = IterativeExpressionKind::DeconstructionDeclaration;
                    sourceFrame.syntax = source;
                    sourceFrame.memberOrdinal = frame.memberOrdinal;
                    if (!initializeDeconstructionDeclarationFrame(sourceFrame))
                        return LoopBlockAdvance::Error;
                    const auto child = nextDeconstructionDeclarationChild(sourceFrame);
                    if (child.has_value())
                    {
                        const std::size_t sourceMemberOrdinal = sourceFrame.memberOrdinal;
                        frames.push_back(std::move(sourceFrame));
                        current = child->syntax;
                        currentMemberOrdinal = sourceMemberOrdinal;
                        currentAddressContext = child->addressContext;
                        currentInputKind = IterativeInputKind::Expression;
                        return LoopBlockAdvance::Descend;
                    }
                    auto node = finishDeconstructionDeclarationFrame(sourceFrame);
                    if (node == nullptr) return LoopBlockAdvance::Error;
                    state.node->children.push_back(std::move(node));
                }
                else if (source != nullptr &&
                    source->kind() == ast::AstNodeKind::InlineAssemblyStatement)
                {
                    IterativeExpressionFrame sourceFrame;
                    sourceFrame.kind = IterativeExpressionKind::InlineAssemblyStatement;
                    sourceFrame.syntax = source;
                    sourceFrame.memberOrdinal = frame.memberOrdinal;
                    initializeInlineAssemblyFrame(sourceFrame);
                    const auto child = advanceInlineAssemblyFrame(sourceFrame);
                    if (!error.empty()) return LoopBlockAdvance::Error;
                    if (child.has_value())
                    {
                        const std::size_t sourceMemberOrdinal = sourceFrame.memberOrdinal;
                        frames.push_back(std::move(sourceFrame));
                        current = child->syntax;
                        currentMemberOrdinal = sourceMemberOrdinal;
                        currentAddressContext = child->addressContext;
                        currentInputKind = IterativeInputKind::Expression;
                        return LoopBlockAdvance::Descend;
                    }
                    state.node->children.push_back(
                        finishInlineAssemblyFrame(sourceFrame));
                }
                else if (source != nullptr &&
                    source->kind() == ast::AstNodeKind::StaticAssertStatement)
                {
                    IterativeExpressionFrame sourceFrame;
                    sourceFrame.kind = IterativeExpressionKind::StaticAssertStatement;
                    sourceFrame.syntax = source;
                    sourceFrame.memberOrdinal = frame.memberOrdinal;
                    frames.push_back(std::move(sourceFrame));
                    current = source->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                else if (source != nullptr && source->kind() == ast::AstNodeKind::IfStatement)
                {
                    IterativeExpressionFrame sourceFrame;
                    sourceFrame.kind = IterativeExpressionKind::IfStatement;
                    sourceFrame.syntax = source;
                    sourceFrame.memberOrdinal = frame.memberOrdinal;
                    sourceFrame.loopBlockNode = makeNode(
                        ProjectOopHirNodeKind::If, source, "builtin:void");
                    frames.push_back(std::move(sourceFrame));
                    current = source->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                else if (source != nullptr &&
                    (source->kind() == ast::AstNodeKind::WhileStatement ||
                     source->kind() == ast::AstNodeKind::DoWhileStatement))
                {
                    IterativeExpressionFrame sourceFrame;
                    sourceFrame.kind = IterativeExpressionKind::LoopStatement;
                    sourceFrame.syntax = source;
                    sourceFrame.memberOrdinal = frame.memberOrdinal;
                    sourceFrame.loopBlockNode = makeNode(
                        source->kind() == ast::AstNodeKind::DoWhileStatement
                            ? ProjectOopHirNodeKind::DoWhile
                            : ProjectOopHirNodeKind::While,
                        source,
                        "builtin:void");
                    frames.push_back(std::move(sourceFrame));
                    current = source->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                else if (source != nullptr && source->kind() == ast::AstNodeKind::SwitchStatement)
                {
                    IterativeExpressionFrame sourceFrame;
                    sourceFrame.memberOrdinal = frame.memberOrdinal;
                    if (!initializeSwitchStatementFrame(sourceFrame, source))
                        return LoopBlockAdvance::Error;
                    frames.push_back(std::move(sourceFrame));
                    current = source->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                else if (source != nullptr && source->kind() == ast::AstNodeKind::ForStatement)
                {
                    IterativeExpressionFrame sourceFrame;
                    sourceFrame.memberOrdinal = frame.memberOrdinal;
                    initializeForStatementFrame(sourceFrame, source);
                    frames.push_back(std::move(sourceFrame));
                    return advanceForStatement(frames.size() - 1U);
                }
                else if (source != nullptr && source->kind() == ast::AstNodeKind::ForeachStatement)
                {
                    IterativeExpressionFrame sourceFrame;
                    sourceFrame.memberOrdinal = frame.memberOrdinal;
                    if (!initializeForeachStatementFrame(sourceFrame, source))
                        return LoopBlockAdvance::Error;
                    frames.push_back(std::move(sourceFrame));
                    frameIndex = frames.size() - 1U;
                    continue;
                }
                else if (isExitValueStatement(source))
                {
                    if (!beginExitValueStatement(source, frame.memberOrdinal))
                        return LoopBlockAdvance::Error;
                    return LoopBlockAdvance::Descend;
                }
                else
                {
                    auto sourceNode = lowerStatement(source, frame.memberOrdinal, error);
                    if (sourceNode == nullptr) return LoopBlockAdvance::Error;
                    state.node->children.push_back(std::move(sourceNode));
                }

                frame.stage = 2U;
                current = state.chosenCandidate;
                currentMemberOrdinal = frame.memberOrdinal;
                currentAddressContext = false;
                currentInputKind = IterativeInputKind::LoopBlock;
                return LoopBlockAdvance::Descend;
            }
        };
        const auto advanceLoopBlock = [&](std::size_t frameIndex) -> LoopBlockAdvance
        {
            while (true)
            {
                IterativeExpressionFrame& frame = frames[frameIndex];
                if (frame.armIndex >= frame.syntax->children().size())
                {
                    activeHirCleanupRegions_.resize(frame.cleanupDepth);
                    lowered = std::move(frame.loopBlockNode);
                    frames.pop_back();
                    return LoopBlockAdvance::Complete;
                }

                const ast::AstNode* statement = frame.syntax->children()[frame.armIndex++];
                if (statement != nullptr &&
                    statement->kind() == ast::AstNodeKind::BlockStatement)
                {
                    current = statement;
                    currentMemberOrdinal = frame.memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::LoopBlock;
                    return LoopBlockAdvance::Descend;
                }
                if (statement != nullptr &&
                    statement->kind() == ast::AstNodeKind::ExpressionStatement)
                {
                    IterativeExpressionFrame statementFrame;
                    statementFrame.kind = IterativeExpressionKind::LoopBlockExpressionStatement;
                    statementFrame.syntax = statement;
                    statementFrame.memberOrdinal = frame.memberOrdinal;
                    frames.push_back(std::move(statementFrame));
                    current = statement->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement != nullptr &&
                    statement->kind() ==
                        ast::AstNodeKind::VariableDeclarationStatement)
                {
                    IterativeExpressionFrame variableFrame;
                    variableFrame.kind =
                        IterativeExpressionKind::VariableDeclaration;
                    variableFrame.syntax = statement;
                    variableFrame.memberOrdinal = frame.memberOrdinal;
                    if (!initializeVariableDeclarationFrame(variableFrame))
                        return LoopBlockAdvance::Error;
                    const auto child =
                        nextVariableDeclarationChild(variableFrame);
                    if (!child.has_value())
                    {
                        auto variableNode =
                            finishVariableDeclarationFrame(variableFrame);
                        activateLocalCleanup(variableNode);
                        frame.loopBlockNode->children.push_back(
                            std::move(variableNode));
                        continue;
                    }
                    const std::size_t variableMemberOrdinal =
                        variableFrame.memberOrdinal;
                    frames.push_back(std::move(variableFrame));
                    current = child->syntax;
                    currentMemberOrdinal = variableMemberOrdinal;
                    currentAddressContext = child->addressContext;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement != nullptr &&
                    statement->kind() ==
                        ast::AstNodeKind::DeconstructionDeclarationStatement)
                {
                    IterativeExpressionFrame declarationFrame;
                    declarationFrame.kind =
                        IterativeExpressionKind::DeconstructionDeclaration;
                    declarationFrame.syntax = statement;
                    declarationFrame.memberOrdinal = frame.memberOrdinal;
                    if (!initializeDeconstructionDeclarationFrame(declarationFrame))
                        return LoopBlockAdvance::Error;
                    const auto child =
                        nextDeconstructionDeclarationChild(declarationFrame);
                    if (!child.has_value())
                    {
                        auto declarationNode =
                            finishDeconstructionDeclarationFrame(declarationFrame);
                        if (declarationNode == nullptr)
                            return LoopBlockAdvance::Error;
                        frame.loopBlockNode->children.push_back(
                            std::move(declarationNode));
                        continue;
                    }
                    const std::size_t declarationMemberOrdinal =
                        declarationFrame.memberOrdinal;
                    frames.push_back(std::move(declarationFrame));
                    current = child->syntax;
                    currentMemberOrdinal = declarationMemberOrdinal;
                    currentAddressContext = child->addressContext;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement != nullptr &&
                    statement->kind() == ast::AstNodeKind::InlineAssemblyStatement)
                {
                    IterativeExpressionFrame assemblyFrame;
                    assemblyFrame.kind =
                        IterativeExpressionKind::InlineAssemblyStatement;
                    assemblyFrame.syntax = statement;
                    assemblyFrame.memberOrdinal = frame.memberOrdinal;
                    initializeInlineAssemblyFrame(assemblyFrame);
                    const auto child = advanceInlineAssemblyFrame(assemblyFrame);
                    if (!error.empty()) return LoopBlockAdvance::Error;
                    if (!child.has_value())
                    {
                        frame.loopBlockNode->children.push_back(
                            finishInlineAssemblyFrame(assemblyFrame));
                        continue;
                    }
                    const std::size_t assemblyMemberOrdinal =
                        assemblyFrame.memberOrdinal;
                    frames.push_back(std::move(assemblyFrame));
                    current = child->syntax;
                    currentMemberOrdinal = assemblyMemberOrdinal;
                    currentAddressContext = child->addressContext;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement != nullptr &&
                    statement->kind() == ast::AstNodeKind::StaticAssertStatement)
                {
                    IterativeExpressionFrame assertionFrame;
                    assertionFrame.kind = IterativeExpressionKind::StaticAssertStatement;
                    assertionFrame.syntax = statement;
                    assertionFrame.memberOrdinal = frame.memberOrdinal;
                    frames.push_back(std::move(assertionFrame));
                    current = statement->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement != nullptr &&
                    statement->kind() == ast::AstNodeKind::IfStatement)
                {
                    IterativeExpressionFrame ifFrame;
                    ifFrame.kind = IterativeExpressionKind::IfStatement;
                    ifFrame.syntax = statement;
                    ifFrame.memberOrdinal = frame.memberOrdinal;
                    ifFrame.loopBlockNode = makeNode(
                        ProjectOopHirNodeKind::If,
                        statement,
                        "builtin:void");
                    frames.push_back(std::move(ifFrame));
                    current = statement->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement != nullptr &&
                    (statement->kind() == ast::AstNodeKind::WhileStatement ||
                     statement->kind() == ast::AstNodeKind::DoWhileStatement))
                {
                    IterativeExpressionFrame loopFrame;
                    loopFrame.kind = IterativeExpressionKind::LoopStatement;
                    loopFrame.syntax = statement;
                    loopFrame.memberOrdinal = frame.memberOrdinal;
                    loopFrame.loopBlockNode = makeNode(
                        statement->kind() == ast::AstNodeKind::DoWhileStatement
                            ? ProjectOopHirNodeKind::DoWhile
                            : ProjectOopHirNodeKind::While,
                        statement,
                        "builtin:void");
                    frames.push_back(std::move(loopFrame));
                    current = statement->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement != nullptr &&
                    statement->kind() == ast::AstNodeKind::SwitchStatement)
                {
                    IterativeExpressionFrame switchFrame;
                    switchFrame.memberOrdinal = frame.memberOrdinal;
                    if (!initializeSwitchStatementFrame(switchFrame, statement))
                        return LoopBlockAdvance::Error;
                    frames.push_back(std::move(switchFrame));
                    current = statement->first();
                    currentMemberOrdinal = frames.back().memberOrdinal;
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::Expression;
                    return LoopBlockAdvance::Descend;
                }
                if (statement != nullptr &&
                    statement->kind() == ast::AstNodeKind::ForStatement)
                {
                    IterativeExpressionFrame forFrame;
                    forFrame.memberOrdinal = frame.memberOrdinal;
                    initializeForStatementFrame(forFrame, statement);
                    frames.push_back(std::move(forFrame));
                    const LoopBlockAdvance advance =
                        advanceForStatement(frames.size() - 1U);
                    if (advance != LoopBlockAdvance::Complete) return advance;
                    IterativeExpressionFrame& parent = frames[frameIndex];
                    activateLocalCleanup(lowered);
                    parent.loopBlockNode->children.push_back(std::move(lowered));
                    continue;
                }
                if (statement != nullptr &&
                    statement->kind() == ast::AstNodeKind::ForeachStatement)
                {
                    IterativeExpressionFrame foreachFrame;
                    foreachFrame.memberOrdinal = frame.memberOrdinal;
                    if (!initializeForeachStatementFrame(foreachFrame, statement))
                        return LoopBlockAdvance::Error;
                    frames.push_back(std::move(foreachFrame));
                    const LoopBlockAdvance advance =
                        advanceForeachStatement(frames.size() - 1U);
                    if (advance != LoopBlockAdvance::Complete) return advance;
                    IterativeExpressionFrame& parent = frames[frameIndex];
                    activateLocalCleanup(lowered);
                    parent.loopBlockNode->children.push_back(std::move(lowered));
                    continue;
                }
                if (isExitValueStatement(statement))
                {
                    if (!beginExitValueStatement(statement, frame.memberOrdinal))
                        return LoopBlockAdvance::Error;
                    return LoopBlockAdvance::Descend;
                }

                std::unique_ptr<ProjectOopHirNode> statementNode =
                    lowerStatement(statement, frame.memberOrdinal, error);
                if (statementNode == nullptr) return LoopBlockAdvance::Error;
                activateLocalCleanup(statementNode);
                frame.loopBlockNode->children.push_back(std::move(statementNode));
            }
        };
        bool worklistComplete = false;
        if (frames.size() == 1U &&
            frames.back().kind == IterativeExpressionKind::ForStatement)
        {
            const LoopBlockAdvance advance = advanceForStatement(0U);
            if (advance == LoopBlockAdvance::Error)
            {
                rollbackMatchCleanups();
                return nullptr;
            }
            if (advance == LoopBlockAdvance::Complete)
                worklistComplete = true;
        }
        if (frames.size() == 1U &&
            frames.back().kind == IterativeExpressionKind::ForeachStatement)
        {
            const LoopBlockAdvance advance = advanceForeachStatement(0U);
            if (advance == LoopBlockAdvance::Error)
            {
                rollbackMatchCleanups();
                return nullptr;
            }
            if (advance == LoopBlockAdvance::Complete)
                worklistComplete = true;
        }
        if (frames.size() == 1U &&
            (frames.back().kind == IterativeExpressionKind::VariableDeclaration ||
             frames.back().kind ==
                IterativeExpressionKind::DeconstructionDeclaration ||
             frames.back().kind ==
                IterativeExpressionKind::InlineAssemblyStatement))
        {
            const bool variable = frames.back().kind ==
                IterativeExpressionKind::VariableDeclaration;
            const bool deconstruction = frames.back().kind ==
                IterativeExpressionKind::DeconstructionDeclaration;
            if ((variable && !initializeVariableDeclarationFrame(frames.back())) ||
                (deconstruction &&
                    !initializeDeconstructionDeclarationFrame(frames.back())))
            {
                if (!lastErrorRange_.isValid())
                    lastErrorRange_ = frames.back().syntax->range();
                return nullptr;
            }
            if (variable)
            {
                const auto child = nextVariableDeclarationChild(frames.back());
                if (child.has_value())
                {
                    current = child->syntax;
                    currentAddressContext = child->addressContext;
                }
                else
                {
                    lowered = finishVariableDeclarationFrame(frames.back());
                    frames.pop_back();
                    worklistComplete = true;
                }
            }
            else if (deconstruction)
            {
                const auto child =
                    nextDeconstructionDeclarationChild(frames.back());
                if (child.has_value())
                {
                    current = child->syntax;
                    currentAddressContext = child->addressContext;
                }
                else
                {
                    lowered = finishDeconstructionDeclarationFrame(frames.back());
                    frames.pop_back();
                    worklistComplete = lowered != nullptr;
                    if (!worklistComplete)
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = statementContinuation->range();
                        return nullptr;
                    }
                }
            }
            else
            {
                initializeInlineAssemblyFrame(frames.back());
                const auto child = advanceInlineAssemblyFrame(frames.back());
                if (!error.empty())
                {
                    if (!lastErrorRange_.isValid())
                        lastErrorRange_ = frames.back().syntax->range();
                    return nullptr;
                }
                if (child.has_value())
                {
                    current = child->syntax;
                    currentAddressContext = child->addressContext;
                }
                else
                {
                    lowered = finishInlineAssemblyFrame(frames.back());
                    frames.pop_back();
                    worklistComplete = true;
                }
            }
        }
        while (!worklistComplete)
        {
            const void* schedulerResult = [&]() -> const void*
            {
            while (!worklistComplete)
            {
        iterative_restart:
            if (currentInputKind == IterativeInputKind::LoopBlock)
            {
                if (current == nullptr || current->kind() != ast::AstNodeKind::BlockStatement)
                {
                    error = "Stage 34 OOP HIR requires block statement body";
                    if (!lastErrorRange_.isValid() && current != nullptr)
                        lastErrorRange_ = current->range();
                    rollbackMatchCleanups();
                    if (resumePendingConditionStatementAfterFailure()) goto iterative_restart;
                    return nullptr;
                }
                IterativeExpressionFrame blockFrame;
                blockFrame.kind = IterativeExpressionKind::LoopBlock;
                blockFrame.syntax = current;
                blockFrame.memberOrdinal = currentMemberOrdinal;
                blockFrame.cleanupDepth = activeHirCleanupRegions_.size();
                blockFrame.loopBlockNode = makeNode(
                    ProjectOopHirNodeKind::Block, current, "builtin:void");
                frames.push_back(std::move(blockFrame));
                const LoopBlockAdvance advance = advanceLoopBlock(frames.size() - 1U);
                if (advance == LoopBlockAdvance::Descend) continue;
                if (advance == LoopBlockAdvance::Error)
                {
                    rollbackMatchCleanups();
                    if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                    if (resumePendingConditionStatementAfterFailure()) goto iterative_restart;
                    return nullptr;
                }
            }
            else
            {
                const std::optional<IterativeExpressionKind> currentKind =
                    iterativeKind(current, currentAddressContext);
                bool completedWithoutChild = false;
                if (currentKind.has_value())
                {
                IterativeExpressionFrame frame;
                frame.kind = *currentKind;
                frame.syntax = current;
                frame.addressContext = currentAddressContext;
                frame.memberOrdinal = currentMemberOrdinal;
                if (*currentKind == IterativeExpressionKind::LoopExpression)
                {
                    if (currentAddressContext)
                    {
                        error = "a loop expression produces a value, not addressable storage";
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        if (resumePendingConditionStatementAfterFailure()) goto iterative_restart;
                        return nullptr;
                    }
                    frame.loopExpressionBinding = methodBodyModel_ != nullptr
                        ? methodBodyModel_->loopExpressionBinding(current) : nullptr;
                    if (frame.loopExpressionBinding == nullptr)
                    {
                        error = "T07-F05 loop expression has no semantic binding";
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        if (resumePendingConditionStatementAfterFailure()) goto iterative_restart;
                        return nullptr;
                    }
                    frame.first = makeNode(
                        ProjectOopHirNodeKind::LoopExpression,
                        current,
                        frame.loopExpressionBinding->resultTypeKey);
                    frame.first->stringValue = frame.loopExpressionBinding->label;
                }
                else if (*currentKind == IterativeExpressionKind::AwaitExpression)
                {
                    const MethodCallBinding* binding = callBinding(current->first());
                    if (binding != nullptr && binding->selectedMemberOrdinal.has_value() &&
                        *binding->selectedMemberOrdinal < memberModel_->members().size())
                    {
                        frame.awaitMemberOrdinal = *binding->selectedMemberOrdinal;
                        if (awaitProducerCanSuspend(*frame.awaitMemberOrdinal))
                        {
                            error = "await on an async function that can suspend requires the state machine "
                                    "lowering (NS304-T05-F03)";
                            if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                            rollbackMatchCleanups();
                            if (resumePendingConditionStatementAfterFailure()) goto iterative_restart;
                            return nullptr;
                        }
                        if (!loweringAsyncBody_ && !activeHirExceptionHandlers_.empty())
                        {
                            const auto& handler = activeHirExceptionHandlers_.back();
                            for (std::size_t index = activeHirCleanupRegions_.size();
                                index > handler.retainedCleanupDepth; --index)
                            {
                                frame.awaitCleanupRegionOrdinals.push_back(
                                    activeHirCleanupRegions_[index - 1U]);
                            }
                            frame.awaitExceptionTargetRegionOrdinal = handler.hirRegionOrdinal;
                        }
                    }
                }
                else if (*currentKind == IterativeExpressionKind::UserUnaryOperator)
                {
                    if (!initializeBoundCallFrame(
                            frame, callBinding(current), {current->first()}, false))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        if (resumePendingConditionStatementAfterFailure()) goto iterative_restart;
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::UserBinaryOperator)
                {
                    if (!initializeBoundCallFrame(
                            frame,
                            callBinding(current),
                            {current->first(), current->second()},
                            false))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        if (resumePendingConditionStatementAfterFailure()) goto iterative_restart;
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::UserConversion)
                {
                    if (!initializeBoundCallFrame(
                            frame, callBinding(current), {current->first()}, false))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        if (resumePendingConditionStatementAfterFailure()) goto iterative_restart;
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::NamespaceCall)
                {
                    if (!initializeNamespaceCallFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        if (resumePendingConditionStatementAfterFailure()) goto iterative_restart;
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::EventInvoke)
                {
                    if (!initializeEventInvokeFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::DelegateInvoke)
                {
                    if (!initializeDelegateInvokeFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::AssertionCore)
                {
                    if (!initializeAssertionCoreFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::IteratorCoreCall)
                {
                    if (!initializeIteratorCoreCallFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::CoreOperandCall)
                {
                    if (!initializeCoreOperandCallFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::AtomicCoreCall)
                {
                    if (!initializeAtomicCoreCallFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::MonitorCoreCall)
                {
                    if (!initializeMonitorCoreCallFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::ConsoleOutputCall)
                {
                    if (!initializeConsoleOutputCallFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::ConsoleReadLineCall)
                {
                    if (!initializeConsoleReadLineCallFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::EnumToStringCall)
                {
                    if (!initializeEnumToStringCallFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::StructCloneCall)
                {
                    if (!initializeStructCloneCallFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::StructAggregateCall)
                {
                    if (!initializeStructAggregateCallFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::EnvironmentExitCall)
                {
                    if (!initializeEnvironmentExitCallFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::OrdinaryCall)
                {
                    if (!initializeOrdinaryCallFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::AggregateCrossLowering)
                {
                    if (!initializeAggregateCrossFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::OrdinaryConstruction)
                {
                    if (!initializeOrdinaryConstructionFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::AllocatorConstruction)
                {
                    if (!initializeAllocatorConstructionFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::PropertyGetter)
                {
                    frame.nullConditionalCall = current->kind() ==
                        ast::AstNodeKind::NullConditionalMemberExpression;
                    const MethodExpressionTypeBinding* receiverFlow =
                        frame.nullConditionalCall ? expressionType(current->first()) : nullptr;
                    frame.flowProvenNonNull = receiverFlow != nullptr &&
                        receiverFlow->flowProvenNonNull;
                    if (!initializeBoundCallFrame(
                            frame,
                            accessorGetterBinding(current),
                            {},
                            frame.nullConditionalCall && !frame.flowProvenNonNull))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::ExtensionPropertyGetter)
                {
                    frame.extensionPropertyBinding = namespaceFunctionCallBinding(current);
                    frame.operandAddressContext =
                        *extensionPropertyReceiverAddress(*frame.extensionPropertyBinding);
                }
                else if (*currentKind == IterativeExpressionKind::UserIndexerGetter)
                {
                    const MethodCoreBinding* core = coreBinding(current);
                    frame.nullConditionalCall = core != nullptr &&
                        core->kind == MethodCoreOperationKind::NullConditionalUserIndex;
                    if (!current->children().empty())
                    {
                        frame.operandSyntaxes.assign(
                            current->children().begin(), current->children().end());
                    }
                    else if (current->second() != nullptr)
                    {
                        frame.operandSyntaxes.push_back(current->second());
                    }
                    if (!initializeBoundCallFrame(
                            frame,
                            callBinding(current),
                            std::move(frame.operandSyntaxes),
                            frame.nullConditionalCall))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::DelegateCreationReceiver)
                {
                    frame.delegateCreationBinding = delegateBinding(current);
                    const std::optional<bool> receiverReady = initializeReceiverFrame(
                        frame.receiverFrame,
                        frame.delegateCreationBinding->receiverSyntax,
                        frame.delegateCreationBinding->receiverTypeKey);
                    if (!receiverReady.has_value())
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::InterpolationHandler)
                {
                    frame.core = coreBinding(current);
                    frame.syntheticLocalOrdinal = nextSyntheticLocalOrdinal_++;
                    std::string parseError;
                    if (!language::parseStaticInterpolation(
                            current->text(), frame.interpolationParts, parseError))
                    {
                        error = std::move(parseError);
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    const std::size_t holeCount = static_cast<std::size_t>(std::count_if(
                        frame.interpolationParts.begin(),
                        frame.interpolationParts.end(),
                        [](const language::InterpolationPart& part)
                        {
                            return part.kind == language::InterpolationPartKind::Hole;
                        }));
                    const std::size_t available =
                        std::min(holeCount, current->children().size());
                    frame.operandSyntaxes.assign(
                        current->children().begin(), current->children().begin() + available);
                    frame.operands.reserve(available);
                }
                else if (*currentKind == IterativeExpressionKind::CollectionBuilder)
                {
                    frame.core = coreBinding(current);
                    frame.syntheticLocalOrdinal = nextSyntheticLocalOrdinal_++;
                    frame.operandSyntaxes.assign(
                        current->children().begin(), current->children().end());
                    frame.operands.reserve(frame.operandSyntaxes.size());
                }
                else if (*currentKind == IterativeExpressionKind::ArrayConstruction)
                {
                    frame.core = coreBinding(current);
                    frame.first = makeNode(
                        frame.core->kind == MethodCoreOperationKind::RangeArrayAllocation
                            ? ProjectOopHirNodeKind::RangeArrayAllocation
                            : ProjectOopHirNodeKind::ArrayAllocation,
                        current, frame.core->resultTypeKey);
                    frame.first->elementTypeKey = frame.core->elementTypeKey;
                    if (frame.core->kind == MethodCoreOperationKind::RangeArrayAllocation)
                    {
                        frame.first->nativeArrayRank =
                            ProjectTypeConversion::nativeArrayRank(frame.core->resultTypeKey);
                        frame.operandSyntaxes.push_back(current->first());
                        frame.operandSyntaxes.insert(frame.operandSyntaxes.end(),
                            current->children().begin(), current->children().end());
                    }
                    else
                    {
                        frame.arraySegmented = std::any_of(
                            current->children().begin(), current->children().end(),
                            [](const ast::AstNode* child) { return child != nullptr &&
                                child->kind() == ast::AstNodeKind::SpreadElementExpression; });
                        frame.arrayInferred = current->auxiliaryText() ==
                            "inferred-array-initializer";
                        frame.first->booleanValue = frame.arrayInferred;
                        if (frame.arraySegmented)
                        {
                            for (const ast::AstNode* child : current->children())
                            {
                                const bool spread = child != nullptr &&
                                    child->kind() == ast::AstNodeKind::SpreadElementExpression;
                                frame.arraySpreadOperands.push_back(spread);
                                frame.operandSyntaxes.push_back(spread ? child->first() : child);
                            }
                        }
                        else
                        {
                            if (frame.arrayInferred)
                            {
                                auto count = makeNode(ProjectOopHirNodeKind::IntegerConstant,
                                    current, "builtin:int");
                                count->integerValue = current->children().size();
                                frame.first->children.push_back(std::move(count));
                            }
                            else frame.operandSyntaxes.push_back(current->first());
                            frame.operandSyntaxes.insert(frame.operandSyntaxes.end(),
                                current->children().begin(), current->children().end());
                        }
                    }
                }
                else if (*currentKind == IterativeExpressionKind::InstanceField ||
                    *currentKind == IterativeExpressionKind::NullConditionalField)
                {
                    frame.fieldBinding = nameBinding(current);
                    if (*currentKind == IterativeExpressionKind::InstanceField)
                    {
                        frame.materializeReceiverTemporary =
                            isStructValueType(frame.fieldBinding->receiverTypeKey) &&
                            !isNaturallyAddressable(frame.fieldBinding->receiverSyntax);
                    }
                }
                else if (*currentKind == IterativeExpressionKind::RangeSlice)
                {
                    frame.rangeBinding = rangeSliceBinding(current);
                    frame.materializeReceiverTemporary =
                        isStructValueType(frame.rangeBinding->receiverTypeKey) &&
                        !isNaturallyAddressable(frame.rangeBinding->receiverSyntax);
                }
                else if (*currentKind == IterativeExpressionKind::CoreIndex)
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
                }
                else if (*currentKind == IterativeExpressionKind::CoreBinary)
                {
                    frame.core = coreBinding(current);
                }
                else if (*currentKind == IterativeExpressionKind::RawIndex)
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
                }
                else if (*currentKind == IterativeExpressionKind::ResultPropagation)
                {
                    frame.resultPropagation = resultPropagationBinding(current);
                    frame.structuredExit = structuredExitBinding(current);
                }
                else if (*currentKind == IterativeExpressionKind::Destruction)
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
                        frame.operandAddressContext = current->flag()
                            ? directElement
                            : directElement &&
                                (isReferenceType(frame.operandType->typeKey) || nativeString);
                    }
                }
                else if (*currentKind == IterativeExpressionKind::StackAllocation)
                {
                    frame.core = coreBinding(current);
                }
                else if (*currentKind == IterativeExpressionKind::MoveProjection)
                {
                    frame.move = moveBinding(current);
                }
                else if (*currentKind == IterativeExpressionKind::Match)
                {
                    frame.matchBinding = methodBodyModel_ != nullptr
                        ? methodBodyModel_->matchExpressionBinding(current) : nullptr;
                    if (frame.matchBinding == nullptr)
                    {
                        error = "T03-F03 match expression has no semantic binding";
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    if (currentAddressContext && !frame.matchBinding->referenceResult)
                    {
                        error = "a value match expression does not produce addressable storage";
                        rollbackMatchCleanups();
                        return nullptr;
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
                }
                else if (*currentKind == IterativeExpressionKind::DeconstructionAssignment)
                {
                    frame.deconstructionBinding =
                        methodBodyModel_->deconstructionBinding(current);
                    frame.materializeReceiverTemporary =
                        frame.deconstructionBinding->selectedMemberOrdinal.has_value() &&
                        isStructValueType(frame.deconstructionBinding->sourceTypeKey) &&
                        !isNaturallyAddressable(current->second());
                }
                else if (*currentKind == IterativeExpressionKind::EventSubscription)
                {
                    frame.eventSubscriptionBinding = eventBinding(current);
                    const MethodEventBinding& binding = *frame.eventSubscriptionBinding;
                    if (!binding.customAccessorMemberOrdinal.has_value() &&
                        (memberModel_ == nullptr ||
                         binding.backingMemberOrdinal >= memberModel_->members().size()))
                    {
                        error = "Stage 42 event binding has invalid backing member";
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    if (!binding.staticEvent && binding.receiverTypeKey.empty())
                    {
                        error = "Stage 40 receiver has no semantic class/interface/struct type";
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    if (!binding.staticEvent &&
                        (binding.receiverSyntax == nullptr ||
                         binding.receiverSyntax->kind() == ast::AstNodeKind::BaseExpression))
                    {
                        if (currentLambda_ != nullptr)
                        {
                            const std::optional<std::size_t> capture =
                                currentLambdaThisCapture();
                            if (!capture.has_value())
                            {
                                error = "Stage 43 implicit instance receiver has no captured this";
                                rollbackMatchCleanups();
                                return nullptr;
                            }
                            frame.first = makeNode(
                                isStructValueType(binding.receiverTypeKey)
                                    ? ProjectOopHirNodeKind::ClosureCaptureAddress
                                    : ProjectOopHirNodeKind::ClosureCaptureValue,
                                binding.receiverSyntax,
                                binding.receiverTypeKey);
                            frame.first->integerValue = *capture;
                        }
                        else
                        {
                            frame.first = makeNode(
                                ProjectOopHirNodeKind::ThisValue,
                                binding.receiverSyntax,
                                binding.receiverTypeKey);
                        }
                        frame.stage = 1U;
                    }
                    else if (!binding.staticEvent)
                    {
                        frame.materializeReceiverTemporary =
                            isStructValueType(binding.receiverTypeKey) &&
                            !isNaturallyAddressable(binding.receiverSyntax);
                    }
                    if (binding.staticEvent) frame.stage = 1U;
                    if (frame.stage == 1U &&
                        !binding.customAccessorMemberOrdinal.has_value())
                    {
                        const ClassMemberSymbol& backing =
                            memberModel_->members()[binding.backingMemberOrdinal];
                        auto addressNode = makeNode(
                            binding.staticEvent
                                ? ProjectOopHirNodeKind::StaticFieldAddress
                                : ProjectOopHirNodeKind::FieldAddress,
                            current,
                            backing.declaredTypeKey);
                        addressNode->memberOrdinal = binding.backingMemberOrdinal;
                        if (!binding.staticEvent)
                            addressNode->children.push_back(std::move(frame.first));
                        frame.first = std::move(addressNode);
                    }
                }
                else if (*currentKind == IterativeExpressionKind::PropertyIndexerSetter)
                {
                    frame.propertySetterBinding = callBinding(current);
                    frame.callTarget = resolveBoundCallTarget(*frame.propertySetterBinding);
                    const ClassMemberSymbol& setter = memberModel_->members()[
                        *frame.propertySetterBinding->selectedMemberOrdinal];
                    if (setter.isIndexerAccessor && current->first() != nullptr &&
                        current->first()->kind() == ast::AstNodeKind::IndexExpression)
                    {
                        const ast::AstNode* indexExpression = current->first();
                        if (!indexExpression->children().empty())
                        {
                            frame.operandSyntaxes.assign(
                                indexExpression->children().begin(),
                                indexExpression->children().end());
                        }
                        else if (indexExpression->second() != nullptr)
                        {
                            frame.operandSyntaxes.push_back(indexExpression->second());
                        }
                    }
                    frame.operandSyntaxes.push_back(current->second());
                    frame.operands.reserve(frame.operandSyntaxes.size());
                    frame.compoundSetter = current->tokenKind() != lexer::TokenKind::Equal;
                    if (frame.compoundSetter)
                    {
                        if (currentAddressContext)
                        {
                            error = "the result of a compound assignment is a value, not addressable storage";
                            rollbackMatchCleanups();
                            return nullptr;
                        }
                        frame.compoundOperator =
                            lexer::compoundAssignmentBaseOperator(current->tokenKind());
                        if (frame.compoundOperator == lexer::TokenKind::Invalid)
                        {
                            error = "unsupported compound assignment operator on a property/indexer";
                            rollbackMatchCleanups();
                            return nullptr;
                        }
                    }
                    else
                    {
                        const std::optional<bool> receiverReady = prepareSetterReceiver(frame);
                        if (!receiverReady.has_value())
                        {
                            rollbackMatchCleanups();
                            return nullptr;
                        }
                        frame.stage = *receiverReady ? 1U : 0U;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::NamespaceExtensionSetter)
                {
                    frame.extensionSetterBinding = namespaceFunctionCallBinding(current);
                    frame.namespaceFunctionOrdinal =
                        *frame.extensionSetterBinding->selectedFunctionOrdinal;
                    const NamespaceFunctionSymbol& target = namespaceFunctionModel_->functions()[
                        frame.namespaceFunctionOrdinal];
                    if (currentAddressContext)
                    {
                        error = "the result of an extension-property assignment is a value, not addressable storage";
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    if (frame.extensionSetterBinding->forwardedReceiver &&
                        (frame.extensionSetterBinding->receiverSyntax == nullptr ||
                         target.parameters.empty()))
                    {
                        error = "extension setter lost its synthetic receiver contract";
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    frame.stage = frame.extensionSetterBinding->forwardedReceiver ? 0U : 1U;
                }
                else if (*currentKind == IterativeExpressionKind::NullConditionalFieldAssignment)
                {
                    frame.fieldBinding = nameBinding(current->first());
                    frame.stage = 0U;
                }
                else if (*currentKind == IterativeExpressionKind::NullConditionalArrayAssignment)
                {
                    frame.core = coreBinding(current->first());
                    frame.stage = 0U;
                }
                else if (*currentKind == IterativeExpressionKind::DirectAssignment ||
                    *currentKind == IterativeExpressionKind::CompoundAssignment ||
                    *currentKind == IterativeExpressionKind::StringCompoundAssignment)
                {
                    const MethodExpressionTypeBinding* assignmentType = expressionType(current);
                    const MethodDelegateBinding* assignmentDelegate = delegateBinding(current);
                    frame.assignmentResultType = assignmentType != nullptr
                        ? assignmentType->typeKey
                        : assignmentDelegate != nullptr ? assignmentDelegate->resultTypeKey : "<error>";
                    frame.assignmentOperation = current->tokenKind();
                    frame.assignmentOwnsStorage = assignmentType != nullptr &&
                        assignmentType->ownsStorageDestination;
                    if (*currentKind == IterativeExpressionKind::DirectAssignment)
                    {
                        const MethodNameBinding* targetBinding = nameBinding(current->first());
                        if (current->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
                            targetBinding != nullptr && targetBinding->localOrdinal.has_value() &&
                            methodBodyModel_ != nullptr &&
                            *targetBinding->localOrdinal < methodBodyModel_->locals().size() &&
                            (methodBodyModel_->locals()[*targetBinding->localOrdinal].isRaii ||
                             methodBodyModel_->locals()[*targetBinding->localOrdinal].requiresValueDrop))
                        {
                            frame.assignmentOwnsStorage = true;
                            frame.assignmentLocalOrdinal = targetBinding->localOrdinal;
                        }
                        if (!frame.assignmentOwnsStorage && memberModel_ != nullptr &&
                            memberModel_->typeRequiresValueDrop(frame.assignmentResultType))
                            frame.assignmentOwnsStorage = true;
                    }
                    else
                    {
                        if (currentAddressContext)
                        {
                            error = "the result of a compound assignment is a value, not addressable storage";
                            rollbackMatchCleanups();
                            return nullptr;
                        }
                        if (*currentKind == IterativeExpressionKind::StringCompoundAssignment)
                        {
                            const MethodNameBinding* targetBinding = nameBinding(current->first());
                            if (targetBinding == nullptr || !targetBinding->localOrdinal.has_value())
                            {
                                error = "string += requires an identifier-backed RAII string destination";
                                rollbackMatchCleanups();
                                return nullptr;
                            }
                            frame.assignmentLocalOrdinal = targetBinding->localOrdinal;
                        }
                        else
                        {
                            frame.operandType = expressionType(current->first());
                            if (frame.operandType == nullptr)
                            {
                                error = "Stage 34 compound assignment target has no resolved semantic type";
                                rollbackMatchCleanups();
                                return nullptr;
                            }
                        }
                    }
                    frame.stage = 0U;
                }
                frames.push_back(std::move(frame));
                if (*currentKind == IterativeExpressionKind::LoopExpression)
                    current = current->first();
                else if (*currentKind == IterativeExpressionKind::AwaitExpression)
                    current = current->first();
                else if (isBoundCallKind(*currentKind))
                {
                    const std::optional<BoundCallChild> child =
                        nextBoundCallChild(*frames.back().boundCall);
                    if (!child.has_value())
                    {
                        error = "Stage 34 bound-call frame has no child to lower";
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    current = child->syntax;
                    currentAddressContext = child->addressContext;
                }
                else if (*currentKind == IterativeExpressionKind::NamespaceCall)
                {
                    const std::optional<NamespaceCallChild> child =
                        nextNamespaceCallChild(*frames.back().namespaceCall);
                    if (!child.has_value())
                    {
                        error = "Stage 34 namespace-call frame has no child to lower";
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    current = child->syntax;
                    currentAddressContext = child->addressContext;
                }
                else if (*currentKind == IterativeExpressionKind::EventInvoke)
                {
                    const std::optional<EventInvokeChild> child =
                        nextEventInvokeChild(*frames.back().eventInvoke);
                    if (!child.has_value())
                    {
                        error = "Stage 34 event-invoke frame has no child to lower";
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    current = child->syntax;
                    currentAddressContext = child->addressContext;
                }
                else if (*currentKind == IterativeExpressionKind::DelegateInvoke)
                {
                    const std::optional<DelegateInvokeChild> child =
                        nextDelegateInvokeChild(frames.back());
                    if (!child.has_value())
                    {
                        error = "Stage 34 delegate-invoke frame has no child to lower";
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    current = child->syntax;
                    currentAddressContext = false;
                }
                else if (*currentKind == IterativeExpressionKind::ConsoleOutputCall ||
                    *currentKind == IterativeExpressionKind::ConsoleReadLineCall ||
                    *currentKind == IterativeExpressionKind::EnvironmentExitCall)
                {
                    current = frames.back().syntax->children().front();
                    currentAddressContext = false;
                }
                else if (*currentKind == IterativeExpressionKind::EnumToStringCall)
                {
                    current = frames.back().enumToStringCall->receiverSyntax;
                    currentAddressContext = false;
                }
                else if (*currentKind == IterativeExpressionKind::StructCloneCall)
                {
                    current = frames.back().operandSyntaxes.front();
                    currentAddressContext = false;
                }
                else if (*currentKind == IterativeExpressionKind::StructAggregateCall)
                {
                    current = frames.back().structAggregateCall->receiverSyntax;
                    currentAddressContext = false;
                }
                else if (*currentKind == IterativeExpressionKind::AssertionCore)
                {
                    current = current->children().front();
                    currentAddressContext = false;
                }
                else if (*currentKind == IterativeExpressionKind::IteratorCoreCall)
                {
                    current = frames.back().operandSyntaxes.front();
                    currentAddressContext = frames.back().operandAddressContext;
                }
                else if (*currentKind == IterativeExpressionKind::CoreOperandCall)
                {
                    const std::optional<CoreOperandCallChild> child =
                        nextCoreOperandCallChild(*frames.back().coreOperandCall);
                    if (!child.has_value())
                    {
                        error = "Stage 34 core operand-call frame has no child to lower";
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    current = child->syntax;
                    currentAddressContext = child->addressContext;
                }
                else if (*currentKind == IterativeExpressionKind::AtomicCoreCall)
                {
                    const std::optional<AtomicCoreCallChild> child =
                        nextAtomicCoreCallChild(frames.back());
                    if (!child.has_value())
                    {
                        error = "Stage 34 atomic core-call frame has no child to lower";
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    current = child->syntax;
                    currentAddressContext = false;
                }
                else if (*currentKind == IterativeExpressionKind::MonitorCoreCall)
                {
                    const std::optional<MonitorCoreCallChild> child =
                        nextMonitorCoreCallChild(*frames.back().monitorCoreCall);
                    if (!child.has_value())
                    {
                        error = "Stage 34 monitor core-call frame has no child to lower";
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    current = child->syntax;
                    currentAddressContext = false;
                }
                else if (*currentKind == IterativeExpressionKind::OrdinaryCall)
                {
                    const std::optional<OrdinaryCallChild> child =
                        nextOrdinaryCallChild(*frames.back().ordinaryCall);
                    if (!child.has_value())
                    {
                        error = "Stage 34 ordinary-call frame has no child to lower";
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    current = child->syntax;
                    currentAddressContext = child->addressContext;
                }
                else if (*currentKind == IterativeExpressionKind::AggregateCrossLowering)
                {
                    const std::optional<AggregateCrossChild> child =
                        nextAggregateCrossChild(*frames.back().aggregateCross);
                    if (!child.has_value())
                    {
                        if (frames.back().aggregateCross->phase !=
                            AggregateCrossFramePhase::Ready)
                        {
                            error = "Stage 34 aggregate frame has no child to lower";
                            if (!lastErrorRange_.isValid())
                                lastErrorRange_ = current->range();
                            rollbackMatchCleanups();
                            return nullptr;
                        }
                        lowered = finishAggregateCrossFrame(frames.back());
                        frames.pop_back();
                        completedWithoutChild = true;
                    }
                    else
                    {
                        current = child->syntax;
                        currentAddressContext = child->addressContext;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::OrdinaryConstruction)
                {
                    const std::optional<OrdinaryConstructionChild> child =
                        nextOrdinaryConstructionChild(
                            *frames.back().ordinaryConstruction);
                    if (!child.has_value())
                    {
                        lowered = finishOrdinaryConstructionFrame(frames.back());
                        if (lowered == nullptr)
                        {
                            if (!lastErrorRange_.isValid())
                                lastErrorRange_ = current->range();
                            rollbackMatchCleanups();
                            return nullptr;
                        }
                        frames.pop_back();
                        completedWithoutChild = true;
                    }
                    else
                    {
                        current = child->syntax;
                        currentAddressContext = false;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::AllocatorConstruction)
                {
                    const std::optional<AllocatorConstructionChild> child =
                        nextAllocatorConstructionChild(
                            *frames.back().allocatorConstruction);
                    if (!child.has_value())
                    {
                        error = "Stage 34 allocator-construction frame has no child to lower";
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    current = child->syntax;
                    currentAddressContext = child->addressContext;
                }
                else if (*currentKind == IterativeExpressionKind::DelegateCreationReceiver)
                {
                    current = frames.back().receiverFrame.syntax;
                    currentAddressContext =
                        isStructValueType(frames.back().receiverFrame.typeKey) &&
                        !frames.back().receiverFrame.materializeTemporary;
                }
                else if (*currentKind == IterativeExpressionKind::ExtensionPropertyGetter)
                    current = frames.back().extensionPropertyBinding->receiverSyntax;
                else if (*currentKind == IterativeExpressionKind::InterpolationHandler)
                    current = frames.back().operandSyntaxes.front();
                else if (*currentKind == IterativeExpressionKind::CollectionBuilder)
                {
                    current = frames.back().operandSyntaxes.front();
                    currentAddressContext = false;
                }
                else if (*currentKind == IterativeExpressionKind::ArrayConstruction)
                {
                    current = frames.back().operandSyntaxes.front();
                    currentAddressContext = false;
                }
                else if (*currentKind == IterativeExpressionKind::InstanceField)
                    current = frames.back().fieldBinding->receiverSyntax;
                else if (*currentKind == IterativeExpressionKind::RangeSlice)
                    current = frames.back().rangeBinding->receiverSyntax;
                else if (*currentKind == IterativeExpressionKind::CoreIndex ||
                    *currentKind == IterativeExpressionKind::RawIndex)
                    current = current->first();
                else if (*currentKind == IterativeExpressionKind::ResultPropagation)
                    current = frames.back().resultPropagation->operandSyntax;
                else if (*currentKind == IterativeExpressionKind::Match)
                    current = current->first();
                else if (*currentKind == IterativeExpressionKind::DeconstructionAssignment)
                    current = current->second();
                else if (*currentKind == IterativeExpressionKind::EventSubscription)
                    current = frames.back().stage == 0U
                        ? frames.back().eventSubscriptionBinding->receiverSyntax
                        : current->second();
                else if (*currentKind == IterativeExpressionKind::PropertyIndexerSetter)
                    current = frames.back().compoundSetter
                        ? current->first()
                        : frames.back().stage == 0U
                            ? frames.back().callTarget.receiverSyntax
                            : frames.back().operandSyntaxes.front();
                else if (*currentKind == IterativeExpressionKind::NamespaceExtensionSetter)
                    current = frames.back().stage == 0U
                        ? frames.back().extensionSetterBinding->receiverSyntax
                        : current->second();
                else if (*currentKind == IterativeExpressionKind::NullConditionalFieldAssignment ||
                    *currentKind == IterativeExpressionKind::NullConditionalArrayAssignment)
                    current = current->first()->first();
                else if (*currentKind == IterativeExpressionKind::DirectAssignment ||
                    *currentKind == IterativeExpressionKind::CompoundAssignment ||
                    *currentKind == IterativeExpressionKind::StringCompoundAssignment)
                    current = current->first();
                else if (*currentKind == IterativeExpressionKind::MoveProjection &&
                    !frames.back().move->sourceIsArrayElement &&
                    !frames.back().move->sourceIsOwnedField &&
                    frames.back().move->sourceIsIteratorCurrent)
                    current = current->first()->first();
                else
                    current = current->first();
                if (*currentKind == IterativeExpressionKind::LoopExpression)
                {
                    currentAddressContext = false;
                    currentInputKind = IterativeInputKind::LoopBlock;
                }
                else if (isBoundCallKind(*currentKind))
                {
                    // The common frame selected both the child and its address context above.
                }
                else if (*currentKind == IterativeExpressionKind::AllocatorConstruction)
                {
                    // The allocator frame selected receiver addressability versus argument value.
                }
                else if (*currentKind == IterativeExpressionKind::AggregateCrossLowering)
                {
                    // The aggregate frame selects value versus move-source address context.
                }
                else if (*currentKind == IterativeExpressionKind::OrdinaryConstruction)
                {
                    // Storage, arguments, and field initializers are value expressions.
                }
                else if (*currentKind == IterativeExpressionKind::ExtensionPropertyGetter)
                {
                    currentAddressContext = frames.back().operandAddressContext;
                }
                else if (*currentKind == IterativeExpressionKind::InterpolationHandler)
                {
                    currentAddressContext = false;
                    currentMemberOrdinal = frames.back().core->builderMemberOrdinals[1];
                }
                else if (*currentKind == IterativeExpressionKind::InstanceField)
                {
                    currentAddressContext =
                        isStructValueType(frames.back().fieldBinding->receiverTypeKey) &&
                        !frames.back().materializeReceiverTemporary;
                }
                else if (*currentKind == IterativeExpressionKind::RangeSlice)
                {
                    currentAddressContext =
                        isStructValueType(frames.back().rangeBinding->receiverTypeKey) &&
                        !frames.back().materializeReceiverTemporary;
                }
                else if (*currentKind == IterativeExpressionKind::RawIndex)
                {
                    currentAddressContext = frames.back().baseIsFixedArray ||
                        frames.back().baseIsFlexibleArray;
                }
                else if (*currentKind == IterativeExpressionKind::NullCoalescingAssignment)
                {
                    currentAddressContext = true;
                }
                else if (*currentKind == IterativeExpressionKind::Destruction)
                {
                    currentAddressContext = frames.back().operandAddressContext;
                }
                else if (*currentKind == IterativeExpressionKind::StorageUnary)
                {
                    currentAddressContext =
                        frames.back().syntax->tokenKind() != lexer::TokenKind::Star;
                }
                else if (*currentKind == IterativeExpressionKind::MoveProjection)
                {
                    currentAddressContext = true;
                }
                else if (*currentKind == IterativeExpressionKind::Match)
                {
                    currentAddressContext = frames.back().matchBinding->governingReference;
                }
                else if (*currentKind == IterativeExpressionKind::DeconstructionAssignment)
                {
                    currentAddressContext =
                        frames.back().deconstructionBinding->selectedMemberOrdinal.has_value() &&
                        isStructValueType(
                            frames.back().deconstructionBinding->sourceTypeKey) &&
                        !frames.back().materializeReceiverTemporary;
                }
                else if (*currentKind == IterativeExpressionKind::EventSubscription)
                {
                    currentAddressContext = frames.back().stage == 0U &&
                        isStructValueType(
                            frames.back().eventSubscriptionBinding->receiverTypeKey) &&
                        !frames.back().materializeReceiverTemporary;
                }
                else if (*currentKind == IterativeExpressionKind::PropertyIndexerSetter)
                {
                    currentAddressContext = !frames.back().compoundSetter &&
                        frames.back().stage == 0U &&
                        isStructValueType(frames.back().callTarget.receiverTypeKey) &&
                        !frames.back().materializeReceiverTemporary;
                    if (!frames.back().compoundSetter && frames.back().stage != 0U)
                    {
                        currentAddressContext = boundCallArgumentNeedsAddress(
                            frames.back().callTarget.memberOrdinal, 0U);
                    }
                }
                else if (*currentKind == IterativeExpressionKind::NamespaceExtensionSetter)
                {
                    currentAddressContext = false;
                }
                else if (*currentKind == IterativeExpressionKind::IteratorCoreCall)
                {
                    // The iterator frame selected value versus address context above.
                }
                else if (*currentKind == IterativeExpressionKind::CoreOperandCall)
                {
                    // The core-operand frame selected value versus address context above.
                }
                else if (*currentKind == IterativeExpressionKind::OrdinaryCall)
                {
                    // The ordinary-call frame selected receiver addressability versus argument value.
                }
                else if (*currentKind == IterativeExpressionKind::AtomicCoreCall)
                {
                    // Atomic locations and values are already resolved value expressions.
                }
                else if (*currentKind == IterativeExpressionKind::MonitorCoreCall)
                {
                    // Monitor receivers and Wait timeouts are value expressions.
                }
                else if (*currentKind == IterativeExpressionKind::ConsoleOutputCall ||
                    *currentKind == IterativeExpressionKind::ConsoleReadLineCall ||
                    *currentKind == IterativeExpressionKind::EnumToStringCall ||
                    *currentKind == IterativeExpressionKind::StructCloneCall ||
                    *currentKind == IterativeExpressionKind::StructAggregateCall ||
                    *currentKind == IterativeExpressionKind::EnvironmentExitCall)
                {
                    // H02S14K frames finalize or schedule their next value child below.
                }
                else if (*currentKind == IterativeExpressionKind::NullConditionalFieldAssignment ||
                    *currentKind == IterativeExpressionKind::NullConditionalArrayAssignment)
                {
                    currentAddressContext = false;
                }
                else if (*currentKind == IterativeExpressionKind::DirectAssignment ||
                    *currentKind == IterativeExpressionKind::CompoundAssignment ||
                    *currentKind == IterativeExpressionKind::StringCompoundAssignment)
                {
                    currentAddressContext = true;
                }
                else if (*currentKind != IterativeExpressionKind::OverflowPolicy)
                {
                    currentAddressContext = false;
                }
                if (*currentKind != IterativeExpressionKind::LoopExpression)
                    currentInputKind = IterativeInputKind::Expression;
                if (!completedWithoutChild) continue;
            }

            if (!completedWithoutChild)
            {
                const bool isExpressionStatementRoot =
                    frames.size() == 1u &&
                    frames.back().kind == IterativeExpressionKind::ExpressionStatement;
                const bool isVariableDeclarationChild =
                    !frames.empty() &&
                    frames.back().kind == IterativeExpressionKind::VariableDeclaration;
                const bool isDeconstructionDeclarationChild =
                    !frames.empty() &&
                    frames.back().kind ==
                        IterativeExpressionKind::DeconstructionDeclaration;
                const bool isInlineAssemblyOperandChild =
                    !frames.empty() &&
                    frames.back().kind ==
                        IterativeExpressionKind::InlineAssemblyStatement;
                const bool isStaticAssertChild =
                    !frames.empty() &&
                    frames.back().kind == IterativeExpressionKind::StaticAssertStatement;
                const bool isStatementExpressionChild =
                    !frames.empty() &&
                    (frames.back().kind == IterativeExpressionKind::IfStatement ||
                     frames.back().kind == IterativeExpressionKind::LoopStatement ||
                     frames.back().kind == IterativeExpressionKind::SwitchStatement) &&
                    frames.back().stage == 0U;
                const bool isForExpressionChild =
                    !frames.empty() &&
                    frames.back().kind == IterativeExpressionKind::ForStatement &&
                    frames.back().stage >= 1U && frames.back().stage <= 3U;
                lowered = isExpressionStatementRoot || isVariableDeclarationChild ||
                        isDeconstructionDeclarationChild ||
                        isInlineAssemblyOperandChild || isStaticAssertChild ||
                        isStatementExpressionChild || isForExpressionChild
                    ? lowerExpressionImpl(
                        current, currentMemberOrdinal, currentAddressContext, error)
                    : lowerExpression(
                        current, currentMemberOrdinal, currentAddressContext, error);
                if (lowered == nullptr)
                {
                    rollbackMatchCleanups();
                    if (!lastErrorRange_.isValid() && !frames.empty())
                    {
                        if ((frames.back().kind ==
                                 IterativeExpressionKind::VariableDeclaration ||
                             frames.back().kind ==
                                 IterativeExpressionKind::DeconstructionDeclaration ||
                             frames.back().kind ==
                                 IterativeExpressionKind::InlineAssemblyStatement ||
                             ((frames.back().kind ==
                                   IterativeExpressionKind::IfStatement ||
                               frames.back().kind ==
                                   IterativeExpressionKind::LoopStatement ||
                               frames.back().kind ==
                                   IterativeExpressionKind::SwitchStatement) &&
                              frames.back().stage == 0U) ||
                             (frames.back().kind ==
                                  IterativeExpressionKind::ForStatement &&
                              frames.back().stage >= 1U &&
                              frames.back().stage <= 3U)) &&
                            current != nullptr)
                        {
                            lastErrorRange_ = current->range();
                        }
                        else if (frames.back().kind !=
                            IterativeExpressionKind::ExpressionStatement)
                        {
                            lastErrorRange_ = frames.back().syntax->range();
                        }
                    }
                    return nullptr;
                }
            }
            }

            currentInputKind = IterativeInputKind::Expression;
            bool descend = false;
            while (!frames.empty())
            {
                IterativeExpressionFrame& frame = frames.back();
                if (frame.kind == IterativeExpressionKind::VariableDeclaration)
                {
                    const auto pendingChild =
                        nextVariableDeclarationChild(frame);
                    if (!pendingChild.has_value() ||
                        !applyExpressionBoundaryPostconditions(
                            lowered,
                            pendingChild->syntax,
                            frame.memberOrdinal))
                    {
                        rollbackMatchCleanups();
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        return nullptr;
                    }
                    if (!acceptVariableDeclarationChild(
                            frame, std::move(lowered)))
                    {
                        rollbackMatchCleanups();
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        return nullptr;
                    }
                    const auto child = nextVariableDeclarationChild(frame);
                    if (child.has_value())
                    {
                        current = child->syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = child->addressContext;
                        descend = true;
                        break;
                    }
                    lowered = finishVariableDeclarationFrame(frame);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind ==
                    IterativeExpressionKind::DeconstructionDeclaration)
                {
                    if (!acceptDeconstructionDeclarationChild(
                            frame, std::move(lowered)))
                    {
                        rollbackMatchCleanups();
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        return nullptr;
                    }
                    lowered = finishDeconstructionDeclarationFrame(frame);
                    if (lowered == nullptr)
                    {
                        rollbackMatchCleanups();
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        return nullptr;
                    }
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::InlineAssemblyStatement)
                {
                    InlineAssemblyFrameState& state = *frame.inlineAssembly;
                    const ast::AstNode* record = state.pendingOperand;
                    const ast::AstNode* operandSyntax =
                        record != nullptr ? record->first() : nullptr;
                    if (record == nullptr || operandSyntax == nullptr ||
                        !applyExpressionBoundaryPostconditions(
                            lowered, operandSyntax, frame.memberOrdinal) ||
                        !acceptInlineAssemblyOperand(frame, std::move(lowered)))
                    {
                        rollbackMatchCleanups();
                        if (!lastErrorRange_.isValid())
                        {
                            lastErrorRange_ = operandSyntax != nullptr
                                ? operandSyntax->range()
                                : frame.syntax->range();
                        }
                        return nullptr;
                    }
                    const auto child = advanceInlineAssemblyFrame(frame);
                    if (!error.empty())
                    {
                        rollbackMatchCleanups();
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        return nullptr;
                    }
                    if (child.has_value())
                    {
                        current = child->syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = child->addressContext;
                        descend = true;
                        break;
                    }
                    lowered = finishInlineAssemblyFrame(frame);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::StaticAssertStatement)
                {
                    const ast::AstNode* assertionSyntax = frame.syntax;
                    if (!applyExpressionBoundaryPostconditions(
                            lowered,
                            assertionSyntax->first(),
                            frame.memberOrdinal))
                    {
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    lowered = finishStaticAssertStatement(
                        assertionSyntax, std::move(lowered), error);
                    frames.pop_back();
                    if (lowered == nullptr)
                    {
                        rollbackMatchCleanups();
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = assertionSyntax->range();
                        return nullptr;
                    }
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::IfStatement)
                {
                    if (frame.stage == 0U)
                    {
                        if (!applyExpressionBoundaryPostconditions(
                                lowered, frame.syntax->first(), frame.memberOrdinal))
                        {
                            rollbackMatchCleanups();
                            return nullptr;
                        }
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        currentInputKind = IterativeInputKind::LoopBlock;
                        descend = true;
                        break;
                    }
                    if (frame.stage == 1U)
                    {
                        frame.second = std::move(lowered);
                        if (frame.statementConditionFailed)
                        {
                            error = std::move(frame.statementConditionError);
                            lowered.reset();
                            frames.pop_back();
                            if (resumePendingConditionStatementAfterFailure())
                                goto iterative_restart;
                            rollbackMatchCleanups();
                            return nullptr;
                        }
                        const ast::AstNode* alternative = frame.syntax->third();
                        if (alternative == nullptr)
                        {
                            frame.loopBlockNode->children.push_back(std::move(frame.first));
                            frame.loopBlockNode->children.push_back(std::move(frame.second));
                            lowered = std::move(frame.loopBlockNode);
                            frames.pop_back();
                            continue;
                        }
                        frame.stage = 2U;
                        if (alternative->kind() == ast::AstNodeKind::BlockStatement)
                        {
                            current = alternative;
                            currentMemberOrdinal = frame.memberOrdinal;
                            currentAddressContext = false;
                            currentInputKind = IterativeInputKind::LoopBlock;
                            descend = true;
                            break;
                        }
                        if (alternative->kind() == ast::AstNodeKind::IfStatement)
                        {
                            IterativeExpressionFrame nested;
                            nested.kind = IterativeExpressionKind::IfStatement;
                            nested.syntax = alternative;
                            nested.memberOrdinal = frame.memberOrdinal;
                            nested.loopBlockNode = makeNode(
                                ProjectOopHirNodeKind::If,
                                alternative,
                                "builtin:void");
                            frames.push_back(std::move(nested));
                            current = alternative->first();
                            currentMemberOrdinal = frames.back().memberOrdinal;
                            currentAddressContext = false;
                            currentInputKind = IterativeInputKind::Expression;
                            descend = true;
                            break;
                        }
                        if (alternative->kind() == ast::AstNodeKind::WhileStatement ||
                            alternative->kind() == ast::AstNodeKind::DoWhileStatement)
                        {
                            IterativeExpressionFrame nested;
                            nested.kind = IterativeExpressionKind::LoopStatement;
                            nested.syntax = alternative;
                            nested.memberOrdinal = frame.memberOrdinal;
                            nested.loopBlockNode = makeNode(
                                alternative->kind() == ast::AstNodeKind::DoWhileStatement
                                    ? ProjectOopHirNodeKind::DoWhile
                                    : ProjectOopHirNodeKind::While,
                                alternative,
                                "builtin:void");
                            frames.push_back(std::move(nested));
                            current = alternative->first();
                            currentMemberOrdinal = frames.back().memberOrdinal;
                            currentAddressContext = false;
                            currentInputKind = IterativeInputKind::Expression;
                            descend = true;
                            break;
                        }
                        if (alternative->kind() == ast::AstNodeKind::SwitchStatement)
                        {
                            IterativeExpressionFrame nested;
                            nested.memberOrdinal = frame.memberOrdinal;
                            if (!initializeSwitchStatementFrame(nested, alternative))
                            {
                                rollbackMatchCleanups();
                                return nullptr;
                            }
                            frames.push_back(std::move(nested));
                            current = alternative->first();
                            currentMemberOrdinal = frames.back().memberOrdinal;
                            currentAddressContext = false;
                            currentInputKind = IterativeInputKind::Expression;
                            descend = true;
                            break;
                        }
                        if (alternative->kind() == ast::AstNodeKind::ForStatement)
                        {
                            IterativeExpressionFrame nested;
                            nested.memberOrdinal = frame.memberOrdinal;
                            initializeForStatementFrame(nested, alternative);
                            frames.push_back(std::move(nested));
                            const LoopBlockAdvance advance =
                                advanceForStatement(frames.size() - 1U);
                            if (advance == LoopBlockAdvance::Error)
                            {
                                rollbackMatchCleanups();
                                return nullptr;
                            }
                            descend = true;
                            break;
                        }
                        if (alternative->kind() == ast::AstNodeKind::ForeachStatement)
                        {
                            IterativeExpressionFrame nested;
                            nested.memberOrdinal = frame.memberOrdinal;
                            if (!initializeForeachStatementFrame(nested, alternative))
                            {
                                rollbackMatchCleanups();
                                return nullptr;
                            }
                            frames.push_back(std::move(nested));
                            const LoopBlockAdvance advance =
                                advanceForeachStatement(frames.size() - 1U);
                            if (advance == LoopBlockAdvance::Error)
                            {
                                rollbackMatchCleanups();
                                return nullptr;
                            }
                            descend = true;
                            break;
                        }
                        if (isExitValueStatement(alternative))
                        {
                            if (!beginExitValueStatement(
                                    alternative, frame.memberOrdinal))
                            {
                                rollbackMatchCleanups();
                                return nullptr;
                            }
                            descend = true;
                            break;
                        }
                        lowered = lowerStatement(
                            alternative, frame.memberOrdinal, error);
                        if (lowered == nullptr)
                        {
                            rollbackMatchCleanups();
                            if (!lastErrorRange_.isValid())
                                lastErrorRange_ = alternative->range();
                            return nullptr;
                        }
                    }
                    frame.loopBlockNode->children.push_back(std::move(frame.first));
                    frame.loopBlockNode->children.push_back(std::move(frame.second));
                    frame.loopBlockNode->children.push_back(std::move(lowered));
                    lowered = std::move(frame.loopBlockNode);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::LoopStatement)
                {
                    if (frame.stage == 0U)
                    {
                        if (!applyExpressionBoundaryPostconditions(
                                lowered, frame.syntax->first(), frame.memberOrdinal))
                        {
                            rollbackMatchCleanups();
                            return nullptr;
                        }
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        currentInputKind = IterativeInputKind::LoopBlock;
                        descend = true;
                        break;
                    }
                    frame.second = std::move(lowered);
                    if (frame.statementConditionFailed)
                    {
                        error = std::move(frame.statementConditionError);
                        lowered.reset();
                        frames.pop_back();
                        if (resumePendingConditionStatementAfterFailure())
                            goto iterative_restart;
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    frame.loopBlockNode->children.push_back(std::move(frame.first));
                    frame.loopBlockNode->children.push_back(std::move(frame.second));
                    lowered = std::move(frame.loopBlockNode);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::SwitchStatement)
                {
                    if (frame.stage == 0U)
                    {
                        if (!applyExpressionBoundaryPostconditions(
                                lowered, frame.syntax->first(), frame.memberOrdinal))
                        {
                            rollbackMatchCleanups();
                            return nullptr;
                        }
                        frame.switchStatement->node->children.push_back(
                            std::move(lowered));
                        frame.stage = 1U;
                    }
                    else
                    {
                        activateDirectStatementCleanup(lowered);
                        frame.switchStatement->sectionBody->children.push_back(
                            std::move(lowered));
                        frame.stage = 1U;
                    }
                    const LoopBlockAdvance advance =
                        advanceSwitchStatement(frames.size() - 1U);
                    if (advance == LoopBlockAdvance::Error)
                    {
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    if (advance == LoopBlockAdvance::Descend)
                    {
                        descend = true;
                        break;
                    }
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::ForStatement)
                {
                    const ast::AstNode* childSyntax = nullptr;
                    bool expressionBoundary = false;
                    if (frame.stage == 1U)
                    {
                        childSyntax = frame.syntax->first();
                        expressionBoundary = childSyntax != nullptr &&
                            childSyntax->kind() != ast::AstNodeKind::BlockStatement &&
                            childSyntax->kind() !=
                                ast::AstNodeKind::VariableDeclarationStatement;
                    }
                    else if (frame.stage == 2U)
                    {
                        childSyntax = frame.syntax->second();
                        expressionBoundary = childSyntax != nullptr;
                    }
                    else if (frame.stage == 3U)
                    {
                        childSyntax = frame.syntax->third();
                        expressionBoundary = childSyntax != nullptr &&
                            childSyntax->kind() != ast::AstNodeKind::BlockStatement;
                    }
                    if (expressionBoundary &&
                        !applyExpressionBoundaryPostconditions(
                            lowered, childSyntax, frame.memberOrdinal))
                    {
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    if (frame.stage == 1U)
                        activateDirectStatementCleanup(lowered);
                    frame.forStatement->node->children.push_back(std::move(lowered));
                    if (frame.stage == 4U)
                    {
                        activeHirCleanupRegions_.resize(frame.cleanupDepth);
                        frame.cleanupActive = false;
                        lowered = std::move(frame.forStatement->node);
                        frames.pop_back();
                        continue;
                    }
                    const LoopBlockAdvance advance =
                        advanceForStatement(frames.size() - 1U);
                    if (advance == LoopBlockAdvance::Error)
                    {
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    if (advance == LoopBlockAdvance::Descend)
                    {
                        descend = true;
                        break;
                    }
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::ExitValueStatement)
                {
                    const ast::AstNode* valueSyntax = frame.syntax->first();
                    if (!applyExpressionBoundaryPostconditions(
                            lowered, valueSyntax, frame.memberOrdinal))
                    {
                        rollbackMatchCleanups();
                        return nullptr;
                    }
                    ExitValueStatementFrameState& state = *frame.exitValueStatement;
                    switch (frame.syntax->kind())
                    {
                        case ast::AstNodeKind::BreakStatement:
                            if (state.loopBreakBinding->conversion.exists() &&
                                state.loopBreakBinding->conversion.kind !=
                                    ProjectConversionKind::Identity)
                            {
                                lowered = wrapImplicitConversion(
                                    std::move(lowered),
                                    state.loopBreakBinding->conversion,
                                    valueSyntax);
                            }
                            state.node->elementTypeKey =
                                state.loopBreakBinding->resultTypeKey;
                            break;

                        case ast::AstNodeKind::ReturnStatement:
                            if (state.valueReturningCallable &&
                                state.declaredTypeKey != "builtin:void" &&
                                !state.referenceReturn)
                            {
                                const MethodExpressionTypeBinding* type =
                                    expressionType(valueSyntax);
                                const ProjectImplicitConversion conversion =
                                    classifyImplicit(
                                        lowered->typeKey,
                                        state.declaredTypeKey,
                                        type != nullptr
                                            ? type->integerConstant : std::nullopt);
                                if (conversion.exists() &&
                                    conversion.kind != ProjectConversionKind::Identity)
                                {
                                    lowered = wrapImplicitConversion(
                                        std::move(lowered), conversion, valueSyntax);
                                }
                                if (state.declaredOwnership ==
                                    sema::OwnershipKind::Borrowed)
                                {
                                    const MethodBorrowBinding* borrow =
                                        borrowBinding(valueSyntax);
                                    if (borrow == nullptr)
                                    {
                                        error = "Stage 50.8 borrowed return has no frozen borrow binding";
                                        rollbackMatchCleanups();
                                        return nullptr;
                                    }
                                    auto borrowed = makeNode(
                                        ProjectOopHirNodeKind::BorrowValue,
                                        valueSyntax,
                                        state.declaredTypeKey);
                                    borrowed->borrowKind = borrow->kind;
                                    borrowed->localOrdinal =
                                        borrow->sourceLocalOrdinal;
                                    borrowed->parameterOrdinal =
                                        borrow->sourceParameterOrdinal;
                                    borrowed->children.push_back(std::move(lowered));
                                    lowered = std::move(borrowed);
                                }
                            }
                            if (frame.syntax->flag())
                            {
                                if (lowered->kind != ProjectOopHirNodeKind::InstanceCall &&
                                    lowered->kind != ProjectOopHirNodeKind::StaticCall)
                                {
                                    error = "T08-F06 validated must_tail return did not lower to a direct call";
                                    rollbackMatchCleanups();
                                    return nullptr;
                                }
                                lowered->mustTailCall = true;
                            }
                            break;

                        case ast::AstNodeKind::ThrowStatement:
                            break;

                        default:
                            error = "non-value exit statement reached iterative HIR exit completion";
                            rollbackMatchCleanups();
                            return nullptr;
                    }
                    state.node->children.push_back(std::move(lowered));
                    lowered = std::move(state.node);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::ForeachStatement)
                {
                    ForeachStatementFrameState& state = *frame.foreachStatement;
                    if (frame.stage == 1U)
                    {
                        state.node->children.push_back(std::move(lowered));
                        frame.stage = 2U;
                        current = state.chosenCandidate;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        currentInputKind = IterativeInputKind::LoopBlock;
                        descend = true;
                        break;
                    }
                    state.node->children.push_back(std::move(lowered));
                    lowered = std::move(state.node);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::LoopBlockExpressionStatement)
                {
                    auto statement = makeNode(
                        ProjectOopHirNodeKind::ExpressionStatement,
                        frame.syntax,
                        "builtin:void");
                    statement->children.push_back(std::move(lowered));
                    lowered = std::move(statement);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::LoopBlock)
                {
                    activateLocalCleanup(lowered);
                    frame.loopBlockNode->children.push_back(std::move(lowered));
                    const ast::AstNode* blockSyntax = frame.syntax;
                    const LoopBlockAdvance advance = advanceLoopBlock(frames.size() - 1U);
                    if (advance == LoopBlockAdvance::Error)
                    {
                        rollbackMatchCleanups();
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = blockSyntax->range();
                        return nullptr;
                    }
                    if (advance == LoopBlockAdvance::Descend)
                    {
                        descend = true;
                        break;
                    }
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::LoopExpression)
                {
                    frame.first->children.push_back(std::move(lowered));
                    lowered = std::move(frame.first);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::AwaitExpression)
                {
                    const ast::AstNode* awaitSyntax = frame.syntax;
                    lowered = lowerAwaitExpression(
                        frame.syntax,
                        std::move(lowered),
                        frame.awaitMemberOrdinal,
                        std::move(frame.awaitCleanupRegionOrdinals),
                        frame.awaitExceptionTargetRegionOrdinal,
                        error);
                    frames.pop_back();
                    if (lowered == nullptr)
                    {
                        rollbackMatchCleanups();
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = awaitSyntax->range();
                        return nullptr;
                    }
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::ExpressionStatement)
                {
                    auto statement = makeNode(
                        ProjectOopHirNodeKind::ExpressionStatement,
                        frame.syntax,
                        "builtin:void");
                    statement->children.push_back(std::move(lowered));
                    lowered = std::move(statement);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::OverflowPolicy)
                {
                    auto policy = makeNode(
                        ProjectOopHirNodeKind::OverflowPolicy,
                        frame.syntax,
                        lowered->typeKey);
                    policy->stringValue = frame.syntax->auxiliaryText();
                    policy->children.push_back(std::move(lowered));
                    lowered = std::move(policy);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::BuiltinUnary)
                {
                    const MethodExpressionTypeBinding* frameType =
                        expressionType(frame.syntax);
                    const MethodDelegateBinding* frameDelegate =
                        delegateBinding(frame.syntax);
                    const std::string resultType = frameType != nullptr
                        ? frameType->typeKey
                        : frameDelegate != nullptr ? frameDelegate->resultTypeKey : "<error>";
                    auto unary = makeNode(
                        ProjectOopHirNodeKind::Unary, frame.syntax, resultType);
                    unary->operation = frame.syntax->tokenKind();
                    unary->children.push_back(std::move(lowered));
                    lowered = std::move(unary);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::DelegateCreationReceiver)
                {
                    acceptReceiverChild(frame.receiverFrame, std::move(lowered));
                    lowered = finishDelegateCreation(
                        frame.syntax,
                        *frame.delegateCreationBinding,
                        std::move(frame.receiverFrame.lowered));
                    frames.pop_back();
                    continue;
                }
                if (isBoundCallKind(frame.kind))
                {
                    BoundCallFrameState& boundCall = *frame.boundCall;
                    acceptBoundCallChild(boundCall, std::move(lowered));
                    const std::optional<BoundCallChild> child =
                        nextBoundCallChild(boundCall);
                    if (child.has_value())
                    {
                        current = child->syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = child->addressContext;
                        descend = true;
                        break;
                    }
                    auto call = finishBoundCall(
                        frame.syntax,
                        frame.addressContext,
                        error,
                        boundCall.typeBinding,
                        *boundCall.binding,
                        boundCall.target,
                        boundCall.argumentSyntaxes.size(),
                        boundCall.nullConditional,
                        std::move(boundCall.receiver.lowered),
                        std::move(boundCall.arguments),
                        boundCall.hasOwnedTemporary);
                    const bool accessor =
                        frame.kind == IterativeExpressionKind::PropertyGetter ||
                        frame.kind == IterativeExpressionKind::UserIndexerGetter;
                    if (call != nullptr && accessor &&
                        !validateNoHeapDirectCall(frame.memberOrdinal, *boundCall.binding, error))
                    {
                        call.reset();
                    }
                    if (call == nullptr)
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    if (frame.kind == IterativeExpressionKind::PropertyGetter &&
                        frame.flowProvenNonNull)
                    {
                        call->stringValue = "flow-nonnull";
                    }
                    lowered = std::move(call);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::Conversion)
                {
                    const MethodExpressionTypeBinding* frameType =
                        expressionType(frame.syntax);
                    const MethodDelegateBinding* frameDelegate =
                        delegateBinding(frame.syntax);
                    const std::string resultType = frameType != nullptr
                        ? frameType->typeKey
                        : frameDelegate != nullptr ? frameDelegate->resultTypeKey : "<error>";
                    const MethodRuntimeTypeBinding* runtime = runtimeTypeBinding(frame.syntax);
                    ProjectOopHirNodeKind nodeKind = ProjectOopHirNodeKind::Conversion;
                    if (frame.syntax->kind() == ast::AstNodeKind::TypeTestExpression ||
                        frame.syntax->kind() == ast::AstNodeKind::SafeCastExpression)
                    {
                        nodeKind = runtime != nullptr &&
                                runtime->kind == MethodRuntimeTypeOperationKind::TypeTest
                            ? ProjectOopHirNodeKind::RuntimeTypeTest
                            : ProjectOopHirNodeKind::SafeReferenceCast;
                    }
                    else if (runtime != nullptr &&
                        runtime->kind == MethodRuntimeTypeOperationKind::CheckedCast)
                    {
                        nodeKind = ProjectOopHirNodeKind::CheckedReferenceCast;
                    }
                    auto conversion = makeNode(nodeKind, frame.syntax, resultType);
                    if (runtime != nullptr)
                    {
                        conversion->targetTypeOrdinal = runtime->targetTypeOrdinal;
                        conversion->targetIsInterface = runtime->targetIsInterface;
                    }
                    conversion->children.push_back(std::move(lowered));
                    lowered = std::move(conversion);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::Match)
                {
                    const auto beginArm = [&]() -> bool
                    {
                        const ast::AstNode* armSyntax =
                            frame.syntax->children()[frame.armIndex];
                        if (armSyntax == nullptr || armSyntax->first() == nullptr ||
                            armSyntax->third() == nullptr)
                        {
                            error = "T03-F03 malformed match expression arm";
                            return false;
                        }
                        frame.matchArm = makeNode(
                            ProjectOopHirNodeKind::MatchArm,
                            armSyntax,
                            frame.matchNode->typeKey);
                        if (frame.armIndex < frame.matchBinding->armTags.size())
                            frame.matchArm->switchLabels =
                                frame.matchBinding->armTags[frame.armIndex];
                        frame.stage = 1U;
                        current = armSyntax->first();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        return true;
                    };
                    const auto advanceDeclarations = [&]() -> bool
                    {
                        const ast::AstNode* armSyntax =
                            frame.syntax->children()[frame.armIndex];
                        while (frame.syntheticLocalOrdinal <
                            armSyntax->children().size())
                        {
                            const ast::AstNode* declaration =
                                armSyntax->children()[frame.syntheticLocalOrdinal++];
                            if (declaration != nullptr &&
                                declaration->kind() ==
                                    ast::AstNodeKind::VariableDeclarationStatement)
                            {
                                IterativeExpressionFrame variableFrame;
                                variableFrame.kind =
                                    IterativeExpressionKind::VariableDeclaration;
                                variableFrame.syntax = declaration;
                                variableFrame.memberOrdinal = frame.memberOrdinal;
                                if (!initializeVariableDeclarationFrame(variableFrame))
                                    return false;
                                const auto child =
                                    nextVariableDeclarationChild(variableFrame);
                                if (!child.has_value())
                                {
                                    auto declarationNode =
                                        finishVariableDeclarationFrame(variableFrame);
                                    activateLocalCleanup(declarationNode);
                                    frame.matchArm->children.push_back(
                                        std::move(declarationNode));
                                    continue;
                                }
                                const std::size_t declarationMemberOrdinal =
                                    variableFrame.memberOrdinal;
                                frame.stage = 3U;
                                frames.push_back(std::move(variableFrame));
                                current = child->syntax;
                                currentMemberOrdinal = declarationMemberOrdinal;
                                currentAddressContext = child->addressContext;
                                return true;
                            }
                            if (declaration != nullptr &&
                                declaration->kind() ==
                                    ast::AstNodeKind::DeconstructionDeclarationStatement)
                            {
                                IterativeExpressionFrame declarationFrame;
                                declarationFrame.kind =
                                    IterativeExpressionKind::DeconstructionDeclaration;
                                declarationFrame.syntax = declaration;
                                declarationFrame.memberOrdinal = frame.memberOrdinal;
                                if (!initializeDeconstructionDeclarationFrame(
                                        declarationFrame))
                                {
                                    return false;
                                }
                                const auto child =
                                    nextDeconstructionDeclarationChild(
                                        declarationFrame);
                                if (!child.has_value())
                                {
                                    auto declarationNode =
                                        finishDeconstructionDeclarationFrame(
                                            declarationFrame);
                                    if (declarationNode == nullptr) return false;
                                    frame.matchArm->children.push_back(
                                        std::move(declarationNode));
                                    continue;
                                }
                                const std::size_t declarationMemberOrdinal =
                                    declarationFrame.memberOrdinal;
                                frame.stage = 3U;
                                frames.push_back(std::move(declarationFrame));
                                current = child->syntax;
                                currentMemberOrdinal = declarationMemberOrdinal;
                                currentAddressContext = child->addressContext;
                                return true;
                            }
                            if (declaration != nullptr &&
                                declaration->kind() ==
                                    ast::AstNodeKind::InlineAssemblyStatement)
                            {
                                IterativeExpressionFrame assemblyFrame;
                                assemblyFrame.kind =
                                    IterativeExpressionKind::InlineAssemblyStatement;
                                assemblyFrame.syntax = declaration;
                                assemblyFrame.memberOrdinal = frame.memberOrdinal;
                                initializeInlineAssemblyFrame(assemblyFrame);
                                const auto child =
                                    advanceInlineAssemblyFrame(assemblyFrame);
                                if (!error.empty()) return false;
                                if (!child.has_value())
                                {
                                    frame.matchArm->children.push_back(
                                        finishInlineAssemblyFrame(assemblyFrame));
                                    continue;
                                }
                                const std::size_t assemblyMemberOrdinal =
                                    assemblyFrame.memberOrdinal;
                                frame.stage = 3U;
                                frames.push_back(std::move(assemblyFrame));
                                current = child->syntax;
                                currentMemberOrdinal = assemblyMemberOrdinal;
                                currentAddressContext = child->addressContext;
                                return true;
                            }
                            if (declaration != nullptr &&
                                declaration->kind() == ast::AstNodeKind::IfStatement)
                            {
                                IterativeExpressionFrame ifFrame;
                                ifFrame.kind = IterativeExpressionKind::IfStatement;
                                ifFrame.syntax = declaration;
                                ifFrame.memberOrdinal = frame.memberOrdinal;
                                ifFrame.loopBlockNode = makeNode(
                                    ProjectOopHirNodeKind::If,
                                    declaration,
                                    "builtin:void");
                                const std::size_t declarationMemberOrdinal =
                                    ifFrame.memberOrdinal;
                                frame.stage = 3U;
                                frames.push_back(std::move(ifFrame));
                                current = declaration->first();
                                currentMemberOrdinal = declarationMemberOrdinal;
                                currentAddressContext = false;
                                return true;
                            }
                            if (declaration != nullptr &&
                                (declaration->kind() == ast::AstNodeKind::WhileStatement ||
                                 declaration->kind() == ast::AstNodeKind::DoWhileStatement))
                            {
                                IterativeExpressionFrame loopFrame;
                                loopFrame.kind = IterativeExpressionKind::LoopStatement;
                                loopFrame.syntax = declaration;
                                loopFrame.memberOrdinal = frame.memberOrdinal;
                                loopFrame.loopBlockNode = makeNode(
                                    declaration->kind() == ast::AstNodeKind::DoWhileStatement
                                        ? ProjectOopHirNodeKind::DoWhile
                                        : ProjectOopHirNodeKind::While,
                                    declaration,
                                    "builtin:void");
                                const std::size_t declarationMemberOrdinal =
                                    loopFrame.memberOrdinal;
                                frame.stage = 3U;
                                frames.push_back(std::move(loopFrame));
                                current = declaration->first();
                                currentMemberOrdinal = declarationMemberOrdinal;
                                currentAddressContext = false;
                                return true;
                            }
                            if (declaration != nullptr &&
                                declaration->kind() == ast::AstNodeKind::SwitchStatement)
                            {
                                IterativeExpressionFrame switchFrame;
                                switchFrame.memberOrdinal = frame.memberOrdinal;
                                if (!initializeSwitchStatementFrame(
                                        switchFrame, declaration))
                                {
                                    return false;
                                }
                                const std::size_t declarationMemberOrdinal =
                                    switchFrame.memberOrdinal;
                                frame.stage = 3U;
                                frames.push_back(std::move(switchFrame));
                                current = declaration->first();
                                currentMemberOrdinal = declarationMemberOrdinal;
                                currentAddressContext = false;
                                return true;
                            }
                            if (declaration != nullptr &&
                                declaration->kind() == ast::AstNodeKind::ForStatement)
                            {
                                IterativeExpressionFrame forFrame;
                                forFrame.memberOrdinal = frame.memberOrdinal;
                                initializeForStatementFrame(forFrame, declaration);
                                frame.stage = 3U;
                                frames.push_back(std::move(forFrame));
                                const LoopBlockAdvance advance =
                                    advanceForStatement(frames.size() - 1U);
                                return advance == LoopBlockAdvance::Descend;
                            }
                            if (declaration != nullptr &&
                                declaration->kind() == ast::AstNodeKind::ForeachStatement)
                            {
                                IterativeExpressionFrame foreachFrame;
                                foreachFrame.memberOrdinal = frame.memberOrdinal;
                                if (!initializeForeachStatementFrame(
                                        foreachFrame, declaration))
                                {
                                    return false;
                                }
                                frame.stage = 3U;
                                frames.push_back(std::move(foreachFrame));
                                const LoopBlockAdvance advance =
                                    advanceForeachStatement(frames.size() - 1U);
                                return advance == LoopBlockAdvance::Descend;
                            }
                            if (isExitValueStatement(declaration))
                            {
                                frame.stage = 3U;
                                return beginExitValueStatement(
                                    declaration, frame.memberOrdinal);
                            }
                            auto declarationNode = lowerStatement(
                                declaration, frame.memberOrdinal, error);
                            if (declarationNode == nullptr) return false;
                            activateLocalCleanup(declarationNode);
                            frame.matchArm->children.push_back(
                                std::move(declarationNode));
                        }
                        frame.stage = 4U;
                        current = armSyntax->third();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext =
                            frame.matchBinding->referenceResult;
                        return true;
                    };
                    const auto beginValue = [&]() -> bool
                    {
                        frame.cleanupDepth = activeHirCleanupRegions_.size();
                        frame.cleanupActive = true;
                        frame.syntheticLocalOrdinal = 0U;
                        if (advanceDeclarations()) return true;
                        activeHirCleanupRegions_.resize(frame.cleanupDepth);
                        frame.cleanupActive = false;
                        return false;
                    };
                    const auto finishMatch = [&]()
                    {
                        lowered = std::move(frame.matchNode);
                        if (frame.matchBinding->referenceResult && !frame.addressContext)
                        {
                            auto load = makeNode(
                                ProjectOopHirNodeKind::PointerLoad,
                                frame.syntax,
                                lowered->typeKey);
                            load->children.push_back(std::move(lowered));
                            lowered = std::move(load);
                        }
                    };
                    if (frame.stage == 0U)
                    {
                        frame.matchNode->children.push_back(std::move(lowered));
                        if (frame.syntax->children().empty())
                        {
                            finishMatch();
                            frames.pop_back();
                            continue;
                        }
                        if (!beginArm())
                        {
                            rollbackMatchCleanups();
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        descend = true;
                        break;
                    }
                    if (frame.stage == 1U)
                    {
                        frame.matchArm->children.push_back(std::move(lowered));
                        const ast::AstNode* guard =
                            frame.syntax->children()[frame.armIndex]->second();
                        if (guard != nullptr)
                        {
                            frame.stage = 2U;
                            current = guard;
                            currentMemberOrdinal = frame.memberOrdinal;
                            currentAddressContext = false;
                            descend = true;
                            break;
                        }
                        auto defaultGuard = makeNode(
                            ProjectOopHirNodeKind::BooleanConstant,
                            frame.syntax->children()[frame.armIndex],
                            "builtin:bool");
                        defaultGuard->booleanValue = true;
                        frame.matchArm->children.push_back(std::move(defaultGuard));
                        if (!beginValue())
                        {
                            rollbackMatchCleanups();
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        descend = true;
                        break;
                    }
                    if (frame.stage == 2U)
                    {
                        frame.matchArm->children.push_back(std::move(lowered));
                        if (!beginValue())
                        {
                            rollbackMatchCleanups();
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        descend = true;
                        break;
                    }
                    if (frame.stage == 3U)
                    {
                        activateLocalCleanup(lowered);
                        frame.matchArm->children.push_back(std::move(lowered));
                        if (!advanceDeclarations())
                        {
                            rollbackMatchCleanups();
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        descend = true;
                        break;
                    }
                    const ast::AstNode* armSyntax =
                        frame.syntax->children()[frame.armIndex];
                    if (!frame.matchBinding->referenceResult &&
                        frame.armIndex < frame.matchBinding->armConversions.size())
                    {
                        const ProjectImplicitConversion& conversion =
                            frame.matchBinding->armConversions[frame.armIndex];
                        if (conversion.exists() &&
                            conversion.kind != ProjectConversionKind::Identity)
                        {
                            lowered = wrapImplicitConversion(
                                std::move(lowered), conversion, armSyntax->third());
                        }
                    }
                    frame.matchArm->children.push_back(std::move(lowered));
                    frame.matchArm->cleanupRegionOrdinals.assign(
                        activeHirCleanupRegions_.begin() +
                            static_cast<std::ptrdiff_t>(frame.cleanupDepth),
                        activeHirCleanupRegions_.end());
                    activeHirCleanupRegions_.resize(frame.cleanupDepth);
                    frame.cleanupActive = false;
                    frame.matchNode->children.push_back(std::move(frame.matchArm));
                    ++frame.armIndex;
                    if (frame.armIndex < frame.syntax->children().size())
                    {
                        if (!beginArm())
                        {
                            rollbackMatchCleanups();
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        descend = true;
                        break;
                    }
                    finishMatch();
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::CoreMember)
                {
                    const MethodCoreBinding& core = *coreBinding(frame.syntax);
                    ProjectOopHirNodeKind nodeKind = ProjectOopHirNodeKind::IteratorCurrent;
                    if (core.kind == MethodCoreOperationKind::ArrayLength)
                        nodeKind = ProjectOopHirNodeKind::ArrayLength;
                    else if (core.kind == MethodCoreOperationKind::StringLength)
                        nodeKind = ProjectOopHirNodeKind::StringLength;
                    auto member = makeNode(nodeKind, frame.syntax, core.resultTypeKey);
                    member->elementTypeKey = core.elementTypeKey;
                    member->children.push_back(std::move(lowered));
                    lowered = std::move(member);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::CoreIndex)
                {
                    if (frame.stage == 0U)
                    {
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        const ast::AstNode* syntax = frame.operandSyntaxes.front();
                        current = syntax->kind() == ast::AstNodeKind::FromEndExpression
                            ? syntax->first() : syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    frame.operands.push_back(std::move(lowered));
                    if (frame.operands.size() < frame.operandSyntaxes.size())
                    {
                        const ast::AstNode* syntax =
                            frame.operandSyntaxes[frame.operands.size()];
                        current = syntax->kind() == ast::AstNodeKind::FromEndExpression
                            ? syntax->first() : syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }

                    const MethodCoreBinding& core = *frame.core;
                    if (core.kind == MethodCoreOperationKind::NullConditionalArrayElement)
                    {
                        auto element = makeNode(
                            ProjectOopHirNodeKind::NullConditionalArrayLoad,
                            frame.syntax,
                            core.resultTypeKey);
                        element->elementTypeKey = core.elementTypeKey;
                        element->children.push_back(std::move(frame.first));
                        element->children.push_back(std::move(frame.operands.front()));
                        lowered = std::move(element);
                        frames.pop_back();
                        continue;
                    }

                    ProjectOopHirNodeKind nodeKind = ProjectOopHirNodeKind::StringElementValue;
                    if (core.kind == MethodCoreOperationKind::ArrayElement)
                    {
                        nodeKind = frame.addressContext
                            ? ProjectOopHirNodeKind::ArrayElementAddress
                            : ProjectOopHirNodeKind::ArrayElementValue;
                    }
                    else if (core.kind == MethodCoreOperationKind::RangeArrayElement)
                    {
                        nodeKind = frame.addressContext
                            ? ProjectOopHirNodeKind::RangeArrayElementAddress
                            : ProjectOopHirNodeKind::RangeArrayElementValue;
                    }
                    auto element = makeNode(nodeKind, frame.syntax, core.resultTypeKey);
                    element->elementTypeKey = core.elementTypeKey;
                    element->nativeArrayRank = core.kind == MethodCoreOperationKind::RangeArrayElement
                        ? frame.operandSyntaxes.size() : 1U;
                    element->children.push_back(std::move(frame.first));
                    for (std::size_t dimension = 0;
                        dimension < frame.operandSyntaxes.size(); ++dimension)
                    {
                        const bool fromEnd = frame.operandSyntaxes[dimension]->kind() ==
                            ast::AstNodeKind::FromEndExpression;
                        if (dimension == 0U) element->booleanValue = fromEnd;
                        if (fromEnd && dimension < 64U)
                            element->integerValue |= (std::uint64_t{1} << dimension);
                        element->children.push_back(std::move(frame.operands[dimension]));
                    }
                    lowered = std::move(element);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::RawIndex)
                {
                    if (frame.stage == 0U)
                    {
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        current = frame.operandSyntaxes.front();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    frame.operands.push_back(std::move(lowered));
                    if (frame.operands.size() < frame.operandSyntaxes.size())
                    {
                        current = frame.operandSyntaxes[frame.operands.size()];
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }

                    const MethodCoreBinding& core = *frame.core;
                    if (frame.variableArrayIndex != nullptr)
                    {
                        std::unique_ptr<ProjectOopHirNode> linearIndex;
                        for (std::size_t dimension = 0;
                            dimension < frame.operands.size(); ++dimension)
                        {
                            auto index = std::move(frame.operands[dimension]);
                            if (index->typeKey != "builtin:nuint")
                            {
                                auto converted = makeNode(
                                    ProjectOopHirNodeKind::Conversion,
                                    frame.operandSyntaxes[dimension],
                                    "builtin:nuint");
                                converted->children.push_back(std::move(index));
                                index = std::move(converted);
                            }
                            if (linearIndex == nullptr)
                            {
                                linearIndex = std::move(index);
                                continue;
                            }
                            const std::size_t extentOrdinal =
                                frame.variableArrayIndex->extentLocalOrdinals[dimension];
                            auto extent = makeNode(
                                ProjectOopHirNodeKind::LocalValue,
                                frame.syntax,
                                methodBodyModel_->locals()[extentOrdinal].typeKey);
                            extent->localOrdinal = extentOrdinal;
                            if (extent->typeKey != "builtin:nuint")
                            {
                                auto convertedExtent = makeNode(
                                    ProjectOopHirNodeKind::Conversion,
                                    frame.syntax,
                                    "builtin:nuint");
                                convertedExtent->children.push_back(std::move(extent));
                                extent = std::move(convertedExtent);
                            }
                            auto rowOffset = makeNode(
                                ProjectOopHirNodeKind::Binary,
                                frame.syntax,
                                "builtin:nuint");
                            rowOffset->operation = lexer::TokenKind::Star;
                            rowOffset->children.push_back(std::move(linearIndex));
                            rowOffset->children.push_back(std::move(extent));
                            auto sum = makeNode(
                                ProjectOopHirNodeKind::Binary,
                                frame.syntax,
                                "builtin:nuint");
                            sum->operation = lexer::TokenKind::Plus;
                            sum->children.push_back(std::move(rowOffset));
                            sum->children.push_back(std::move(index));
                            linearIndex = std::move(sum);
                        }
                        auto address = makeNode(
                            ProjectOopHirNodeKind::RawElementAddress,
                            frame.syntax,
                            core.elementTypeKey);
                        address->elementTypeKey = core.elementTypeKey;
                        address->children.push_back(std::move(frame.first));
                        address->children.push_back(std::move(linearIndex));
                        if (frame.addressContext)
                        {
                            lowered = std::move(address);
                        }
                        else
                        {
                            auto value = makeNode(
                                ProjectOopHirNodeKind::PointerLoad,
                                frame.syntax,
                                core.resultTypeKey);
                            value->children.push_back(std::move(address));
                            lowered = std::move(value);
                        }
                        frames.pop_back();
                        continue;
                    }

                    const MethodExpressionTypeBinding* baseType =
                        expressionType(frame.syntax->first());
                    std::string indexedType = baseType != nullptr
                        ? baseType->typeKey : std::string();
                    std::unique_ptr<ProjectOopHirNode> base = std::move(frame.first);
                    for (std::size_t dimension = 0;
                        dimension < frame.operands.size(); ++dimension)
                    {
                        const std::optional<std::string> fixedElement = frame.baseIsFixedArray
                            ? ProjectMemberModel::fixedArrayElementTypeKey(indexedType)
                            : std::optional<std::string>();
                        const std::optional<std::string> flexibleElement = frame.baseIsFlexibleArray
                            ? ProjectMemberModel::flexibleArrayElementTypeKey(indexedType)
                            : std::optional<std::string>();
                        const std::string elementType = fixedElement.has_value()
                            ? *fixedElement
                            : flexibleElement.has_value() ? *flexibleElement : core.elementTypeKey;
                        auto address = makeNode(
                            ProjectOopHirNodeKind::RawElementAddress,
                            frame.syntax,
                            elementType);
                        address->elementTypeKey = elementType;
                        const ast::AstNode* baseSyntax = frame.syntax->first();
                        if (frame.baseIsFlexibleArray && metaprogrammingModel_ != nullptr &&
                            baseSyntax != nullptr && baseSyntax->first() != nullptr &&
                            inheritanceModel_ != nullptr && memberModel_ != nullptr &&
                            objectLayoutModel_ != nullptr)
                        {
                            const MethodExpressionTypeBinding* receiverType =
                                expressionType(baseSyntax->first());
                            if (receiverType != nullptr)
                            {
                                const std::optional<std::size_t> structOrdinal =
                                    inheritanceModel_->typeOrdinalForTypeKey(receiverType->typeKey);
                                if (structOrdinal.has_value())
                                {
                                    for (const StaticAttributeInfo& attribute :
                                         metaprogrammingModel_->attributes())
                                    {
                                        if (attribute.identity != "TrailingStorage" ||
                                            attribute.constantArguments.empty() ||
                                            !attribute.typeOrdinal.has_value() ||
                                            attribute.typeOrdinal != structOrdinal)
                                        {
                                            continue;
                                        }
                                        const std::span<const std::size_t> lengthMembers =
                                            memberModel_->findMembers(
                                                *structOrdinal,
                                                attribute.constantArguments[0]);
                                        if (!lengthMembers.empty() &&
                                            attribute.memberOrdinal.has_value())
                                        {
                                            const ObjectFieldLayout* lengthLayout =
                                                objectLayoutModel_->fieldLayoutForMember(
                                                    lengthMembers.front());
                                            const ObjectFieldLayout* dataLayout =
                                                objectLayoutModel_->fieldLayoutForMember(
                                                    *attribute.memberOrdinal);
                                            if (lengthLayout != nullptr && dataLayout != nullptr)
                                            {
                                                address->hasTrailingLengthBounds = true;
                                                address->trailingLengthRelativeOffset =
                                                    static_cast<std::int64_t>(lengthLayout->offset) -
                                                    static_cast<std::int64_t>(dataLayout->offset);
                                            }
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                        address->children.push_back(std::move(base));
                        address->children.push_back(std::move(frame.operands[dimension]));
                        base = std::move(address);
                        indexedType = elementType;
                    }
                    if (frame.addressContext)
                    {
                        lowered = std::move(base);
                    }
                    else
                    {
                        auto value = makeNode(
                            ProjectOopHirNodeKind::PointerLoad,
                            frame.syntax,
                            core.resultTypeKey);
                        value->children.push_back(std::move(base));
                        lowered = std::move(value);
                    }
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::NamespaceCall)
                {
                    NamespaceCallFrameState& namespaceCall = *frame.namespaceCall;
                    if (!acceptNamespaceCallChild(namespaceCall, std::move(lowered)))
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    const std::optional<NamespaceCallChild> child =
                        nextNamespaceCallChild(namespaceCall);
                    if (child.has_value())
                    {
                        current = child->syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = child->addressContext;
                        descend = true;
                        break;
                    }
                    lowered = finishNamespaceCall(
                        frame.syntax,
                        frame.addressContext,
                        error,
                        *namespaceCall.binding,
                        *namespaceCall.target,
                        std::move(namespaceCall.node),
                        namespaceCall.hasOwnedTemporary);
                    if (lowered == nullptr)
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        frames.clear();
                        break;
                    }
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::EventInvoke)
                {
                    if (!acceptEventInvokeChild(frame, std::move(lowered)))
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    EventInvokeFrameState& eventInvoke = *frame.eventInvoke;
                    const std::optional<EventInvokeChild> child =
                        nextEventInvokeChild(eventInvoke);
                    if (child.has_value())
                    {
                        current = child->syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = child->addressContext;
                        descend = true;
                        break;
                    }
                    eventInvoke.node->argumentCount = frame.syntax->children().size();
                    lowered = std::move(eventInvoke.node);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::DelegateInvoke)
                {
                    acceptDelegateInvokeChild(frame, std::move(lowered));
                    DelegateInvokeFrameState& delegateInvoke = *frame.delegateInvoke;
                    const std::optional<DelegateInvokeChild> child =
                        nextDelegateInvokeChild(frame);
                    if (child.has_value())
                    {
                        current = child->syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    lowered = finishDelegateInvoke(
                        frame.syntax,
                        error,
                        *delegateInvoke.binding,
                        std::move(delegateInvoke.node));
                    if (lowered == nullptr)
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        frames.clear();
                        break;
                    }
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::AssertionCore)
                {
                    const bool hasCondition =
                        frame.core->kind == MethodCoreOperationKind::Assert ||
                        frame.core->kind == MethodCoreOperationKind::Assume;
                    const bool validOperand = hasCondition
                        ? lowered->typeKey == "builtin:bool"
                        : lowered->typeKey == "builtin:string" ||
                            lowered->typeKey == "string-literal";
                    if (!validOperand)
                    {
                        error = hasCondition
                            ? "T07-F06 assertion condition lost its bool type"
                            : "T07-F06 panic payload lost its string type";
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    frame.first->children.push_back(std::move(lowered));
                    lowered = std::move(frame.first);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::IteratorCoreCall)
                {
                    frame.first->children.push_back(std::move(lowered));
                    if (!activeHirExceptionHandlers_.empty())
                    {
                        frame.first->exceptionTargetRegionOrdinal =
                            activeHirExceptionHandlers_.back().hirRegionOrdinal;
                    }
                    lowered = std::move(frame.first);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::CoreOperandCall)
                {
                    acceptCoreOperandCallChild(frame, std::move(lowered));
                    CoreOperandCallFrameState& call = *frame.coreOperandCall;
                    const std::optional<CoreOperandCallChild> child =
                        nextCoreOperandCallChild(call);
                    if (child.has_value())
                    {
                        current = child->syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = child->addressContext;
                        descend = true;
                        break;
                    }
                    lowered = std::move(call.node);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::AtomicCoreCall)
                {
                    if (!acceptAtomicCoreCallChild(frame, std::move(lowered)))
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    AtomicCoreCallFrameState& call = *frame.atomicCoreCall;
                    const std::optional<AtomicCoreCallChild> child =
                        nextAtomicCoreCallChild(frame);
                    if (child.has_value())
                    {
                        current = child->syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    lowered = std::move(call.node);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::MonitorCoreCall)
                {
                    acceptMonitorCoreCallChild(frame, std::move(lowered));
                    MonitorCoreCallFrameState& call = *frame.monitorCoreCall;
                    const std::optional<MonitorCoreCallChild> child =
                        nextMonitorCoreCallChild(call);
                    if (child.has_value())
                    {
                        current = child->syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    lowered = std::move(call.node);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::ConsoleOutputCall)
                {
                    ConsoleOutputCallFrameState& call = *frame.consoleOutputCall;
                    const ast::AstNode* argumentSyntax = frame.syntax->children().front();
                    if (call.binding != nullptr &&
                        !call.binding->selectedConversions.empty() &&
                        call.binding->selectedConversions.front().kind !=
                            ProjectConversionKind::Identity)
                    {
                        lowered = wrapImplicitConversion(
                            std::move(lowered),
                            call.binding->selectedConversions.front(),
                            argumentSyntax);
                    }
                    if (call.binding != nullptr &&
                        call.binding->selectedMemberOrdinal.has_value())
                    {
                        lowered = wrapCallArgumentBorrow(
                            std::move(lowered),
                            argumentSyntax,
                            *call.binding->selectedMemberOrdinal,
                            0U,
                            call.hasOwnedTemporary);
                    }
                    auto node = makeNode(
                        ProjectOopHirNodeKind::CoreConsoleWriteLine,
                        frame.syntax,
                        "builtin:void");
                    node->integerValue = call.mask;
                    node->children.push_back(std::move(lowered));
                    lowered = wrapFullExpressionCleanup(
                        std::move(node), frame.syntax, call.hasOwnedTemporary);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::ConsoleReadLineCall)
                {
                    auto node = makeNode(
                        ProjectOopHirNodeKind::CoreConsoleReadLine,
                        frame.syntax,
                        "builtin:string");
                    node->children.push_back(std::move(lowered));
                    lowered = std::move(node);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::EnumToStringCall)
                {
                    EnumToStringCallFrameState& call = *frame.enumToStringCall;
                    auto node = makeNode(
                        ProjectOopHirNodeKind::EnumName,
                        frame.syntax,
                        "builtin:string");
                    node->children.push_back(std::move(lowered));
                    auto fallback = makeNode(
                        ProjectOopHirNodeKind::StringLiteral,
                        frame.syntax,
                        "builtin:string");
                    fallback->stringValue = shortTypeName(frame.core->elementTypeKey);
                    node->children.push_back(std::move(fallback));
                    for (const auto& enumerator : call.enumerators)
                    {
                        auto constant = makeNode(
                            ProjectOopHirNodeKind::IntegerConstant,
                            frame.syntax,
                            frame.core->elementTypeKey);
                        constant->integerValue =
                            static_cast<std::uint64_t>(enumerator.first);
                        auto name = makeNode(
                            ProjectOopHirNodeKind::StringLiteral,
                            frame.syntax,
                            "builtin:string");
                        name->stringValue = enumerator.second;
                        node->children.push_back(std::move(constant));
                        node->children.push_back(std::move(name));
                    }
                    lowered = std::move(node);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::StructCloneCall)
                {
                    // Clone is intentionally transparent: return the exact receiver HIR.
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::StructAggregateCall)
                {
                    StructAggregateCallFrameState& call = *frame.structAggregateCall;
                    call.node->children.push_back(std::move(lowered));
                    if (call.stage == StructAggregateCallFrameStage::Receiver && call.equals)
                    {
                        call.stage = StructAggregateCallFrameStage::Argument;
                        current = frame.syntax->children().front();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    call.node->argumentCount = call.node->children.size();
                    if (!appendAggregateLeaves(
                            frame.core->elementTypeKey,
                            0,
                            frame.syntax,
                            call.node->children,
                            error))
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    call.stage = StructAggregateCallFrameStage::Ready;
                    lowered = std::move(call.node);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::EnvironmentExitCall)
                {
                    if (frame.environmentExitBinding != nullptr &&
                        !frame.environmentExitBinding->selectedConversions.empty() &&
                        frame.environmentExitBinding->selectedConversions.front().kind !=
                            ProjectConversionKind::Identity)
                    {
                        lowered = wrapImplicitConversion(
                            std::move(lowered),
                            frame.environmentExitBinding->selectedConversions.front(),
                            frame.syntax->children().front());
                    }
                    auto node = makeNode(
                        ProjectOopHirNodeKind::CoreEnvironmentExit,
                        frame.syntax,
                        "builtin:void");
                    node->children.push_back(std::move(lowered));
                    lowered = std::move(node);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::OrdinaryCall)
                {
                    OrdinaryCallFrameState& call = *frame.ordinaryCall;
                    if (!acceptOrdinaryCallChild(call, std::move(lowered)))
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    const std::optional<OrdinaryCallChild> child =
                        nextOrdinaryCallChild(call);
                    if (child.has_value())
                    {
                        current = child->syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = child->addressContext;
                        descend = true;
                        break;
                    }
                    lowered = finishOrdinaryCall(frame);
                    if (lowered == nullptr)
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        frames.clear();
                        break;
                    }
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::AggregateCrossLowering)
                {
                    AggregateCrossFrameState& aggregate = *frame.aggregateCross;
                    if (!acceptAggregateCrossChild(aggregate, std::move(lowered)))
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    const std::optional<AggregateCrossChild> child =
                        nextAggregateCrossChild(aggregate);
                    if (child.has_value())
                    {
                        current = child->syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = child->addressContext;
                        descend = true;
                        break;
                    }
                    if (aggregate.phase != AggregateCrossFramePhase::Ready)
                    {
                        error = "Stage 34 aggregate frame did not reach completion";
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    lowered = finishAggregateCrossFrame(frame);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::OrdinaryConstruction)
                {
                    OrdinaryConstructionFrameState& construction =
                        *frame.ordinaryConstruction;
                    if (!acceptOrdinaryConstructionChild(
                            construction, std::move(lowered)))
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    const std::optional<OrdinaryConstructionChild> child =
                        nextOrdinaryConstructionChild(construction);
                    if (child.has_value())
                    {
                        current = child->syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    lowered = finishOrdinaryConstructionFrame(frame);
                    if (lowered == nullptr)
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        frames.clear();
                        break;
                    }
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::AllocatorConstruction)
                {
                    AllocatorConstructionFrameState& construction =
                        *frame.allocatorConstruction;
                    if (!acceptAllocatorConstructionChild(
                            construction, std::move(lowered)))
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    const std::optional<AllocatorConstructionChild> child =
                        nextAllocatorConstructionChild(construction);
                    if (child.has_value())
                    {
                        current = child->syntax;
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = child->addressContext;
                        descend = true;
                        break;
                    }
                    lowered = finishAllocatorConstructionFrame(frame);
                    if (lowered == nullptr)
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        frames.clear();
                        break;
                    }
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::ExtensionPropertyGetter)
                {
                    auto getter = lowerExtensionPropertyGetterWithReceiver(
                        frame.syntax,
                        frame.memberOrdinal,
                        error,
                        *frame.extensionPropertyBinding,
                        std::move(lowered));
                    if (getter == nullptr)
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    lowered = std::move(getter);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::InterpolationHandler)
                {
                    frame.operands.push_back(std::move(lowered));
                    if (frame.operands.size() < frame.operandSyntaxes.size())
                    {
                        current = frame.operandSyntaxes[frame.operands.size()];
                        currentAddressContext = false;
                        currentMemberOrdinal = frame.core->builderMemberOrdinals[1];
                        descend = true;
                        break;
                    }
                    auto interpolation = buildInterpolationHandlerExpression(
                        frame.syntax,
                        *frame.core,
                        frame.syntheticLocalOrdinal,
                        frame.interpolationParts,
                        std::move(frame.operands),
                        error);
                    if (interpolation == nullptr)
                    {
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    lowered = std::move(interpolation);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::CollectionBuilder)
                {
                    frame.operands.push_back(std::move(lowered));
                    if (frame.operands.size() < frame.operandSyntaxes.size())
                    {
                        current = frame.operandSyntaxes[frame.operands.size()];
                        currentAddressContext = false;
                        currentMemberOrdinal = frame.memberOrdinal;
                        descend = true;
                        break;
                    }
                    lowered = finishCollectionBuilderExpression(
                        frame.syntax,
                        *frame.core,
                        frame.syntheticLocalOrdinal,
                        std::move(frame.operands));
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::ArrayConstruction)
                {
                    const std::size_t index = frame.armIndex++;
                    if (frame.core->kind == MethodCoreOperationKind::RangeArrayAllocation)
                    {
                        frame.first->children.push_back(std::move(lowered));
                    }
                    else if (frame.arraySegmented && frame.arraySpreadOperands[index])
                    {
                        frame.first->children.push_back(std::move(lowered));
                        frame.first->collectionSegments.push_back(ProjectCollectionSegmentKind::Spread);
                    }
                    else
                    {
                        const bool isCount = !frame.arraySegmented && !frame.arrayInferred && index == 0U;
                        if (!isCount)
                        {
                            const std::size_t initializerIndex = frame.arraySegmented
                                ? index : index - (frame.arrayInferred ? 0U : 1U);
                            const ast::AstNode* initializer =
                                frame.syntax->children()[initializerIndex];
                            const MethodExpressionTypeBinding* valueType = expressionType(initializer);
                            if (valueType == nullptr)
                            {
                                error = frame.arraySegmented
                                    ? "NSC-LIMIT-017 array spread element has no semantic type"
                                    : "NSC-LIMIT-017 array initializer element has no semantic type";
                                if (!lastErrorRange_.isValid())
                                    lastErrorRange_ = initializer->range();
                                lowered.reset(); frames.clear(); break;
                            }
                            const ProjectImplicitConversion conversion = classifyImplicit(
                                valueType->typeKey, frame.core->elementTypeKey, valueType->integerConstant);
                            if (!conversion.exists())
                            {
                                error = frame.arraySegmented
                                    ? "NSC-LIMIT-017 array spread element lost its validated conversion"
                                    : "NSC-LIMIT-017 array initializer lost its validated conversion";
                                if (!lastErrorRange_.isValid())
                                    lastErrorRange_ = initializer->range();
                                lowered.reset(); frames.clear(); break;
                            }
                            if (conversion.kind != ProjectConversionKind::Identity)
                                lowered = wrapImplicitConversion(std::move(lowered), conversion, initializer);
                        }
                        frame.first->children.push_back(std::move(lowered));
                        if (frame.arraySegmented)
                            frame.first->collectionSegments.push_back(ProjectCollectionSegmentKind::Element);
                    }
                    if (frame.armIndex < frame.operandSyntaxes.size())
                    {
                        current = frame.operandSyntaxes[frame.armIndex];
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true; break;
                    }
                    lowered = std::move(frame.first);
                    frames.pop_back(); continue;
                }
                if (frame.kind == IterativeExpressionKind::InstanceField)
                {
                    const MethodNameBinding& binding = *frame.fieldBinding;
                    if (frame.materializeReceiverTemporary)
                    {
                        auto temporary = makeNode(
                            ProjectOopHirNodeKind::TemporaryAddress,
                            binding.receiverSyntax,
                            binding.receiverTypeKey);
                        temporary->elementTypeKey = binding.receiverTypeKey;
                        temporary->children.push_back(std::move(lowered));
                        lowered = std::move(temporary);
                    }
                    auto field = makeNode(
                        frame.addressContext
                            ? ProjectOopHirNodeKind::FieldAddress
                            : ProjectOopHirNodeKind::FieldValue,
                        frame.syntax,
                        binding.typeKey);
                    field->memberOrdinal = binding.candidateMemberOrdinals.front();
                    field->volatileAccess = memberModel_ != nullptr &&
                        *field->memberOrdinal < memberModel_->members().size() &&
                        memberModel_->members()[*field->memberOrdinal].isVolatile;
                    field->baseReceiver = binding.baseReceiver;
                    field->children.push_back(std::move(lowered));
                    lowered = std::move(field);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::NullConditionalField)
                {
                    const MethodExpressionTypeBinding* frameType =
                        expressionType(frame.syntax);
                    const MethodDelegateBinding* frameDelegate =
                        delegateBinding(frame.syntax);
                    const std::string resultType = frameType != nullptr
                        ? frameType->typeKey
                        : frameDelegate != nullptr ? frameDelegate->resultTypeKey : "<error>";
                    const MethodExpressionTypeBinding* receiverFlow =
                        expressionType(frame.syntax->first());
                    const bool provenNonNull =
                        receiverFlow != nullptr && receiverFlow->flowProvenNonNull;
                    auto field = makeNode(
                        provenNonNull
                            ? ProjectOopHirNodeKind::FieldValue
                            : ProjectOopHirNodeKind::NullConditionalFieldValue,
                        frame.syntax,
                        resultType);
                    field->memberOrdinal = frame.fieldBinding->candidateMemberOrdinals.front();
                    field->booleanValue = provenNonNull;
                    field->volatileAccess = memberModel_ != nullptr &&
                        *field->memberOrdinal < memberModel_->members().size() &&
                        memberModel_->members()[*field->memberOrdinal].isVolatile;
                    field->children.push_back(std::move(lowered));
                    lowered = std::move(field);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::RangeSlice)
                {
                    const MethodRangeSliceBinding& range = *frame.rangeBinding;
                    if (frame.stage == 0U)
                    {
                        if (frame.materializeReceiverTemporary)
                        {
                            auto temporary = makeNode(
                                ProjectOopHirNodeKind::TemporaryAddress,
                                range.receiverSyntax,
                                range.receiverTypeKey);
                            temporary->elementTypeKey = range.receiverTypeKey;
                            temporary->children.push_back(std::move(lowered));
                            lowered = std::move(temporary);
                        }
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        if (range.startSyntax != nullptr)
                        {
                            current = range.startSyntax;
                            currentMemberOrdinal = frame.memberOrdinal;
                            currentAddressContext = false;
                            descend = true;
                            break;
                        }
                        lowered = makeNode(
                            ProjectOopHirNodeKind::IntegerConstant,
                            frame.syntax,
                            "builtin:int");
                        lowered->integerValue = 0;
                        continue;
                    }
                    if (frame.stage == 1U)
                    {
                        if (range.startSyntax != nullptr && range.startConversion.exists() &&
                            range.startConversion.kind != ProjectConversionKind::Identity)
                        {
                            lowered = wrapImplicitConversion(
                                std::move(lowered), range.startConversion, range.startSyntax);
                        }
                        frame.second = std::move(lowered);
                        frame.stage = 2U;
                        if (range.endSyntax != nullptr)
                        {
                            current = range.endSyntax;
                            currentMemberOrdinal = frame.memberOrdinal;
                            currentAddressContext = false;
                            descend = true;
                            break;
                        }
                        lowered = makeNode(
                            ProjectOopHirNodeKind::IntegerConstant,
                            frame.syntax,
                            "builtin:int");
                        lowered->integerValue = 0;
                        continue;
                    }
                    if (range.endSyntax != nullptr && range.endConversion.exists() &&
                        range.endConversion.kind != ProjectConversionKind::Identity)
                    {
                        lowered = wrapImplicitConversion(
                            std::move(lowered), range.endConversion, range.endSyntax);
                    }
                    const ClassMemberSymbol& sliceMember =
                        memberModel_->members()[range.sliceMemberOrdinal];
                    auto slice = makeNode(
                        ProjectOopHirNodeKind::RangeSlice,
                        frame.syntax,
                        range.resultTypeKey);
                    slice->elementTypeKey = range.receiverTypeKey;
                    slice->memberOrdinal = range.sliceMemberOrdinal;
                    slice->secondaryMemberOrdinal = range.lengthMemberOrdinal;
                    slice->referenceResult = sliceMember.isReferenceReturn;
                    slice->integerValue =
                        (range.hasStart ? 1U : 0U) |
                        (range.startFromEnd ? 2U : 0U) |
                        (range.hasEnd ? 4U : 0U) |
                        (range.endFromEnd ? 8U : 0U) |
                        (range.boundsStaticallyProven ? 16U : 0U);
                    slice->argumentCount = 2U;
                    slice->argumentParameterOrdinals = {0U, 1U};
                    slice->children.push_back(std::move(frame.first));
                    slice->children.push_back(std::move(frame.second));
                    slice->children.push_back(std::move(lowered));
                    lowered = std::move(slice);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::BuiltinBinary)
                {
                    if (frame.stage == 0U)
                    {
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    lowered = buildBuiltinBinaryExpression(
                        frame.syntax, std::move(frame.first), std::move(lowered));
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::CoreBinary)
                {
                    if (frame.stage == 0U)
                    {
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    if (frame.core->kind == MethodCoreOperationKind::StructEquals)
                    {
                        auto equality = makeNode(
                            ProjectOopHirNodeKind::AggregateEquals,
                            frame.syntax,
                            frame.core->resultTypeKey);
                        equality->elementTypeKey = frame.core->elementTypeKey;
                        equality->children.push_back(std::move(frame.first));
                        equality->children.push_back(std::move(lowered));
                        equality->argumentCount = equality->children.size();
                        if (!appendAggregateLeaves(
                                frame.core->elementTypeKey,
                                0,
                                frame.syntax,
                                equality->children,
                                error))
                        {
                            if (!lastErrorRange_.isValid())
                                lastErrorRange_ = frame.syntax->range();
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        if (frame.syntax->tokenKind() == lexer::TokenKind::BangEqual)
                        {
                            auto negated = makeNode(
                                ProjectOopHirNodeKind::Unary,
                                frame.syntax,
                                frame.core->resultTypeKey);
                            negated->operation = lexer::TokenKind::Bang;
                            negated->children.push_back(std::move(equality));
                            lowered = std::move(negated);
                        }
                        else
                        {
                            lowered = std::move(equality);
                        }
                    }
                    else
                    {
                        const ProjectOopHirNodeKind kind =
                            frame.core->kind == MethodCoreOperationKind::StringConcat
                                ? ProjectOopHirNodeKind::StringConcat
                                : frame.core->kind == MethodCoreOperationKind::StringEquals
                                    ? ProjectOopHirNodeKind::StringEquals
                                    : ProjectOopHirNodeKind::PointerArithmetic;
                        auto binary = makeNode(
                            kind, frame.syntax, frame.core->resultTypeKey);
                        binary->operation = frame.syntax->tokenKind();
                        if (frame.core->kind == MethodCoreOperationKind::PointerArithmetic)
                            binary->elementTypeKey = frame.core->elementTypeKey;
                        binary->children.push_back(std::move(frame.first));
                        binary->children.push_back(std::move(lowered));
                        lowered = std::move(binary);
                    }
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::NullCoalescing)
                {
                    if (frame.stage == 0U)
                    {
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    const MethodExpressionTypeBinding* frameType =
                        expressionType(frame.syntax);
                    const MethodDelegateBinding* frameDelegate =
                        delegateBinding(frame.syntax);
                    const std::string resultType = frameType != nullptr
                        ? frameType->typeKey
                        : frameDelegate != nullptr ? frameDelegate->resultTypeKey : "<error>";
                    const MethodExpressionTypeBinding* rhsType =
                        expressionType(frame.syntax->second());
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        lowered->typeKey,
                        resultType,
                        rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                    if (!conversion.exists())
                    {
                        error = "T01-F03 null-coalescing RHS cannot convert from '" +
                            lowered->typeKey + "' to '" + resultType + "'";
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    if (conversion.kind != ProjectConversionKind::Identity)
                    {
                        lowered = wrapImplicitConversion(
                            std::move(lowered), conversion, frame.syntax->second());
                    }
                    const MethodOptionalCarrierBinding* optional =
                        optionalCarrierBinding(frame.syntax);
                    auto coalescing = makeNode(
                        optional != nullptr
                            ? ProjectOopHirNodeKind::OptionalCoalescing
                            : ProjectOopHirNodeKind::NullCoalescing,
                        frame.syntax,
                        resultType);
                    if (optional != nullptr)
                    {
                        coalescing->elementTypeKey = optional->carrierTypeKey;
                        coalescing->memberOrdinal = optional->hasValueMemberOrdinal;
                        coalescing->secondaryMemberOrdinal =
                            optional->uncheckedValueMemberOrdinal;
                    }
                    coalescing->children.push_back(std::move(frame.first));
                    coalescing->children.push_back(std::move(lowered));
                    lowered = std::move(coalescing);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::NullCoalescingAssignment)
                {
                    if (frame.stage == 0U)
                    {
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    const MethodExpressionTypeBinding* targetType =
                        expressionType(frame.syntax->first());
                    const MethodExpressionTypeBinding* rhsType =
                        expressionType(frame.syntax->second());
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        lowered->typeKey,
                        targetType->typeKey,
                        rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                    if (!conversion.exists())
                    {
                        error = "T01-F03 null-coalescing assignment RHS cannot convert from '" +
                            lowered->typeKey + "' to '" + targetType->typeKey + "'";
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    if (conversion.kind != ProjectConversionKind::Identity)
                    {
                        lowered = wrapImplicitConversion(
                            std::move(lowered), conversion, frame.syntax->second());
                    }
                    auto assignment = makeNode(
                        ProjectOopHirNodeKind::NullCoalescingAssignment,
                        frame.syntax,
                        targetType->typeKey);
                    assignment->children.push_back(std::move(frame.first));
                    assignment->children.push_back(std::move(lowered));
                    lowered = std::move(assignment);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::DeconstructionAssignment)
                {
                    if (frame.stage == 0U)
                    {
                        if (frame.deconstructionBinding->selectedMemberOrdinal.has_value())
                        {
                            if (frame.materializeReceiverTemporary)
                            {
                                auto temporary = makeNode(
                                    ProjectOopHirNodeKind::TemporaryAddress,
                                    frame.syntax->second(),
                                    frame.deconstructionBinding->sourceTypeKey);
                                temporary->elementTypeKey =
                                    frame.deconstructionBinding->sourceTypeKey;
                                temporary->children.push_back(std::move(lowered));
                                lowered = std::move(temporary);
                            }
                            auto call = makeNode(
                                ProjectOopHirNodeKind::InstanceCall,
                                frame.syntax->second(),
                                frame.deconstructionBinding->tupleTypeKey);
                            call->memberOrdinal =
                                *frame.deconstructionBinding->selectedMemberOrdinal;
                            call->children.push_back(std::move(lowered));
                            lowered = std::move(call);
                        }
                        const auto sourceElements = ProjectTypeConversion::tupleElementTypeKeys(
                            frame.deconstructionBinding->tupleTypeKey);
                        if (!sourceElements.has_value() || objectLayoutModel_ == nullptr ||
                            nameLookup_ == nullptr)
                        {
                            error = "T02-F03 deconstruction source has no tuple layout";
                            if (!lastErrorRange_.isValid())
                                lastErrorRange_ = frame.syntax->range();
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        frame.deconstructionNode = makeNode(
                            ProjectOopHirNodeKind::TupleDeconstruct,
                            frame.syntax,
                            frame.deconstructionBinding->tupleTypeKey);
                        frame.deconstructionNode->children.push_back(std::move(lowered));
                        const auto expandTargetTuple = [&] (
                            const ast::AstNode* targetSyntax,
                            const std::string& sourceTypeKey,
                            std::uint64_t byteOffset,
                            std::vector<HirTuplePatternFrame>& children) -> bool
                        {
                            const auto nestedElements =
                                ProjectTypeConversion::tupleElementTypeKeys(sourceTypeKey);
                            const auto nestedLayout = objectLayoutModel_->tupleLayoutForTypeKey(
                                *nameLookup_, sourceTypeKey);
                            if (!nestedElements.has_value() || !nestedLayout.has_value() ||
                                nestedElements->size() != targetSyntax->children().size() ||
                                nestedLayout->offsets.size() != nestedElements->size())
                            {
                                error = "T02-F03 nested assignment target lost its tuple layout";
                                return false;
                            }
                            children.reserve(nestedElements->size());
                            for (std::size_t index = 0; index < nestedElements->size(); ++index)
                                children.push_back(HirTuplePatternFrame{
                                    targetSyntax->children()[index],
                                    (*nestedElements)[index],
                                    byteOffset + nestedLayout->offsets[index]});
                            return true;
                        };
                        const auto collectTargetLeaf = [&] (
                            const ast::AstNode* targetSyntax,
                            const std::string& sourceTypeKey,
                            std::uint64_t byteOffset) -> bool
                        {
                            if (targetSyntax->kind() == ast::AstNodeKind::IdentifierExpression &&
                                targetSyntax->text() == "_") return true;
                            frame.deconstructionLeaves.push_back(
                                HirTuplePatternFrame{targetSyntax, sourceTypeKey, byteOffset});
                            return true;
                        };
                        if (!visitHirTuplePatternLeaves(
                                frame.syntax->first(),
                                frame.deconstructionBinding->tupleTypeKey,
                                0,
                                expandTargetTuple,
                                collectTargetLeaf))
                        {
                            if (!lastErrorRange_.isValid())
                                lastErrorRange_ = frame.syntax->range();
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        if (frame.deconstructionLeaves.empty())
                        {
                            lowered = std::move(frame.deconstructionNode);
                            frames.pop_back();
                            continue;
                        }
                        frame.stage = 1U;
                    }
                    else
                    {
                        const HirTuplePatternFrame& leaf =
                            frame.deconstructionLeaves[frame.armIndex];
                        const ProjectImplicitConversion conversion = classifyImplicit(
                            leaf.typeKey, frame.operandType->typeKey, std::nullopt);
                        if (!conversion.exists())
                        {
                            error =
                                "T02-F03 deconstruction lost its validated element conversion";
                            if (!lastErrorRange_.isValid())
                                lastErrorRange_ = frame.syntax->range();
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        auto element = makeNode(
                            ProjectOopHirNodeKind::TupleDeconstructElement,
                            leaf.syntax,
                            frame.operandType->typeKey);
                        element->elementTypeKey = leaf.typeKey;
                        element->integerValue = leaf.byteOffset;
                        element->conversion = conversion;
                        element->children.push_back(std::move(lowered));
                        frame.deconstructionNode->children.push_back(std::move(element));
                        ++frame.armIndex;
                        if (frame.armIndex == frame.deconstructionLeaves.size())
                        {
                            lowered = std::move(frame.deconstructionNode);
                            frames.pop_back();
                            continue;
                        }
                    }
                    const ast::AstNode* targetSyntax =
                        frame.deconstructionLeaves[frame.armIndex].syntax;
                    frame.operandType = expressionType(targetSyntax);
                    if (frame.operandType == nullptr)
                    {
                        error = "T02-F03 deconstruction target has no semantic type";
                        if (!lastErrorRange_.isValid())
                            lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    current = targetSyntax;
                    currentMemberOrdinal = frame.memberOrdinal;
                    currentAddressContext = true;
                    descend = true;
                    break;
                }
                if (frame.kind == IterativeExpressionKind::EventSubscription)
                {
                    const MethodEventBinding& binding = *frame.eventSubscriptionBinding;
                    if (frame.stage == 0U)
                    {
                        if (frame.materializeReceiverTemporary)
                        {
                            auto temporary = makeNode(
                                ProjectOopHirNodeKind::TemporaryAddress,
                                binding.receiverSyntax,
                                binding.receiverTypeKey);
                            temporary->elementTypeKey = binding.receiverTypeKey;
                            temporary->children.push_back(std::move(lowered));
                            lowered = std::move(temporary);
                        }
                        if (!binding.customAccessorMemberOrdinal.has_value())
                        {
                            const ClassMemberSymbol& backing =
                                memberModel_->members()[binding.backingMemberOrdinal];
                            auto addressNode = makeNode(
                                ProjectOopHirNodeKind::FieldAddress,
                                frame.syntax,
                                backing.declaredTypeKey);
                            addressNode->memberOrdinal = binding.backingMemberOrdinal;
                            addressNode->children.push_back(std::move(lowered));
                            lowered = std::move(addressNode);
                        }
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    if (!binding.argumentConversions.empty() &&
                        binding.argumentConversions.front().kind !=
                            ProjectConversionKind::Identity)
                    {
                        lowered = wrapImplicitConversion(
                            std::move(lowered),
                            binding.argumentConversions.front(),
                            frame.syntax->second());
                    }
                    auto subscription = makeNode(
                        binding.customAccessorMemberOrdinal.has_value()
                            ? ProjectOopHirNodeKind::InstanceCall
                            : binding.kind == MethodEventOperationKind::Add
                                ? ProjectOopHirNodeKind::EventAdd
                                : ProjectOopHirNodeKind::EventRemove,
                        frame.syntax,
                        "builtin:void");
                    if (binding.customAccessorMemberOrdinal.has_value())
                    {
                        subscription->memberOrdinal =
                            *binding.customAccessorMemberOrdinal;
                        if (!binding.staticEvent)
                            subscription->children.push_back(std::move(frame.first));
                        subscription->argumentParameterOrdinals.push_back(0U);
                        subscription->argumentTypeKeys.push_back(binding.delegateTypeKey);
                    }
                    else
                    {
                        subscription->elementTypeKey = binding.delegateTypeKey;
                        subscription->memberOrdinal = binding.backingMemberOrdinal;
                        subscription->children.push_back(std::move(frame.first));
                    }
                    subscription->children.push_back(std::move(lowered));
                    lowered = std::move(subscription);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::PropertyIndexerSetter)
                {
                    const auto materializeReceiver = [&]()
                    {
                        frame.first = std::move(lowered);
                        if (!frame.materializeReceiverTemporary) return;
                        auto temporary = makeNode(
                            ProjectOopHirNodeKind::TemporaryAddress,
                            frame.callTarget.receiverSyntax,
                            std::string(frame.callTarget.receiverTypeKey));
                        temporary->elementTypeKey = frame.callTarget.receiverTypeKey;
                        temporary->children.push_back(std::move(frame.first));
                        frame.first = std::move(temporary);
                    };
                    if (frame.compoundSetter && frame.stage == 0U)
                    {
                        frame.second = std::move(lowered);
                        frame.stage = 1U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    if (frame.compoundSetter && frame.stage == 1U)
                    {
                        const MethodExpressionTypeBinding* rhsType =
                            expressionType(frame.syntax->second());
                        const ProjectImplicitConversion conversion = classifyImplicit(
                            lowered->typeKey,
                            frame.second->typeKey,
                            rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                        const bool shift = frame.compoundOperator == lexer::TokenKind::LessLess ||
                            frame.compoundOperator == lexer::TokenKind::GreaterGreater;
                        if (!shift && !conversion.exists())
                        {
                            error = "compound assignment to a property/indexer cannot convert its "
                                "right-hand side from '" + lowered->typeKey + "' to '" +
                                frame.second->typeKey + "'";
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        if (!shift && conversion.kind != ProjectConversionKind::Identity)
                        {
                            lowered = wrapImplicitConversion(
                                std::move(lowered), conversion, frame.syntax->second());
                        }
                        frame.setterCombinedValue = makeNode(
                            ProjectOopHirNodeKind::Binary,
                            frame.syntax,
                            frame.second->typeKey);
                        frame.setterCombinedValue->operation = frame.compoundOperator;
                        frame.setterCombinedValue->children.push_back(std::move(frame.second));
                        frame.setterCombinedValue->children.push_back(std::move(lowered));
                        const std::optional<bool> receiverReady = prepareSetterReceiver(frame);
                        if (!receiverReady.has_value())
                        {
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        frame.stage = *receiverReady ? 3U : 2U;
                        if (!*receiverReady)
                        {
                            current = frame.callTarget.receiverSyntax;
                            currentMemberOrdinal = frame.memberOrdinal;
                            currentAddressContext =
                                isStructValueType(frame.callTarget.receiverTypeKey) &&
                                !frame.materializeReceiverTemporary;
                            descend = true;
                            break;
                        }
                    }
                    else if (frame.compoundSetter && frame.stage == 2U)
                    {
                        materializeReceiver();
                        frame.stage = 3U;
                    }
                    else if (frame.compoundSetter && frame.stage == 3U)
                    {
                        frame.operands.push_back(std::move(lowered));
                    }
                    else if (!frame.compoundSetter && frame.stage == 0U)
                    {
                        materializeReceiver();
                        frame.stage = 1U;
                    }
                    else if (!frame.compoundSetter)
                    {
                        frame.operands.push_back(std::move(lowered));
                    }
                    const std::size_t targetCount = frame.compoundSetter
                        ? frame.operandSyntaxes.size() - 1U
                        : frame.operandSyntaxes.size();
                    if (frame.operands.size() < targetCount)
                    {
                        current = frame.operandSyntaxes[frame.operands.size()];
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = boundCallArgumentNeedsAddress(
                            frame.callTarget.memberOrdinal, frame.operands.size());
                        descend = true;
                        break;
                    }
                    if (frame.compoundSetter)
                        frame.operands.push_back(std::move(frame.setterCombinedValue));
                    auto setterNode = lowerBoundCall(
                        frame.syntax,
                        frame.memberOrdinal,
                        frame.addressContext,
                        error,
                        expressionType(frame.syntax),
                        frame.propertySetterBinding,
                        frame.operandSyntaxes,
                        nullptr,
                        false,
                        std::move(frame.first),
                        &frame.operands);
                    if (setterNode == nullptr || !validateNoHeapDirectCall(
                            frame.memberOrdinal, *frame.propertySetterBinding, error))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = frame.syntax->range();
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    setterNode->returnsAssignedValue = true;
                    lowered = std::move(setterNode);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::NamespaceExtensionSetter)
                {
                    const NamespaceFunctionCallBinding& binding =
                        *frame.extensionSetterBinding;
                    if (frame.stage == 0U)
                    {
                        frame.first = std::move(lowered);
                        if (!binding.selectedConversions.empty() &&
                            binding.selectedConversions.front().kind !=
                                ProjectConversionKind::Identity)
                        {
                            frame.first = wrapImplicitConversion(
                                std::move(frame.first), binding.selectedConversions.front(),
                                binding.receiverSyntax);
                        }
                        frame.stage = 1U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    const NamespaceFunctionSymbol& target = namespaceFunctionModel_->functions()[
                        frame.namespaceFunctionOrdinal];
                    const std::size_t valueParameterOrdinal =
                        binding.forwardedReceiver ? 1U : 0U;
                    if (valueParameterOrdinal >= target.parameters.size())
                    {
                        error = "extension setter has no value parameter";
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    if (valueParameterOrdinal < binding.selectedConversions.size() &&
                        binding.selectedConversions[valueParameterOrdinal].kind !=
                            ProjectConversionKind::Identity)
                    {
                        lowered = wrapImplicitConversion(
                            std::move(lowered),
                            binding.selectedConversions[valueParameterOrdinal],
                            frame.syntax->second());
                    }
                    auto setter = makeNode(
                        ProjectOopHirNodeKind::StaticCall,
                        frame.syntax,
                        binding.resultTypeKey);
                    setter->namespaceFunctionOrdinal = frame.namespaceFunctionOrdinal;
                    setter->stringValue = target.mangledName;
                    if (binding.forwardedReceiver)
                    {
                        setter->children.push_back(std::move(frame.first));
                        setter->argumentParameterOrdinals.push_back(0U);
                        setter->argumentTypeKeys.push_back(target.parameters[0].typeKey);
                    }
                    setter->children.push_back(std::move(lowered));
                    setter->argumentParameterOrdinals.push_back(valueParameterOrdinal);
                    setter->argumentTypeKeys.push_back(
                        target.parameters[valueParameterOrdinal].typeKey);
                    setter->returnsAssignedValue = true;
                    lowered = std::move(setter);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::NullConditionalFieldAssignment)
                {
                    if (frame.stage == 0U)
                    {
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    const MethodExpressionTypeBinding* assignmentType = expressionType(frame.syntax);
                    const MethodDelegateBinding* assignmentDelegate = delegateBinding(frame.syntax);
                    const std::string resultType = assignmentType != nullptr
                        ? assignmentType->typeKey
                        : assignmentDelegate != nullptr ? assignmentDelegate->resultTypeKey : "<error>";
                    auto store = makeNode(
                        ProjectOopHirNodeKind::NullConditionalFieldStore,
                        frame.syntax,
                        resultType);
                    store->operation = frame.syntax->tokenKind();
                    store->booleanValue = assignmentType != nullptr &&
                        assignmentType->ownsStorageDestination;
                    store->memberOrdinal = frame.fieldBinding->candidateMemberOrdinals.front();
                    store->volatileAccess = memberModel_ != nullptr &&
                        *store->memberOrdinal < memberModel_->members().size() &&
                        memberModel_->members()[*store->memberOrdinal].isVolatile;
                    store->children.push_back(std::move(frame.first));
                    store->children.push_back(std::move(lowered));
                    lowered = std::move(store);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::NullConditionalArrayAssignment)
                {
                    if (frame.stage == 0U)
                    {
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        current = !frame.syntax->first()->children().empty()
                            ? frame.syntax->first()->children().front()
                            : frame.syntax->first()->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    if (frame.stage == 1U)
                    {
                        frame.second = std::move(lowered);
                        frame.stage = 2U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    const MethodExpressionTypeBinding* assignmentType = expressionType(frame.syntax);
                    const MethodDelegateBinding* assignmentDelegate = delegateBinding(frame.syntax);
                    const std::string resultType = assignmentType != nullptr
                        ? assignmentType->typeKey
                        : assignmentDelegate != nullptr ? assignmentDelegate->resultTypeKey : "<error>";
                    auto store = makeNode(
                        ProjectOopHirNodeKind::NullConditionalArrayStore,
                        frame.syntax,
                        resultType);
                    store->elementTypeKey = frame.core->elementTypeKey;
                    store->booleanValue = assignmentType != nullptr &&
                        assignmentType->ownsStorageDestination;
                    store->children.push_back(std::move(frame.first));
                    store->children.push_back(std::move(frame.second));
                    store->children.push_back(std::move(lowered));
                    lowered = std::move(store);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::DirectAssignment)
                {
                    if (frame.stage == 0U)
                    {
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    const MethodExpressionTypeBinding* rhsType = expressionType(frame.syntax->second());
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        lowered->typeKey, frame.first->typeKey,
                        rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                    if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
                        lowered = wrapImplicitConversion(std::move(lowered), conversion, frame.syntax->second());
                    auto assignment = makeNode(
                        ProjectOopHirNodeKind::Assignment, frame.syntax, frame.assignmentResultType);
                    assignment->operation = frame.assignmentOperation;
                    assignment->booleanValue = frame.assignmentOwnsStorage;
                    assignment->localOrdinal = frame.assignmentLocalOrdinal;
                    assignment->children.push_back(std::move(frame.first));
                    assignment->children.push_back(std::move(lowered));
                    lowered = std::move(assignment);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::StringCompoundAssignment)
                {
                    if (frame.stage == 0U)
                    {
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        current = frame.syntax->first();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    if (frame.stage == 1U)
                    {
                        frame.second = std::move(lowered);
                        frame.stage = 2U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    auto concatenated = makeNode(
                        ProjectOopHirNodeKind::StringConcat, frame.syntax, "builtin:string");
                    concatenated->operation = lexer::TokenKind::Plus;
                    concatenated->children.push_back(std::move(frame.second));
                    concatenated->children.push_back(std::move(lowered));
                    auto assignment = makeNode(
                        ProjectOopHirNodeKind::Assignment, frame.syntax, frame.assignmentResultType);
                    assignment->operation = lexer::TokenKind::Equal;
                    assignment->booleanValue = true;
                    assignment->localOrdinal = frame.assignmentLocalOrdinal;
                    assignment->children.push_back(std::move(frame.first));
                    assignment->children.push_back(std::move(concatenated));
                    lowered = std::move(assignment);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::CompoundAssignment)
                {
                    if (frame.stage == 0U)
                    {
                        frame.first = std::move(lowered);
                        frame.stage = 1U;
                        current = frame.syntax->second();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = false;
                        descend = true;
                        break;
                    }
                    auto compound = makeNode(
                        ProjectOopHirNodeKind::CompoundStore, frame.syntax, frame.operandType->typeKey);
                    compound->operation = lexer::compoundAssignmentBaseOperator(frame.assignmentOperation);
                    if (compound->operation == lexer::TokenKind::Invalid)
                    {
                        error = "unsupported compound assignment operator";
                        lowered.reset();
                        frames.clear();
                        break;
                    }
                    compound->booleanValue = false;
                    if (ProjectTypeConversion::isPointer(frame.operandType->typeKey))
                    {
                        compound->elementTypeKey = frame.operandType->typeKey.substr(
                            0U, frame.operandType->typeKey.size() - 1U);
                    }
                    else
                    {
                        const MethodExpressionTypeBinding* rhsType = expressionType(frame.syntax->second());
                        const ProjectImplicitConversion conversion = classifyImplicit(
                            lowered->typeKey, frame.operandType->typeKey,
                            rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                        const bool shift = compound->operation == lexer::TokenKind::LessLess ||
                            compound->operation == lexer::TokenKind::GreaterGreater;
                        if (!shift && !conversion.exists())
                        {
                            error = "compound assignment cannot convert its right-hand side from '" +
                                lowered->typeKey + "' to '" + frame.operandType->typeKey + "'";
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        if (!shift && conversion.kind != ProjectConversionKind::Identity)
                            lowered = wrapImplicitConversion(std::move(lowered), conversion, frame.syntax->second());
                    }
                    compound->children.push_back(std::move(frame.first));
                    compound->children.push_back(std::move(lowered));
                    lowered = std::move(compound);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::ResultPropagation)
                {
                    const MethodExpressionTypeBinding* frameType =
                        expressionType(frame.syntax);
                    const MethodDelegateBinding* frameDelegate =
                        delegateBinding(frame.syntax);
                    const std::string resultType = frameType != nullptr
                        ? frameType->typeKey
                        : frameDelegate != nullptr ? frameDelegate->resultTypeKey : "<error>";
                    auto propagation = makeNode(
                        ProjectOopHirNodeKind::ResultPropagation,
                        frame.syntax,
                        resultType);
                    propagation->resultSourceCarrierTypeKey =
                        frame.resultPropagation->sourceCarrierTypeKey;
                    propagation->resultSourceErrorTypeKey =
                        frame.resultPropagation->sourceErrorTypeKey;
                    propagation->resultReturnCarrierTypeKey =
                        frame.resultPropagation->returnCarrierTypeKey;
                    propagation->resultReturnErrorTypeKey =
                        frame.resultPropagation->returnErrorTypeKey;
                    propagation->resultSuccessTag = frame.resultPropagation->sourceSuccessTag;
                    propagation->resultErrorTag = frame.resultPropagation->sourceErrorTag;
                    propagation->resultReturnErrorTag = frame.resultPropagation->returnErrorTag;
                    propagation->resultSuccessTestMemberOrdinal =
                        frame.resultPropagation->sourceSuccessTestMemberOrdinal;
                    propagation->resultSuccessExtractorMemberOrdinal =
                        frame.resultPropagation->sourceSuccessExtractorMemberOrdinal;
                    propagation->resultErrorExtractorMemberOrdinal =
                        frame.resultPropagation->sourceErrorExtractorMemberOrdinal;
                    propagation->resultErrorFactoryMemberOrdinal =
                        frame.resultPropagation->returnErrorFactoryMemberOrdinal;
                    propagation->resultErrorConversionMemberOrdinal =
                        frame.resultPropagation->errorConversionMemberOrdinal;
                    propagation->cleanupRegionOrdinals =
                        frame.structuredExit->cleanupRegionOrdinals;
                    propagation->cleanupContinuationKind =
                        MethodCleanupContinuationKind::Return;
                    propagation->children.push_back(std::move(lowered));
                    lowered = std::move(propagation);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::Destruction)
                {
                    if (frame.syntax->kind() == ast::AstNodeKind::DeleteExpression)
                    {
                        auto deallocation = makeNode(
                            ProjectOopHirNodeKind::Deallocation,
                            frame.syntax,
                            "builtin:void");
                        deallocation->elementTypeKey = frame.operandType->typeKey;
                        deallocation->booleanValue = frame.operandAddressContext;
                        if (frame.syntax->flag())
                        {
                            deallocation->dynamicDestruction = false;
                            deallocation->storageOwnership = frame.operandType->storageOwnership;
                        }
                        else
                        {
                            const bool rawPointer = frame.operandType->typeKey.ends_with("*");
                            const bool nativeString =
                                frame.operandType->typeKey == "builtin:string";
                            const bool deletable = frame.operandType->typeKey == "builtin:null" ||
                                isReferenceType(frame.operandType->typeKey) ||
                                rawPointer || nativeString;
                            deallocation->dynamicDestruction =
                                isReferenceType(frame.operandType->typeKey) && !nativeString;
                            deallocation->valueTypeNoOpDeletion = !deletable;
                            if (!deallocation->valueTypeNoOpDeletion)
                            {
                                deallocation->objectLifetimeOwnership =
                                    deallocation->dynamicDestruction
                                        ? sema::OwnershipKind::Owned
                                        : sema::OwnershipKind::None;
                                deallocation->storageOwnership = sema::OwnershipKind::Owned;
                            }
                        }
                        deallocation->children.push_back(std::move(lowered));
                        lowered = std::move(deallocation);
                        frames.pop_back();
                        continue;
                    }

                    bool dynamicDestruction = isReferenceType(frame.operandType->typeKey);
                    if (!dynamicDestruction && frame.operandType->typeKey.ends_with("*"))
                    {
                        const std::string pointee = frame.operandType->typeKey.substr(
                            0, frame.operandType->typeKey.size() - 1);
                        dynamicDestruction = isReferenceType(pointee);
                    }
                    auto destruction = makeNode(
                        ProjectOopHirNodeKind::ObjectDestruction,
                        frame.syntax,
                        "builtin:void");
                    destruction->dynamicDestruction = dynamicDestruction;
                    destruction->elementTypeKey = frame.operandType->typeKey;
                    destruction->objectLifetimeOwnership = sema::OwnershipKind::Owned;
                    destruction->storageOwnership = sema::OwnershipKind::Borrowed;
                    destruction->children.push_back(std::move(lowered));
                    lowered = std::move(destruction);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::StackAllocation)
                {
                    constexpr std::uint64_t kMaximumStackallocBytes = std::uint64_t{1} << 20;
                    if (lowered->kind == ProjectOopHirNodeKind::IntegerConstant)
                    {
                        if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
                        {
                            error = "CC1 stackalloc requires the object layout model to size it";
                            if (!lastErrorRange_.isValid())
                                lastErrorRange_ = frame.syntax->range();
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        const std::optional<std::pair<std::uint64_t, std::uint64_t>> elementLayout =
                            objectLayoutModel_->valueLayoutForTypeKey(
                                *nameLookup_, frame.core->elementTypeKey);
                        if (!elementLayout.has_value())
                        {
                            error = "stackalloc cannot size '" + frame.core->elementTypeKey +
                                "': it has no value layout";
                            if (!lastErrorRange_.isValid())
                                lastErrorRange_ = frame.syntax->range();
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                        const std::uint64_t elementSize = elementLayout->first;
                        const std::uint64_t elements = lowered->integerValue;
                        if (elementSize != 0 &&
                            elements > kMaximumStackallocBytes / elementSize)
                        {
                            error = "stackalloc of " + std::to_string(elements) + " x " +
                                std::to_string(elementSize) + " bytes does not fit on the stack; "
                                "the default reserve is " +
                                std::to_string(kMaximumStackallocBytes) +
                                " bytes -- allocate it on the heap instead";
                            if (!lastErrorRange_.isValid())
                                lastErrorRange_ = frame.syntax->range();
                            lowered.reset();
                            frames.clear();
                            break;
                        }
                    }
                    auto allocation = makeNode(
                        ProjectOopHirNodeKind::StackAllocation,
                        frame.syntax,
                        frame.core->resultTypeKey);
                    allocation->elementTypeKey = frame.core->elementTypeKey;
                    allocation->children.push_back(std::move(lowered));
                    lowered = std::move(allocation);
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::StorageUnary)
                {
                    const MethodExpressionTypeBinding* frameType = expressionType(frame.syntax);
                    const MethodDelegateBinding* frameDelegate = delegateBinding(frame.syntax);
                    const std::string resultType = frameType != nullptr
                        ? frameType->typeKey
                        : frameDelegate != nullptr ? frameDelegate->resultTypeKey : "<error>";
                    if (frame.syntax->tokenKind() == lexer::TokenKind::PlusPlus ||
                        frame.syntax->tokenKind() == lexer::TokenKind::MinusMinus)
                    {
                        const MethodExpressionTypeBinding* targetType =
                            expressionType(frame.syntax->first());
                        auto step = makeNode(
                            ProjectOopHirNodeKind::CompoundStore,
                            frame.syntax,
                            targetType->typeKey);
                        step->operation = frame.syntax->tokenKind() == lexer::TokenKind::PlusPlus
                            ? lexer::TokenKind::Plus
                            : lexer::TokenKind::Minus;
                        step->booleanValue =
                            frame.syntax->kind() == ast::AstNodeKind::PostfixExpression;
                        std::unique_ptr<ProjectOopHirNode> one;
                        if (ProjectTypeConversion::isPointer(targetType->typeKey))
                        {
                            step->elementTypeKey = targetType->typeKey.substr(
                                0, targetType->typeKey.size() - 1);
                            one = makeNode(
                                ProjectOopHirNodeKind::IntegerConstant,
                                frame.syntax,
                                "builtin:nint");
                            one->integerValue = 1;
                        }
                        else if (ProjectTypeConversion::isNumeric(targetType->typeKey) &&
                                 !ProjectTypeConversion::isIntegral(targetType->typeKey))
                        {
                            one = makeNode(
                                ProjectOopHirNodeKind::FloatingConstant,
                                frame.syntax,
                                targetType->typeKey);
                            one->floatingValue = 1.0;
                        }
                        else
                        {
                            one = makeNode(
                                ProjectOopHirNodeKind::IntegerConstant,
                                frame.syntax,
                                targetType->typeKey);
                            one->integerValue = 1;
                        }
                        step->children.push_back(std::move(lowered));
                        step->children.push_back(std::move(one));
                        lowered = std::move(step);
                    }
                    else if (frame.syntax->tokenKind() == lexer::TokenKind::Amp)
                    {
                        auto address = makeNode(
                            ProjectOopHirNodeKind::AddressValue,
                            frame.syntax,
                            resultType);
                        address->children.push_back(std::move(lowered));
                        lowered = std::move(address);
                    }
                    else if (!frame.addressContext)
                    {
                        auto load = makeNode(
                            ProjectOopHirNodeKind::PointerLoad,
                            frame.syntax,
                            resultType);
                        load->children.push_back(std::move(lowered));
                        lowered = std::move(load);
                    }
                    frames.pop_back();
                    continue;
                }
                if (frame.kind == IterativeExpressionKind::MoveProjection)
                {
                    const ProjectOopHirNodeKind kind = frame.move->sourceIsArrayElement
                        ? ProjectOopHirNodeKind::MoveArrayElement
                        : frame.move->sourceIsOwnedField
                            ? ProjectOopHirNodeKind::MoveOwnedField
                            : ProjectOopHirNodeKind::MoveIteratorCurrent;
                    auto move = makeNode(kind, frame.syntax, frame.move->typeKey);
                    move->booleanValue = frame.move->clearSourceStorage;
                    move->children.push_back(std::move(lowered));
                    lowered = std::move(move);
                    frames.pop_back();
                    continue;
                }
                if (frame.stage == 0U)
                {
                    frame.first = std::move(lowered);
                    frame.stage = 1U;
                    current = frame.syntax->second();
                    currentMemberOrdinal = frame.memberOrdinal;
                    currentAddressContext = false;
                    descend = true;
                    break;
                }
                if (frame.stage == 1U)
                {
                    frame.second = std::move(lowered);
                    frame.stage = 2U;
                    current = frame.syntax->third();
                    currentMemberOrdinal = frame.memberOrdinal;
                    currentAddressContext = false;
                    descend = true;
                    break;
                }
                lowered = buildConditionalExpression(
                    frame.syntax,
                    std::move(frame.first),
                    std::move(frame.second),
                    std::move(lowered));
                frames.pop_back();
            }
            if (lowered == nullptr && !error.empty()) return nullptr;
            if (!descend) break;
            }
            return &worklistComplete;
            }();
            if (schedulerResult != nullptr) break;
            if (resumePendingConditionStatementAfterFailure()) continue;
            rollbackMatchCleanups();
            return nullptr;
        }
    }
    else
    {
        lowered = lowerExpressionImpl(expression, memberOrdinal, addressContext, error);
    }
    const ClassMemberSymbol* activeMember = memberModel_ != nullptr &&
            memberOrdinal < memberModel_->members().size()
        ? &memberModel_->members()[memberOrdinal]
        : nullptr;
    const sema::EffectContext effects(activeMember != nullptr
        ? activeMember->effects
        : sema::EffectSet{});
    const auto directlyAllocatesHeap = [](ProjectOopHirNodeKind kind) noexcept
    {
        switch (kind)
        {
            case ProjectOopHirNodeKind::ArrayAllocation:
            case ProjectOopHirNodeKind::RangeArrayAllocation:
            case ProjectOopHirNodeKind::RawAllocation:
            case ProjectOopHirNodeKind::StringConcat:
            case ProjectOopHirNodeKind::StringSubstring:
            case ProjectOopHirNodeKind::StringFromCharArray:
            case ProjectOopHirNodeKind::DelegateCreate:
            case ProjectOopHirNodeKind::LambdaCreate:
            case ProjectOopHirNodeKind::EventAdd:
                return true;
            default:
                return false;
        }
    };
    const ProjectOopHirNode* loweredExpression = lowered.get();
    if (statementContinuation != nullptr &&
        statementContinuation->kind() == ast::AstNodeKind::ExpressionStatement &&
        loweredExpression != nullptr &&
        loweredExpression->kind == ProjectOopHirNodeKind::ExpressionStatement &&
        loweredExpression->children.size() == 1U)
    {
        loweredExpression = loweredExpression->children.front().get();
    }
    const bool statementChildPolicyApplied = statementContinuation != nullptr &&
        (statementContinuation->kind() == ast::AstNodeKind::StaticAssertStatement ||
         statementContinuation->kind() == ast::AstNodeKind::IfStatement ||
         statementContinuation->kind() == ast::AstNodeKind::WhileStatement ||
         statementContinuation->kind() == ast::AstNodeKind::DoWhileStatement ||
         statementContinuation->kind() == ast::AstNodeKind::SwitchStatement ||
         statementContinuation->kind() == ast::AstNodeKind::ForStatement ||
         statementContinuation->kind() == ast::AstNodeKind::ForeachStatement ||
         statementContinuation->kind() == ast::AstNodeKind::BreakStatement ||
         statementContinuation->kind() == ast::AstNodeKind::ReturnStatement ||
         statementContinuation->kind() == ast::AstNodeKind::ThrowStatement);
    if (!statementChildPolicyApplied && lowered != nullptr &&
        effects.allows(sema::Effect::NoHeap) &&
        loweredExpression != nullptr && directlyAllocatesHeap(loweredExpression->kind))
    {
        error = "NS3030: noheap callable '" + activeMember->name +
            "' cannot contain direct heap allocation '" +
            std::string(projectOopHirNodeKindName(loweredExpression->kind)) + "'";
        lowered.reset();
    }
    if (!statementChildPolicyApplied && lowered != nullptr &&
        effects.allows(sema::Effect::NoHeap))
    {
        const ClassMemberSymbol& caller = *activeMember;
        const MethodCallBinding* call = callBinding(expression);
        if (call != nullptr && call->resolution == MethodCallResolutionKind::Resolved &&
            call->selectedMemberOrdinal.has_value() &&
            *call->selectedMemberOrdinal < memberModel_->members().size())
        {
            const ClassMemberSymbol& callee = memberModel_->members()[*call->selectedMemberOrdinal];
            if (!callee.isNoHeap)
            {
                error = "NS3030: noheap callable '" + caller.signatureKey +
                    "' cannot call non-noheap callable '" + callee.signatureKey + "'";
                lowered.reset();
            }
        }
        if (lowered != nullptr &&
            loweredExpression != nullptr &&
            (loweredExpression->kind == ProjectOopHirNodeKind::DelegateInvoke ||
             loweredExpression->kind == ProjectOopHirNodeKind::EventInvoke))
        {
            error = "NS3030: noheap callable '" + caller.signatureKey +
                "' cannot invoke an indirect target without a noheap contract";
            lowered.reset();
        }
        if (lowered != nullptr &&
            loweredExpression != nullptr &&
            (loweredExpression->kind == ProjectOopHirNodeKind::PlacementConstruction ||
             loweredExpression->kind == ProjectOopHirNodeKind::ValueConstruction))
        {
            const ObjectConstructionBinding* binding = constructionBinding(expression);
            if (binding == nullptr || !binding->selectedConstructorMemberOrdinal.has_value() ||
                *binding->selectedConstructorMemberOrdinal >= memberModel_->members().size())
            {
                error = "NS3030: noheap callable '" + caller.signatureKey +
                    "' cannot invoke an implicit constructor without a noheap contract";
                lowered.reset();
            }
            else
            {
                const ClassMemberSymbol& constructor = memberModel_->members()[
                    *binding->selectedConstructorMemberOrdinal];
                if (!constructor.isNoHeap)
                {
                    error = "NS3030: noheap callable '" + caller.signatureKey +
                        "' cannot call non-noheap constructor '" +
                        constructor.signatureKey + "'";
                    lowered.reset();
                }
            }
        }
    }
    // Solo se apunta si NO hay nada apuntado: asi gana el nodo mas interno, que es el que de
    // verdad fallo, y no la expresion que lo contiene.
    if (lowered == nullptr && !error.empty() && !lastErrorRange_.isValid() && expression != nullptr)
    {
        lastErrorRange_ = expression->range();
    }
    return lowered;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::buildConditionalExpression(
    const ast::AstNode* expression,
    std::unique_ptr<ProjectOopHirNode> condition,
    std::unique_ptr<ProjectOopHirNode> whenTrue,
    std::unique_ptr<ProjectOopHirNode> whenFalse)
{
    const MethodExpressionTypeBinding* typeBinding = expressionType(expression);
    const MethodDelegateBinding* stage42Delegate = delegateBinding(expression);
    const std::string resultType = typeBinding != nullptr ? typeBinding->typeKey :
        stage42Delegate != nullptr ? stage42Delegate->resultTypeKey : "<error>";
    auto node = makeNode(ProjectOopHirNodeKind::Conditional, expression, resultType);
    const auto convertBranch = [&](std::unique_ptr<ProjectOopHirNode>& branch,
                                   const ast::AstNode* syntax)
    {
        if (branch->typeKey == resultType) return;
        const MethodExpressionTypeBinding* binding = expressionType(syntax);
        const ProjectImplicitConversion conversion = ProjectTypeConversion::classifyImplicit(
            branch->typeKey,
            resultType,
            binding != nullptr ? binding->integerConstant : std::nullopt);
        if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
            branch = wrapImplicitConversion(std::move(branch), conversion, syntax);
    };
    convertBranch(whenTrue, expression->second());
    convertBranch(whenFalse, expression->third());
    node->children.push_back(std::move(condition));
    node->children.push_back(std::move(whenTrue));
    node->children.push_back(std::move(whenFalse));
    return node;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::buildBuiltinBinaryExpression(
    const ast::AstNode* expression,
    std::unique_ptr<ProjectOopHirNode> left,
    std::unique_ptr<ProjectOopHirNode> right)
{
    const MethodExpressionTypeBinding* typeBinding = expressionType(expression);
    const MethodDelegateBinding* stage42Delegate = delegateBinding(expression);
    const std::string resultType = typeBinding != nullptr ? typeBinding->typeKey :
        stage42Delegate != nullptr ? stage42Delegate->resultTypeKey : "<error>";
    auto node = makeNode(ProjectOopHirNodeKind::Binary, expression, resultType);
    node->operation = expression->tokenKind();
    node->booleanValue = typeBinding != nullptr && typeBinding->flowRuntimeCheckElided;

    const auto isEnumTypeKey = [&](std::string_view typeKey) noexcept
    {
        if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
        const std::string_view identity = typeKey.substr(5);
        return std::any_of(
            nameLookup_->namedTypes().begin(),
            nameLookup_->namedTypes().end(),
            [&](const NamedTypeDeclaration& type)
            {
                return type.kind == NamedTypeDeclarationKind::Enum &&
                    type.identity.key() == identity;
            });
    };
    const auto isZeroIntegerLiteral = [](const ast::AstNode* syntax) noexcept
    {
        return syntax != nullptr &&
            syntax->kind() == ast::AstNodeKind::IntegerLiteralExpression &&
            syntax->integerValue() == 0;
    };
    if (expression->tokenKind() == lexer::TokenKind::EqualEqual ||
        expression->tokenKind() == lexer::TokenKind::BangEqual)
    {
        if (isEnumTypeKey(left->typeKey) && isZeroIntegerLiteral(expression->second()))
        {
            auto converted = makeNode(
                ProjectOopHirNodeKind::Conversion, expression->second(), left->typeKey);
            converted->children.push_back(std::move(right));
            right = std::move(converted);
        }
        else if (isEnumTypeKey(right->typeKey) && isZeroIntegerLiteral(expression->first()))
        {
            auto converted = makeNode(
                ProjectOopHirNodeKind::Conversion, expression->first(), right->typeKey);
            converted->children.push_back(std::move(left));
            left = std::move(converted);
        }
    }
    std::optional<std::string> numericOperandType;
    if (ProjectTypeConversion::isNumeric(resultType))
    {
        numericOperandType = resultType;
    }
    else if (isComparison(expression->tokenKind()) &&
        ProjectTypeConversion::isNumeric(left->typeKey) &&
        ProjectTypeConversion::isNumeric(right->typeKey))
    {
        const MethodExpressionTypeBinding* leftType = expressionType(expression->first());
        const MethodExpressionTypeBinding* rightType = expressionType(expression->second());
        numericOperandType = ProjectTypeConversion::commonNumericType(
            left->typeKey,
            right->typeKey,
            leftType != nullptr ? leftType->integerConstant : std::nullopt,
            rightType != nullptr ? rightType->integerConstant : std::nullopt);
    }
    if (numericOperandType.has_value())
    {
        const MethodExpressionTypeBinding* leftType = expressionType(expression->first());
        const MethodExpressionTypeBinding* rightType = expressionType(expression->second());
        const ProjectImplicitConversion leftConversion = ProjectTypeConversion::classifyImplicit(
            left->typeKey,
            *numericOperandType,
            leftType != nullptr ? leftType->integerConstant : std::nullopt);
        const ProjectImplicitConversion rightConversion = ProjectTypeConversion::classifyImplicit(
            right->typeKey,
            *numericOperandType,
            rightType != nullptr ? rightType->integerConstant : std::nullopt);
        if (leftConversion.exists() && leftConversion.kind != ProjectConversionKind::Identity)
            left = wrapImplicitConversion(std::move(left), leftConversion, expression->first());
        if (rightConversion.exists() && rightConversion.kind != ProjectConversionKind::Identity)
            right = wrapImplicitConversion(std::move(right), rightConversion, expression->second());
    }
    node->children.push_back(std::move(left));
    node->children.push_back(std::move(right));
    return node;
}

bool ProjectOopHirModel::hasValidDelegateCreationTarget(
    const MethodDelegateBinding& binding) const noexcept
{
    if (!binding.selectedMemberOrdinal.has_value() || memberModel_ == nullptr)
        return false;
    if (*binding.selectedMemberOrdinal < memberModel_->members().size())
        return true;
    return namespaceFunctionModel_ != nullptr &&
        *binding.selectedMemberOrdinal - memberModel_->members().size() <
            namespaceFunctionModel_->functions().size();
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::finishDelegateCreation(
    const ast::AstNode* expression,
    const MethodDelegateBinding& binding,
    std::unique_ptr<ProjectOopHirNode> receiver)
{
    auto node = makeNode(
        ProjectOopHirNodeKind::DelegateCreate,
        expression,
        binding.delegateTypeKey);
    node->elementTypeKey = binding.delegateTypeKey;
    node->memberOrdinal = binding.selectedMemberOrdinal;
    node->booleanValue = binding.virtualDispatch;
    node->interfaceDispatch = binding.interfaceDispatch;
    node->ownership = binding.ownsReceiver
        ? sema::OwnershipKind::Owned : sema::OwnershipKind::Borrowed;
    if (binding.interfaceDispatch)
        node->targetTypeOrdinal = binding.receiverTypeOrdinal;
    if (!binding.staticTarget)
        node->children.push_back(std::move(receiver));
    return node;
}


std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExpressionImpl(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error)
{
    if (expression == nullptr)
    {
        error = "null expression reached Stage 34 OOP HIR";
        return nullptr;
    }
    const MethodLambdaBinding* stage43Lambda = lambdaBinding(expression);
    if (stage43Lambda != nullptr && expression->kind() == ast::AstNodeKind::LambdaExpression)
    {
        auto node = makeNode(ProjectOopHirNodeKind::LambdaCreate, expression, stage43Lambda->delegateTypeKey);
        node->elementTypeKey = stage43Lambda->delegateTypeKey;
        node->stringValue = stage43Lambda->generatedName;
        node->integerValue = stage43Lambda->lambdaOrdinal;
        for (const MethodLambdaCapture& capture : stage43Lambda->captures)
        {
            std::unique_ptr<ProjectOopHirNode> value;
            switch (capture.sourceKind)
            {
                case MethodLambdaCaptureSourceKind::OriginalLocal:
                case MethodLambdaCaptureSourceKind::ParentLocal:
                    if (capture.sourceOrdinal.has_value())
                    {
                        value = makeNode(ProjectOopHirNodeKind::CaptureCellReference, expression, capture.typeKey);
                        value->localOrdinal = capture.sourceOrdinal;
                    }
                    break;
                case MethodLambdaCaptureSourceKind::OriginalParameter:
                case MethodLambdaCaptureSourceKind::ParentParameter:
                    if (capture.sourceOrdinal.has_value())
                    {
                        value = makeNode(ProjectOopHirNodeKind::CaptureCellReference, expression, capture.typeKey);
                        value->parameterOrdinal = capture.sourceOrdinal;
                    }
                    break;
                case MethodLambdaCaptureSourceKind::OriginalThis:
                    value = makeNode(ProjectOopHirNodeKind::CaptureCellReference, expression, capture.typeKey);
                    break;
                case MethodLambdaCaptureSourceKind::ParentCapture:
                    if (capture.sourceOrdinal.has_value())
                    {
                        value = makeNode(
                            ProjectOopHirNodeKind::ClosureCaptureCellReference,
                            expression,
                            capture.typeKey);
                        value->integerValue = *capture.sourceOrdinal;
                    }
                    break;
            }
            if (value == nullptr)
            {
                error = "Stage 43 lambda capture source is unavailable";
                return nullptr;
            }
            value->captureMode = capture.mode;
            value->booleanValue = capture.clearMovedSource;
            node->children.push_back(std::move(value));
        }
        return node;
    }

    const MethodDelegateBinding* stage42Delegate = delegateBinding(expression);
    if (stage42Delegate != nullptr && stage42Delegate->kind == MethodDelegateOperationKind::Create)
    {
        if (!hasValidDelegateCreationTarget(*stage42Delegate))
        {
            error = "Stage 42 delegate creation has no selected callable";
            return nullptr;
        }
        std::unique_ptr<ProjectOopHirNode> receiver;
        if (!stage42Delegate->staticTarget)
        {
            if (stage42Delegate->ownsReceiver &&
                (stage42Delegate->receiverSourceLocalOrdinal.has_value() ||
                 stage42Delegate->receiverSourceParameterOrdinal.has_value()))
            {
                receiver = makeNode(
                    ProjectOopHirNodeKind::MoveValue,
                    stage42Delegate->receiverSyntax,
                    stage42Delegate->receiverTypeKey);
                receiver->localOrdinal = stage42Delegate->receiverSourceLocalOrdinal;
                receiver->parameterOrdinal = stage42Delegate->receiverSourceParameterOrdinal;
                receiver->booleanValue = true;
            }
            else
            {
                receiver = lowerImplicitReceiver(
                    stage42Delegate->receiverSyntax,
                    stage42Delegate->receiverTypeKey,
                    error);
            }
            if (receiver == nullptr) return nullptr;
        }
        return finishDelegateCreation(
            expression, *stage42Delegate, std::move(receiver));
    }

    const MethodExpressionTypeBinding* typeBinding = expressionType(expression);
    const std::string resultType = typeBinding != nullptr ? typeBinding->typeKey :
        stage42Delegate != nullptr ? stage42Delegate->resultTypeKey : "<error>";

    // ID 82. `preloweredLast`, cuando no es nulo, SUSTITUYE la bajada del ultimo argumento. Tiene
    // un solo llamador: la asignacion compuesta sobre propiedad e indexador, donde el valor que
    // recibe el setter no es una expresion de la fuente sino `get_X(...) op derecho`, un arbol que
    // hay que construir en el sitio. Todo lo demas --receptor, conversiones seleccionadas,
    // borrows-- sigue siendo el mismo codigo para todas las llamadas resueltas, que es justamente
    // el motivo de entrar por aqui en vez de armar el nodo de llamada a mano.
    const auto lowerResolvedCall = [&](
        const MethodCallBinding* binding,
        const std::vector<const ast::AstNode*>& argumentSyntaxes,
        std::unique_ptr<ProjectOopHirNode> preloweredLast = nullptr,
        bool nullConditional = false)
        -> std::unique_ptr<ProjectOopHirNode>
    {
        return lowerBoundCall(
            expression,
            memberOrdinal,
            addressContext,
            error,
            typeBinding,
            binding,
            argumentSyntaxes,
            std::move(preloweredLast),
            nullConditional);
    };

    const auto lowerEventBacking = [&](
        const MethodEventBinding* binding,
        bool address) -> std::unique_ptr<ProjectOopHirNode>
    {
        return lowerEventBackingField(expression, memberOrdinal, error, binding, address);
    };

    switch (expression->kind())
    {
        case ast::AstNodeKind::LoopExpression:
            return lowerExpressionWithStatementFrame(
                expression, memberOrdinal, addressContext, nullptr, error);
        case ast::AstNodeKind::ThisExpression:
        {
            if (currentLambda_ != nullptr)
            {
                const std::optional<std::size_t> capture = currentLambdaThisCapture();
                if (!capture.has_value())
                {
                    error = "Stage 43 lambda this-reference has no captured receiver";
                    return nullptr;
                }
                auto node = makeNode(
                    addressContext && isStructValueType(resultType)
                        ? ProjectOopHirNodeKind::ClosureCaptureAddress
                        : ProjectOopHirNodeKind::ClosureCaptureValue,
                    expression,
                    resultType);
                node->integerValue = *capture;
                return node;
            }
            // `this` inside a STRUCT method is the receiver's ADDRESS -- struct receivers are
            // passed by address and memory-homed since Stage 40.2 -- so in a VALUE context the
            // expression `this` has to load through it, exactly the way a `ref` parameter does a
            // few cases below. Handing back the bare ThisValue there was two reported bugs at
            // once, and they are the same line:
            //
            //   * `Get(this)` passed the POINTER where the callee's signature wanted the
            //     aggregate; the LLVM verifier rejected the module. Loud.
            //   * `S8 self = this;` stored the pointer BITS into the local's own bytes -- for an
            //     8-byte struct the carrier is `i64`, so `store ptr` fit and nothing complained --
            //     and every later field read decoded those bits as the struct's fields. Silent,
            //     no diagnostic, passes the verifier, and it is the one that reaches production.
            //     It was also, bitterly, the workaround people wrote for the loud one.
            //
            // Nothing changes for a CLASS: `this` IS the reference, and its value in both
            // contexts is that pointer. Nothing changes in an ADDRESS context either -- and every
            // existing consumer of ThisValue is one: `lowerReceiver` lowers a struct receiver
            // with addressContext = true (so `this.field` and `this.Method()` keep taking the
            // address), and the constructor-initializer and instance-initializer helpers build
            // ThisValue directly as a receiver.
            auto self = makeNode(ProjectOopHirNodeKind::ThisValue, expression, resultType);
            if (addressContext || !isStructValueType(resultType))
            {
                return self;
            }
            auto load = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, resultType);
            load->children.push_back(std::move(self));
            return load;
        }

        case ast::AstNodeKind::NullConditionalMemberExpression:
        {
            if (addressContext)
            {
                error = "a null-conditional access produces a value, not addressable storage";
                return nullptr;
            }
            const MethodExpressionTypeBinding* receiverFlow = expressionType(expression->first());
            const bool provenNonNull = receiverFlow != nullptr && receiverFlow->flowProvenNonNull;
            if (const MethodCallBinding* accessorCall = accessorGetterBinding(expression);
                accessorCall != nullptr)
            {
                auto call = lowerResolvedCall(accessorCall, {}, nullptr, !provenNonNull);
                if (call == nullptr) return nullptr;
                if (provenNonNull) call->stringValue = "flow-nonnull";
                return call;
            }
            const MethodNameBinding* binding = nameBinding(expression);
            if (binding == nullptr || binding->candidateMemberOrdinals.size() != 1 ||
                binding->kind != MethodNameBindingKind::ExplicitObjectField)
            {
                error = "T01-F03 null-conditional access has no unique instance-field binding";
                return nullptr;
            }
            auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (receiver == nullptr) return nullptr;
            auto node = makeNode(
                provenNonNull ? ProjectOopHirNodeKind::FieldValue
                              : ProjectOopHirNodeKind::NullConditionalFieldValue,
                expression, resultType);
            node->memberOrdinal = binding->candidateMemberOrdinals.front();
            node->booleanValue = provenNonNull;
            node->volatileAccess = memberModel_ != nullptr &&
                *node->memberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[*node->memberOrdinal].isVolatile;
            node->children.push_back(std::move(receiver));
            return node;
        }

        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
        {
            const MethodCoreBinding* core = coreBinding(expression);
            if (core != nullptr && core->kind == MethodCoreOperationKind::FixedArrayLength)
            {
                if (addressContext)
                {
                    error = "NativeSharp 3.0 fixed-array Length is a constant value and is not addressable";
                    return nullptr;
                }
                const std::optional<std::uint64_t> count =
                    ProjectMemberModel::fixedArrayCount(core->elementTypeKey);
                if (!count.has_value())
                {
                    error = "NativeSharp 3.0 fixed-array Length lost its canonical extent";
                    return nullptr;
                }
                auto node = makeNode(
                    ProjectOopHirNodeKind::IntegerConstant, expression, core->resultTypeKey);
                node->integerValue = *count;
                return node;
            }
            if (core != nullptr &&
                (core->kind == MethodCoreOperationKind::ArrayLength ||
                 core->kind == MethodCoreOperationKind::StringLength))
            {
                if (addressContext)
                {
                    error = "Stage 41 Length is an intrinsic value and is not addressable";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                auto node = makeNode(
                    core->kind == MethodCoreOperationKind::ArrayLength
                        ? ProjectOopHirNodeKind::ArrayLength
                        : ProjectOopHirNodeKind::StringLength,
                    expression,
                    core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->children.push_back(std::move(receiver));
                return node;
            }
            if (core != nullptr && core->kind == MethodCoreOperationKind::IteratorCurrent)
            {
                // NS304-T06-F05: a `ref:`/`refmut:` item Current IS a reference into the frame, so
                // taking it in address context is allowed (it yields the reference, not a value).
                const bool itemByReference = core->resultTypeKey.starts_with("ref:") ||
                    core->resultTypeKey.starts_with("refmut:");
                if (addressContext && !itemByReference)
                {
                    error = "iterator Current is an intrinsic value and is not addressable";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::IteratorCurrent, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->children.push_back(std::move(receiver));
                return node;
            }
            // NSC-LIMIT-008: la misma pregunta que hace isNaturallyAddressable, hecha una sola
            // vez. Antes estaba escrita aqui a mano y alli no se hacia, y esa discrepancia era el
            // defecto: un getter se clasificaba como direccionable y lowerReceiver no le hacia
            // temporal.
            if (const MethodCallBinding* accessorCall = accessorGetterBinding(expression);
                accessorCall != nullptr)
            {
                const std::size_t accessorOrdinal = *accessorCall->selectedMemberOrdinal;
                if (addressContext && !memberModel_->members()[accessorOrdinal].isReferenceReturn)
                {
                    error = "Stage 38 properties are semantic accessor calls and are not addressable";
                    return nullptr;
                }
                return lowerResolvedCall(accessorCall, {});
            }
            // NS304-T03-F03: a marked list-pattern length member that resolved to a
            // zero-argument `Length()` method was auto-invoked in Sema; lower it as the
            // ordinary instance call it became.
            if (const MethodCallBinding* lengthCall = callBinding(expression);
                lengthCall != nullptr && expression->auxiliaryText() == "list-pattern-length")
            {
                return lowerResolvedCall(lengthCall, {});
            }
            // NS304-T08-F02: extension property getter auto-invocado en Sema (namespace call con
            // receptor reenviado). Se baja igual que la llamada de namespace del path de Call.
            if (const NamespaceFunctionCallBinding* namespaceCall =
                    namespaceFunctionCallBinding(expression);
                namespaceCall != nullptr)
            {
                return lowerExtensionPropertyGetterWithReceiver(
                    expression, memberOrdinal, error, *namespaceCall);
            }
            if (currentLambda_ != nullptr)
            {
                const MethodLambdaValueBinding* lambdaValue = lambdaValueBinding(expression);
                if (lambdaValue != nullptr && lambdaValue->lambdaOrdinal == currentLambda_->lambdaOrdinal)
                {
                    if (lambdaValue->lambdaParameter)
                    {
                        if (lambdaValue->lambdaParameterByReference)
                        {
                            const std::string pointerType = lambdaValue->typeKey + "*";
                            auto pointer = makeNode(
                                ProjectOopHirNodeKind::ParameterValue, expression, pointerType);
                            pointer->parameterOrdinal = lambdaValue->valueOrdinal;
                            if (addressContext) return pointer;
                            auto value = makeNode(
                                ProjectOopHirNodeKind::PointerLoad, expression, lambdaValue->typeKey);
                            value->children.push_back(std::move(pointer));
                            return value;
                        }
                        auto node = makeNode(
                            addressContext ? ProjectOopHirNodeKind::ParameterAddress : ProjectOopHirNodeKind::ParameterValue,
                            expression, lambdaValue->typeKey);
                        node->parameterOrdinal = lambdaValue->valueOrdinal;
                        return node;
                    }
                    auto node = makeNode(
                        addressContext
                            ? ProjectOopHirNodeKind::ClosureCaptureAddress
                            : ProjectOopHirNodeKind::ClosureCaptureValue,
                        expression,
                        lambdaValue->typeKey);
                    node->integerValue = lambdaValue->valueOrdinal;
                    return node;
                }
            }
            const MethodNameBinding* binding = nameBinding(expression);
            if (binding == nullptr)
            {
                error = "Stage 34 cannot map identifier/member expression to Stage 27 binding";
                return nullptr;
            }
            if (binding->kind == MethodNameBindingKind::Parameter)
            {
                // C4 "language references": a by-reference parameter physically receives an
                // ADDRESS, and every use of it means the value on the other side. So the
                // parameter slot itself is the pointer:
                //
                //   value context   -> load the pointer, then load through it (PointerLoad)
                //   address context -> the pointer IS the address; hand it over untouched
                //
                // which is exactly how `*p` already behaves for an explicit pointer parameter.
                // Nothing new reaches MIR or the backend: `ref`/`out`/`in` are rules on top of
                // machinery C1 already certified, not a second calling convention.
                const MethodParameterSymbol* parameter = nullptr;
                if (binding->parameterOrdinal.has_value() && memberModel_ != nullptr)
                {
                    if (binding->methodMemberOrdinal < memberModel_->members().size())
                    {
                        const ClassMemberSymbol& owner =
                            memberModel_->members()[binding->methodMemberOrdinal];
                        if (*binding->parameterOrdinal < owner.parameters.size())
                            parameter = &owner.parameters[*binding->parameterOrdinal];
                    }
                    else if (namespaceFunctionModel_ != nullptr)
                    {
                        const std::size_t functionOrdinal =
                            binding->methodMemberOrdinal - memberModel_->members().size();
                        if (functionOrdinal < namespaceFunctionModel_->functions().size())
                        {
                            const NamespaceFunctionSymbol& owner =
                                namespaceFunctionModel_->functions()[functionOrdinal];
                            if (*binding->parameterOrdinal < owner.parameters.size())
                                parameter = &owner.parameters[*binding->parameterOrdinal];
                        }
                    }
                }
                if (parameter != nullptr && parameter->passKind != ParameterPassKind::Value)
                {
                    auto pointer = makeNode(
                        ProjectOopHirNodeKind::ParameterValue, expression, parameter->typeKey);
                    pointer->parameterOrdinal = binding->parameterOrdinal;
                    if (addressContext) return pointer;
                    auto value = makeNode(
                        ProjectOopHirNodeKind::PointerLoad, expression, parameter->pointeeTypeKey);
                    value->children.push_back(std::move(pointer));
                    return value;
                }
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::ParameterAddress : ProjectOopHirNodeKind::ParameterValue,
                    expression,
                    binding->typeKey);
                node->parameterOrdinal = binding->parameterOrdinal;
                return node;
            }
            if (binding->kind == MethodNameBindingKind::Local)
            {
                const MethodLocalSymbol* local = binding->localOrdinal.has_value() &&
                        *binding->localOrdinal < methodBodyModel_->locals().size()
                    ? &methodBodyModel_->locals()[*binding->localOrdinal]
                    : nullptr;
                if (local != nullptr && local->isReference)
                {
                    // The reference local's slot stores a pointer. Address context consumes that
                    // pointer directly; value context loads through it once more.
                    auto pointer = makeNode(
                        ProjectOopHirNodeKind::LocalValue,
                        expression,
                        binding->typeKey + "*");
                    pointer->localOrdinal = binding->localOrdinal;
                    if (addressContext) return pointer;
                    auto value = makeNode(
                        ProjectOopHirNodeKind::PointerLoad,
                        expression,
                        binding->typeKey);
                    value->children.push_back(std::move(pointer));
                    return value;
                }
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::LocalAddress : ProjectOopHirNodeKind::LocalValue,
                    expression,
                    binding->typeKey);
                node->localOrdinal = binding->localOrdinal;
                node->volatileAccess = local != nullptr && local->isVolatile;
                return node;
            }
            if (binding->kind == MethodNameBindingKind::ImplicitField ||
                binding->kind == MethodNameBindingKind::ExplicitThisField ||
                binding->kind == MethodNameBindingKind::ExplicitObjectField ||
                binding->kind == MethodNameBindingKind::ExplicitBaseField)
            {
                if (binding->candidateMemberOrdinals.empty())
                {
                    error = "Macro A field binding has no candidate member";
                    return nullptr;
                }
                const std::size_t fieldOrdinal = binding->candidateMemberOrdinals.front();
                auto receiver = lowerReceiver(
                    binding->receiverSyntax, binding->receiverTypeKey, memberOrdinal, error);
                if (receiver == nullptr)
                {
                    return nullptr;
                }
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::FieldAddress : ProjectOopHirNodeKind::FieldValue,
                    expression,
                    binding->typeKey);
                node->memberOrdinal = fieldOrdinal;
                node->volatileAccess = memberModel_ != nullptr &&
                    *node->memberOrdinal < memberModel_->members().size() &&
                    memberModel_->members()[*node->memberOrdinal].isVolatile;
                node->baseReceiver = binding->baseReceiver;
                node->children.push_back(std::move(receiver));
                return node;
            }
            if (binding->kind == MethodNameBindingKind::NamespaceState)
            {
                if (binding->candidateMemberOrdinals.size() != 1 ||
                    namespaceFunctionModel_ == nullptr ||
                    binding->candidateMemberOrdinals.front() >=
                        namespaceFunctionModel_->states().size())
                {
                    error = "namespace state binding has no unique state ordinal";
                    return nullptr;
                }
                const std::size_t stateOrdinal =
                    binding->candidateMemberOrdinals.front();
                const NamespaceStateSymbol& state =
                    namespaceFunctionModel_->states()[stateOrdinal];
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::NamespaceStateAddress
                                   : ProjectOopHirNodeKind::NamespaceStateValue,
                    expression,
                    binding->typeKey);
                node->namespaceStateOrdinal = stateOrdinal;
                node->memberOrdinal =
                    (memberModel_ != nullptr ? memberModel_->members().size() : 0) +
                    stateOrdinal;
                node->stringValue = state.storageName;
                return node;
            }
            if (binding->kind == MethodNameBindingKind::StaticField)
            {
                if (binding->candidateMemberOrdinals.empty())
                {
                    error = "Stage 37 static-field binding has no candidate member";
                    return nullptr;
                }
                auto node = makeNode(
                    addressContext ? ProjectOopHirNodeKind::StaticFieldAddress
                                   : ProjectOopHirNodeKind::StaticFieldValue,
                    expression,
                    binding->typeKey);
                node->memberOrdinal = binding->candidateMemberOrdinals.front();
                node->volatileAccess = memberModel_ != nullptr &&
                    *node->memberOrdinal < memberModel_->members().size() &&
                    memberModel_->members()[*node->memberOrdinal].isVolatile;
                return node;
            }
            if (binding->kind == MethodNameBindingKind::TypeReference)
            {
                error = "Stage 37 type reference may only appear as the receiver of a static member access";
                return nullptr;
            }
            error = "Stage 34 method-group expression can only appear as call target";
            return nullptr;
        }

        case ast::AstNodeKind::IntegerLiteralExpression:
        case ast::AstNodeKind::CharacterLiteralExpression:
        {
            auto node = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, resultType);
            node->integerValue = expression->integerValue();
            return node;
        }

        case ast::AstNodeKind::FloatingLiteralExpression:
        {
            auto node = makeNode(ProjectOopHirNodeKind::FloatingConstant, expression, resultType);
            node->floatingValue = expression->floatingValue();
            return node;
        }

        case ast::AstNodeKind::StringLiteralExpression:
        case ast::AstNodeKind::NameOfExpression:
        {
            if (addressContext)
            {
                error = "Stage 41 string literals are immutable references and are not addressable values";
                return nullptr;
            }
            if (expression->tokenKind() == lexer::TokenKind::Utf8StringLiteral)
            {
                // NS304-T10-F01: u8"..." is a ReadOnlySpan<byte> over the literal's UTF-8 bytes.
                const MethodCoreBinding* binding = coreBinding(expression);
                const std::string spanKey = binding != nullptr
                    ? binding->resultTypeKey : std::string{};
                auto node = makeNode(
                    ProjectOopHirNodeKind::Utf8StringLiteral, expression,
                    spanKey.empty() ? "builtin:byte" : spanKey);
                node->stringValue = expression->text();
                node->elementTypeKey = "builtin:byte";
                node->integerValue = node->stringValue.size();
                return node;
            }
            auto node = makeNode(ProjectOopHirNodeKind::StringLiteral, expression, "string-literal");
            node->stringValue = expression->text();
            return node;
        }

        case ast::AstNodeKind::OverflowPolicyExpression:
        {
            auto operand = lowerExpression(expression->first(), memberOrdinal, addressContext, error);
            if (operand == nullptr) return nullptr;
            auto node = makeNode(
                ProjectOopHirNodeKind::OverflowPolicy, expression, operand->typeKey);
            node->stringValue = expression->auxiliaryText();
            node->children.push_back(std::move(operand));
            return node;
        }

        case ast::AstNodeKind::InterpolatedStringExpression:
        {
            if (const MethodCoreBinding* core = coreBinding(expression);
                core != nullptr && core->kind == MethodCoreOperationKind::InterpolationHandler)
            {
                if (addressContext)
                {
                    error = "an interpolation handler result is a value, not addressable storage";
                    return nullptr;
                }
                const std::size_t tempOrdinal = nextSyntheticLocalOrdinal_++;
                std::vector<language::InterpolationPart> parts;
                std::string parseError;
                if (!language::parseStaticInterpolation(expression->text(), parts, parseError))
                {
                    error = std::move(parseError);
                    return nullptr;
                }
                const bool hasHole = std::any_of(
                    parts.begin(), parts.end(), [](const language::InterpolationPart& part)
                {
                    return part.kind == language::InterpolationPartKind::Hole;
                });
                if (hasHole)
                {
                    error = "interpolation handler hole has no expression";
                    return nullptr;
                }
                return buildInterpolationHandlerExpression(
                    expression, *core, tempOrdinal, parts, {}, error);
            }
            if (addressContext)
            {
                error = "a static interpolated string is immutable and not addressable";
                return nullptr;
            }
            const std::string* value = methodBodyModel_ != nullptr
                ? methodBodyModel_->staticInterpolationValue(expression) : nullptr;
            if (value == nullptr)
            {
                error = "T01-F06 static interpolation has no compile-time handler result";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::StringLiteral, expression, "string-literal");
            node->stringValue = *value;
            return node;
        }

        case ast::AstNodeKind::BooleanLiteralExpression:
        {
            auto node = makeNode(ProjectOopHirNodeKind::BooleanConstant, expression, "builtin:bool");
            node->booleanValue = expression->tokenKind() == lexer::TokenKind::KwTrue;
            return node;
        }

        case ast::AstNodeKind::NullLiteralExpression:
            return makeNode(ProjectOopHirNodeKind::NullConstant, expression, "builtin:null");

        case ast::AstNodeKind::DefaultValueExpression:
        {
            // Stage 52: default(T) -- the resolved concrete type (already monomorphized, if T
            // was a generic parameter -- see ProjectMethodBodyModel's DefaultValueExpression
            // case) was recorded during semantic analysis; LLVM lowering builds the all-zero-bits
            // value of that EXACT type (see the DefaultValue case in ProjectOopLlvmLowerer.cpp),
            // which is uniformly correct for every T: 0/false/'\0' for a numeric primitive, a
            // null pointer for a reference type or string, an all-zero-fields instance for a
            // value-typed struct.
            if (addressContext)
            {
                error = "Stage 52 default(T) value is not addressable";
                return nullptr;
            }
            const MethodExpressionTypeBinding* type = expressionType(expression);
            if (type == nullptr)
            {
                error = "Stage 52 default(T) expression has no resolved semantic type";
                return nullptr;
            }
            return makeNode(ProjectOopHirNodeKind::DefaultValue, expression, type->typeKey);
        }

        case ast::AstNodeKind::UnaryExpression:
        case ast::AstNodeKind::PostfixExpression:
        {
            if (expression->tokenKind() == lexer::TokenKind::PlusPlus ||
                expression->tokenKind() == lexer::TokenKind::MinusMinus)
            {
                // ID 79. `++x` es `x += 1` y `x++` es lo mismo devolviendo el valor anterior, asi
                // que los dos salen por el MISMO nodo que la asignacion compuesta y no por una
                // ruta paralela. La tarea 62 pedia extender esto "para punteros" cuando no existia
                // para ningun tipo; aqui entra para los dos a la vez, que es la unica forma de que
                // `++p` y `++i` no se separen.
                if (addressContext)
                {
                    error = "the result of ++/-- is a value, not addressable storage";
                    return nullptr;
                }
                const MethodExpressionTypeBinding* targetType = expressionType(expression->first());
                if (targetType == nullptr)
                {
                    error = "Stage 34 ++/-- operand has no resolved semantic type";
                    return nullptr;
                }
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;

                auto step = makeNode(ProjectOopHirNodeKind::CompoundStore, expression, targetType->typeKey);
                step->operation = expression->tokenKind() == lexer::TokenKind::PlusPlus
                    ? lexer::TokenKind::Plus
                    : lexer::TokenKind::Minus;
                // La UNICA diferencia entre postfijo y prefijo.
                step->booleanValue = expression->kind() == ast::AstNodeKind::PostfixExpression;

                std::unique_ptr<ProjectOopHirNode> one;
                if (ProjectTypeConversion::isPointer(targetType->typeKey))
                {
                    // `++p` avanza UN ELEMENTO, no un byte. El tipo apuntado es lo que da el paso,
                    // y lo resuelve el MIR con el mismo servicio de tamano que todo lo demas.
                    step->elementTypeKey = targetType->typeKey.substr(0, targetType->typeKey.size() - 1);
                    one = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, "builtin:nint");
                    one->integerValue = 1;
                }
                else if (ProjectTypeConversion::isNumeric(targetType->typeKey) &&
                         !ProjectTypeConversion::isIntegral(targetType->typeKey))
                {
                    one = makeNode(ProjectOopHirNodeKind::FloatingConstant, expression, targetType->typeKey);
                    one->floatingValue = 1.0;
                }
                else
                {
                    // El uno se construye YA en el tipo del destino, no en `int` con una conversion
                    // detras: asi la operacion entera ocurre en un solo tipo y no hay una segunda
                    // regla de promocion que mantener de acuerdo con la de `Binary`.
                    one = makeNode(ProjectOopHirNodeKind::IntegerConstant, expression, targetType->typeKey);
                    one->integerValue = 1;
                }
                step->children.push_back(std::move(address));
                step->children.push_back(std::move(one));
                return step;
            }
            // Interop Foundation: `&lvalue` and `*pointer` are now first-class in the OOP
            // pipeline. HISTORY: both used to be walled off ("keeps raw pointer
            // address/dereference in the procedural backend boundary"), which made every
            // out-parameter Win32 API unusable -- `GetMessage(&msg)` could not produce a MSG*
            // from a local at all. Since Stage 40.2 every local is memory-homed (an alloca,
            // with indirect structs carried as [N x i8]), so an address-of is LITERALLY the
            // operand lowered in address context -- no new machinery, just returning what the
            // assignment path always computed for its own targets.
            //
            // These are the language's RAW tier -- same third reference category the Bloque 1
            // docs pin for plain aliases: no lifetime tracking, dangles if it outlives its
            // target, by design. C APIs are expressed through exactly this shape.
            if (expression->tokenKind() == lexer::TokenKind::Amp)
            {
                if (addressContext)
                {
                    error = "Stage 34 address-of result is not itself addressable";
                    return nullptr;
                }
                // Bit fields are logical slices of an allocation unit, not independently
                // addressable objects.  Assignment lowering may request their allocation-unit
                // address internally, but source `&field` must never expose that address as a
                // pointer to the logical field type.
                if (const MethodNameBinding* target = nameBinding(expression->first());
                    target != nullptr && !target->candidateMemberOrdinals.empty() &&
                    memberModel_ != nullptr &&
                    target->candidateMemberOrdinals.front() < memberModel_->members().size() &&
                    memberModel_->members()[target->candidateMemberOrdinals.front()].isBitField)
                {
                    error = "bit fields have no address";
                    return nullptr;
                }
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;
                // WRAPPED, never mutated: overwriting the operand node's typeKey was measured
                // to miscompile -- a ParameterValue whose typeKey was rewritten to the pointee
                // struct loaded the [N x i8] aggregate where the pointer belonged ("GEP base
                // pointer is not a vector..."). The wrapper carries the expression's type (T*)
                // for consumers that derive ABI decisions from operand typeKeys (call-site
                // indirect-argument wrapping reads exactly that), while the operand keeps its
                // own meaning.
                auto node = makeNode(ProjectOopHirNodeKind::AddressValue, expression, resultType);
                node->children.push_back(std::move(address));
                return node;
            }
            if (expression->tokenKind() == lexer::TokenKind::Star)
            {
                if (addressContext)
                {
                    // The ADDRESS of `*p` is p's own value: `(*m).field = x` GEPs off it, and
                    // `*p = v` stores straight through it.
                    auto pointer = lowerExpression(expression->first(), memberOrdinal, false, error);
                    if (pointer == nullptr) return nullptr;
                    // Returned AS-IS: the pointer's value IS the address, and its own typeKey
                    // (T*) is already the honest description of an address-of-T.
                    return pointer;
                }
                auto pointer = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (pointer == nullptr) return nullptr;
                auto node = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, resultType);
                node->children.push_back(std::move(pointer));
                return node;
            }
            auto node = makeNode(ProjectOopHirNodeKind::Unary, expression, resultType);
            node->operation = expression->tokenKind();
            auto operand = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (operand == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(operand));
            return node;
        }

        case ast::AstNodeKind::NullCoalescingExpression:
        {
            if (addressContext)
            {
                error = "a null-coalescing expression produces a value, not addressable storage";
                return nullptr;
            }
            auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
            auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
            if (left == nullptr || right == nullptr) return nullptr;
            const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
            const ProjectImplicitConversion conversion = classifyImplicit(
                right->typeKey,
                resultType,
                rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
            if (!conversion.exists())
            {
                error = "T01-F03 null-coalescing RHS cannot convert from '" + right->typeKey +
                    "' to '" + resultType + "'";
                return nullptr;
            }
            if (conversion.kind != ProjectConversionKind::Identity)
                right = wrapImplicitConversion(std::move(right), conversion, expression->second());
            const MethodOptionalCarrierBinding* optional = optionalCarrierBinding(expression);
            auto node = makeNode(
                optional != nullptr ? ProjectOopHirNodeKind::OptionalCoalescing
                                    : ProjectOopHirNodeKind::NullCoalescing,
                expression, resultType);
            if (optional != nullptr)
            {
                node->elementTypeKey = optional->carrierTypeKey;
                node->memberOrdinal = optional->hasValueMemberOrdinal;
                node->secondaryMemberOrdinal = optional->uncheckedValueMemberOrdinal;
            }
            node->children.push_back(std::move(left));
            node->children.push_back(std::move(right));
            return node;
        }

        case ast::AstNodeKind::MatchValueExpression:
        {
            if (addressContext)
            {
                auto node = makeNode(ProjectOopHirNodeKind::MatchValue, expression, resultType);
                node->referenceResult = true;
                return node;
            }
            return makeNode(ProjectOopHirNodeKind::MatchValue, expression, resultType);
        }

        case ast::AstNodeKind::MatchExpression:
            return lowerExpressionWithStatementFrame(
                expression, memberOrdinal, addressContext, nullptr, error);

        case ast::AstNodeKind::MatchArmExpression:
            error = "a match arm reached HIR outside its match expression";
            return nullptr;

        case ast::AstNodeKind::ConditionalExpression:
        {
            if (addressContext)
            {
                error = "a conditional expression produces a value, not addressable storage";
                return nullptr;
            }
            auto condition = lowerExpression(expression->first(), memberOrdinal, false, error);
            if (condition == nullptr) return nullptr;
            auto whenTrue = lowerExpression(expression->second(), memberOrdinal, false, error);
            if (whenTrue == nullptr) return nullptr;
            auto whenFalse = lowerExpression(expression->third(), memberOrdinal, false, error);
            if (whenFalse == nullptr) return nullptr;
            return buildConditionalExpression(
                expression,
                std::move(condition),
                std::move(whenTrue),
                std::move(whenFalse));
        }

        case ast::AstNodeKind::BinaryExpression:
        {
            const MethodCoreBinding* stringCore = coreBinding(expression);
            // NSC-LIMIT-004. `a == b` y `a != b` sobre dos structs. El modelo semantico registro
            // un `StructEquals` sobre el propio BinaryExpression, asi que aqui se construye el
            // MISMO nodo que produce `a.Equals(b)` -- no hay una segunda semantica de igualdad de
            // agregados, solo dos formas de escribirla. La clase del nodo de sintaxis es lo que
            // las distingue: alli es una CallExpression, aqui un BinaryExpression.
            //
            // Para `!=` se envuelve la igualdad en el `Unary`/`Bang` que ya existe, en vez de
            // dar al nodo de agregados un modo negado propio: negar un bool es una instruccion, y
            // un segundo modo seria un segundo sitio donde la regla puede discrepar.
            if (stringCore != nullptr && stringCore->kind == MethodCoreOperationKind::StructEquals)
            {
                if (addressContext)
                {
                    error = "C8 struct comparison result is not addressable";
                    return nullptr;
                }
                auto node = makeNode(
                    ProjectOopHirNodeKind::AggregateEquals, expression, stringCore->resultTypeKey);
                node->elementTypeKey = stringCore->elementTypeKey;
                auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (left == nullptr) return nullptr;
                auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (right == nullptr) return nullptr;
                node->children.push_back(std::move(left));
                node->children.push_back(std::move(right));
                node->argumentCount = node->children.size();
                if (!appendAggregateLeaves(
                        stringCore->elementTypeKey, 0, expression, node->children, error))
                {
                    return nullptr;
                }
                if (expression->tokenKind() == lexer::TokenKind::BangEqual)
                {
                    auto negated = makeNode(
                        ProjectOopHirNodeKind::Unary, expression, stringCore->resultTypeKey);
                    negated->operation = lexer::TokenKind::Bang;
                    negated->children.push_back(std::move(node));
                    return negated;
                }
                return node;
            }
            if (stringCore != nullptr &&
                (stringCore->kind == MethodCoreOperationKind::StringConcat ||
                 stringCore->kind == MethodCoreOperationKind::StringEquals))
            {
                if (addressContext)
                {
                    error = "Stage 51 string operation result is not addressable";
                    return nullptr;
                }
                auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
                auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (left == nullptr || right == nullptr) return nullptr;
                auto node = makeNode(
                    stringCore->kind == MethodCoreOperationKind::StringConcat
                        ? ProjectOopHirNodeKind::StringConcat
                        : ProjectOopHirNodeKind::StringEquals,
                    expression,
                    stringCore->resultTypeKey);
                node->operation = expression->tokenKind();
                node->children.push_back(std::move(left));
                node->children.push_back(std::move(right));
                return node;
            }

            // CC1 fase 4D. El binding ya decidio que esto es aritmetica de punteros y con que
            // tipo de elemento; aqui solo se construye el nodo con los dos operandos EN EL ORDEN
            // EN QUE SE ESCRIBIERON. Ese orden importa: `i + p` y `p + i` dan lo mismo, pero
            // `p - i` y `i - p` no, y el backend necesita saber cual de los dos hijos es el
            // puntero. Lo deduce de los tipos de los hijos, no de una bandera aparte.
            if (stringCore != nullptr && stringCore->kind == MethodCoreOperationKind::PointerArithmetic)
            {
                if (addressContext)
                {
                    error = "CC1 pointer arithmetic yields a value, not addressable storage";
                    return nullptr;
                }
                auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (left == nullptr) return nullptr;
                auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (right == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::PointerArithmetic, expression, stringCore->resultTypeKey);
                node->operation = expression->tokenKind();
                node->elementTypeKey = stringCore->elementTypeKey;
                node->children.push_back(std::move(left));
                node->children.push_back(std::move(right));
                return node;
            }

            auto left = lowerExpression(expression->first(), memberOrdinal, false, error);
            auto right = lowerExpression(expression->second(), memberOrdinal, false, error);
            if (left == nullptr || right == nullptr) return nullptr;
            return buildBuiltinBinaryExpression(
                expression, std::move(left), std::move(right));
        }

        case ast::AstNodeKind::IndexExpression:
        case ast::AstNodeKind::NullConditionalIndexExpression:
        {
            if (const MethodRangeSliceBinding* range = rangeSliceBinding(expression);
                range != nullptr)
            {
                if (addressContext)
                {
                    error = "T01-F04 a range slice is a value and cannot be used as assignable storage";
                    return nullptr;
                }
                if (memberModel_ == nullptr ||
                    range->lengthMemberOrdinal >= memberModel_->members().size() ||
                    range->sliceMemberOrdinal >= memberModel_->members().size())
                {
                    error = "T01-F04 range protocol lost its selected Length/Slice members";
                    return nullptr;
                }
                auto receiver = lowerReceiver(
                    range->receiverSyntax, range->receiverTypeKey, memberOrdinal, error);
                if (receiver == nullptr) return nullptr;

                const auto lowerEndpoint = [&](
                    const ast::AstNode* syntax,
                    const ProjectImplicitConversion& conversion)
                    -> std::unique_ptr<ProjectOopHirNode>
                {
                    if (syntax == nullptr)
                    {
                        auto zero = makeNode(
                            ProjectOopHirNodeKind::IntegerConstant,
                            expression,
                            "builtin:int");
                        zero->integerValue = 0;
                        return zero;
                    }
                    auto endpoint = lowerExpression(syntax, memberOrdinal, false, error);
                    if (endpoint == nullptr) return nullptr;
                    if (conversion.exists() &&
                        conversion.kind != ProjectConversionKind::Identity)
                    {
                        endpoint = wrapImplicitConversion(
                            std::move(endpoint), conversion, syntax);
                    }
                    return endpoint;
                };

                auto start = lowerEndpoint(range->startSyntax, range->startConversion);
                auto end = lowerEndpoint(range->endSyntax, range->endConversion);
                if (start == nullptr || end == nullptr) return nullptr;

                const ClassMemberSymbol& sliceMember =
                    memberModel_->members()[range->sliceMemberOrdinal];
                auto node = makeNode(
                    ProjectOopHirNodeKind::RangeSlice,
                    expression,
                    range->resultTypeKey);
                node->elementTypeKey = range->receiverTypeKey;
                node->memberOrdinal = range->sliceMemberOrdinal;
                node->secondaryMemberOrdinal = range->lengthMemberOrdinal;
                node->referenceResult = sliceMember.isReferenceReturn;
                node->integerValue =
                    (range->hasStart ? 1U : 0U) |
                    (range->startFromEnd ? 2U : 0U) |
                    (range->hasEnd ? 4U : 0U) |
                    (range->endFromEnd ? 8U : 0U) |
                    (range->boundsStaticallyProven ? 16U : 0U);
                node->argumentCount = 2;
                node->argumentParameterOrdinals = {0U, 1U};
                node->children.push_back(std::move(receiver));
                node->children.push_back(std::move(start));
                node->children.push_back(std::move(end));
                return node;
            }
            const MethodCoreBinding* core = coreBinding(expression);
            if (core != nullptr &&
                (core->kind == MethodCoreOperationKind::ArrayElement ||
                 core->kind == MethodCoreOperationKind::RangeArrayElement ||
                 core->kind == MethodCoreOperationKind::StringElement))
            {
                if (core->kind == MethodCoreOperationKind::StringElement && addressContext)
                {
                    error = "Stage 41 strings are immutable; string elements are not assignable";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                std::vector<const ast::AstNode*> indexSyntaxes;
                if (!expression->children().empty())
                    indexSyntaxes.assign(expression->children().begin(), expression->children().end());
                else indexSyntaxes.push_back(expression->second());
                ProjectOopHirNodeKind kind = ProjectOopHirNodeKind::StringElementValue;
                if (core->kind == MethodCoreOperationKind::ArrayElement)
                {
                    kind = addressContext
                        ? ProjectOopHirNodeKind::ArrayElementAddress
                        : ProjectOopHirNodeKind::ArrayElementValue;
                }
                else if (core->kind == MethodCoreOperationKind::RangeArrayElement)
                {
                    kind = addressContext
                        ? ProjectOopHirNodeKind::RangeArrayElementAddress
                        : ProjectOopHirNodeKind::RangeArrayElementValue;
                }
                auto node = makeNode(kind, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->nativeArrayRank = core->kind == MethodCoreOperationKind::RangeArrayElement
                    ? indexSyntaxes.size() : 1U;
                node->children.push_back(std::move(receiver));
                for (std::size_t dimension = 0; dimension < indexSyntaxes.size(); ++dimension)
                {
                    const ast::AstNode* syntax = indexSyntaxes[dimension];
                    const bool fromEnd = syntax != nullptr &&
                        syntax->kind() == ast::AstNodeKind::FromEndExpression;
                    if (dimension == 0U) node->booleanValue = fromEnd;
                    if (fromEnd && dimension < 64U)
                        node->integerValue |= (std::uint64_t{1} << dimension);
                    auto index = lowerExpression(
                        fromEnd ? syntax->first() : syntax, memberOrdinal, false, error);
                    if (index == nullptr) return nullptr;
                    node->children.push_back(std::move(index));
                }
                return node;
            }
            // NS304-T10-F05: `receiver?[index]` — load condicional de elemento de array nativo.
            if (core != nullptr && core->kind == MethodCoreOperationKind::NullConditionalArrayElement)
            {
                if (addressContext)
                {
                    error = "T10-F05 null-conditional array element is not addressable";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                const ast::AstNode* indexSyntax = !expression->children().empty()
                    ? expression->children().front() : expression->second();
                auto index = lowerExpression(indexSyntax, memberOrdinal, false, error);
                if (index == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::NullConditionalArrayLoad, expression, core->resultTypeKey);
                node->elementTypeKey = core->elementTypeKey;
                node->children.push_back(std::move(receiver));
                node->children.push_back(std::move(index));
                return node;
            }
            // NS304-T10-F05: `receiver?[index]` sobre un indexador de usuario — null-check +
            // llamada al get_Item (receptor evaluado una vez por lowerResolvedCall).
            if (core != nullptr && core->kind == MethodCoreOperationKind::NullConditionalUserIndex)
            {
                if (addressContext)
                {
                    error = "T10-F05 null-conditional user index is not addressable";
                    return nullptr;
                }
                const MethodCallBinding* indexerCall = callBinding(expression);
                if (indexerCall == nullptr ||
                    indexerCall->resolution != MethodCallResolutionKind::Resolved ||
                    !indexerCall->selectedMemberOrdinal.has_value())
                {
                    error = "T10-F05 null-conditional user index has no resolved get_Item accessor";
                    return nullptr;
                }
                std::vector<const ast::AstNode*> arguments;
                if (!expression->children().empty())
                    arguments.assign(expression->children().begin(), expression->children().end());
                else if (expression->second() != nullptr)
                    arguments.push_back(expression->second());
                return lowerResolvedCall(indexerCall, arguments, nullptr, true);
            }
            // C2.1 "fixed arrays": inline, unmanaged element access -- `fixedArrayField[i]` and
            // `rawPointer[i]`. The only difference between the two is how the BASE ADDRESS is
            // obtained, and it is a real difference: a fixed array IS its storage, so its address
            // comes from lowering the base in address context; a pointer HOLDS an address, so its
            // address comes from lowering the base as an ordinary value. Everything after that --
            // the scaled GEP, the load, the store -- is identical, which is why one node covers
            // both.
            if (core != nullptr && core->kind == MethodCoreOperationKind::RawElement)
            {
                const ast::AstNode* baseSyntax = expression->first();
                const MethodExpressionTypeBinding* baseType = expressionType(baseSyntax);
                const bool baseIsFixedArray = baseType != nullptr &&
                    ProjectMemberModel::fixedArrayElementTypeKey(baseType->typeKey).has_value();
                const bool baseIsFlexibleArray = baseType != nullptr &&
                    ProjectMemberModel::flexibleArrayElementTypeKey(baseType->typeKey).has_value();
                auto base = lowerExpression(
                    baseSyntax, memberOrdinal, baseIsFixedArray || baseIsFlexibleArray, error);
                if (base == nullptr) return nullptr;
                std::vector<const ast::AstNode*> indices;
                if (!expression->children().empty())
                    indices.assign(expression->children().begin(), expression->children().end());
                else if (expression->second() != nullptr)
                    indices.push_back(expression->second());

                const MethodVariableArrayIndexBinding* variableArrayIndex =
                    methodBodyModel_ != nullptr
                        ? methodBodyModel_->variableArrayIndexBinding(expression)
                        : nullptr;
                if (variableArrayIndex != nullptr)
                {
                    if (indices.size() != variableArrayIndex->extentLocalOrdinals.size())
                    {
                        error = "multidimensional VLA index lost its semantic rank";
                        return nullptr;
                    }
                    std::unique_ptr<ProjectOopHirNode> linearIndex;
                    for (std::size_t dimension = 0; dimension < indices.size(); ++dimension)
                    {
                        auto index = lowerExpression(indices[dimension], memberOrdinal, false, error);
                        if (index == nullptr) return nullptr;
                        if (index->typeKey != "builtin:nuint")
                        {
                            auto converted = makeNode(
                                ProjectOopHirNodeKind::Conversion, indices[dimension], "builtin:nuint");
                            converted->children.push_back(std::move(index));
                            index = std::move(converted);
                        }
                        if (linearIndex == nullptr)
                        {
                            linearIndex = std::move(index);
                            continue;
                        }

                        auto extent = makeNode(
                            ProjectOopHirNodeKind::LocalValue,
                            expression,
                            methodBodyModel_->locals()[
                                variableArrayIndex->extentLocalOrdinals[dimension]].typeKey);
                        extent->localOrdinal =
                            variableArrayIndex->extentLocalOrdinals[dimension];
                        if (extent->typeKey != "builtin:nuint")
                        {
                            auto convertedExtent = makeNode(
                                ProjectOopHirNodeKind::Conversion, expression, "builtin:nuint");
                            convertedExtent->children.push_back(std::move(extent));
                            extent = std::move(convertedExtent);
                        }
                        auto rowOffset = makeNode(
                            ProjectOopHirNodeKind::Binary, expression, "builtin:nuint");
                        rowOffset->operation = lexer::TokenKind::Star;
                        rowOffset->children.push_back(std::move(linearIndex));
                        rowOffset->children.push_back(std::move(extent));
                        auto sum = makeNode(
                            ProjectOopHirNodeKind::Binary, expression, "builtin:nuint");
                        sum->operation = lexer::TokenKind::Plus;
                        sum->children.push_back(std::move(rowOffset));
                        sum->children.push_back(std::move(index));
                        linearIndex = std::move(sum);
                    }

                    auto address = makeNode(
                        ProjectOopHirNodeKind::RawElementAddress,
                        expression,
                        core->elementTypeKey);
                    address->elementTypeKey = core->elementTypeKey;
                    address->children.push_back(std::move(base));
                    address->children.push_back(std::move(linearIndex));
                    if (addressContext) return address;
                    auto value = makeNode(
                        ProjectOopHirNodeKind::PointerLoad,
                        expression,
                        core->resultTypeKey);
                    value->children.push_back(std::move(address));
                    return value;
                }

                std::string indexedType = baseType != nullptr ? baseType->typeKey : std::string();
                std::unique_ptr<ProjectOopHirNode> address;
                for (const ast::AstNode* indexSyntax : indices)
                {
                    const std::optional<std::string> fixedElement = baseIsFixedArray
                        ? ProjectMemberModel::fixedArrayElementTypeKey(indexedType)
                        : std::optional<std::string>();
                    const std::optional<std::string> flexibleElement = baseIsFlexibleArray
                        ? ProjectMemberModel::flexibleArrayElementTypeKey(indexedType)
                        : std::optional<std::string>();
                    const std::string elementType = fixedElement.has_value()
                        ? *fixedElement
                        : flexibleElement.has_value() ? *flexibleElement : core->elementTypeKey;
                    auto index = lowerExpression(indexSyntax, memberOrdinal, false, error);
                    if (index == nullptr) return nullptr;
                    address = makeNode(
                        ProjectOopHirNodeKind::RawElementAddress, expression, elementType);
                    address->elementTypeKey = elementType;
                    // NS304-T13-F06: a flexible-array index bounds-checks against its `Length`
                    // member named by [TrailingStorage(nameof(Length), ...)]. Resolve its byte
                    // offset here so the raw element address can emit the runtime guard.
                    if (baseIsFlexibleArray && metaprogrammingModel_ != nullptr &&
                        baseSyntax != nullptr && baseSyntax->first() != nullptr &&
                        inheritanceModel_ != nullptr && memberModel_ != nullptr &&
                        objectLayoutModel_ != nullptr)
                    {
                        const MethodExpressionTypeBinding* receiverType =
                            expressionType(baseSyntax->first());
                        if (receiverType != nullptr)
                        {
                            const std::optional<std::size_t> structOrdinal =
                                inheritanceModel_->typeOrdinalForTypeKey(receiverType->typeKey);
                            if (structOrdinal.has_value())
                            {
                                for (const StaticAttributeInfo& attribute :
                                     metaprogrammingModel_->attributes())
                                {
                                    if (attribute.identity != "TrailingStorage" ||
                                        attribute.constantArguments.empty() ||
                                        !attribute.typeOrdinal.has_value() ||
                                        attribute.typeOrdinal != structOrdinal)
                                    {
                                        continue;
                                    }
                                    const std::span<const std::size_t> lengthMembers =
                                        memberModel_->findMembers(
                                            *structOrdinal, attribute.constantArguments[0]);
                                    if (!lengthMembers.empty() &&
                                        attribute.memberOrdinal.has_value())
                                    {
                                        const ObjectFieldLayout* lengthLayout =
                                            objectLayoutModel_->fieldLayoutForMember(
                                                lengthMembers.front());
                                        const ObjectFieldLayout* dataLayout =
                                            objectLayoutModel_->fieldLayoutForMember(
                                                *attribute.memberOrdinal);
                                        if (lengthLayout != nullptr && dataLayout != nullptr)
                                        {
                                            address->hasTrailingLengthBounds = true;
                                            address->trailingLengthRelativeOffset =
                                                static_cast<std::int64_t>(lengthLayout->offset) -
                                                static_cast<std::int64_t>(dataLayout->offset);
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    address->children.push_back(std::move(base));
                    address->children.push_back(std::move(index));
                    base = std::move(address);
                    indexedType = elementType;
                }
                if (base == nullptr)
                {
                    error = "fixed-array/raw-pointer index expression has no index";
                    return nullptr;
                }
                if (addressContext) return base;
                auto value = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, core->resultTypeKey);
                value->children.push_back(std::move(base));
                return value;
            }

            const MethodCallBinding* indexerCall = callBinding(expression);
            if (indexerCall != nullptr && indexerCall->resolution == MethodCallResolutionKind::Resolved &&
                indexerCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *indexerCall->selectedMemberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[*indexerCall->selectedMemberOrdinal].isIndexerAccessor &&
                memberModel_->members()[*indexerCall->selectedMemberOrdinal].isGetter)
            {
                const ClassMemberSymbol& getter =
                    memberModel_->members()[*indexerCall->selectedMemberOrdinal];
                if (addressContext && !getter.isReferenceReturn)
                {
                    error = "Stage 38 indexers are semantic accessor calls and are not directly addressable";
                    return nullptr;
                }
                std::vector<const ast::AstNode*> arguments;
                if (!expression->children().empty())
                {
                    arguments.insert(arguments.end(), expression->children().begin(), expression->children().end());
                }
                else if (expression->second() != nullptr)
                {
                    arguments.push_back(expression->second());
                }
                return lowerResolvedCall(indexerCall, arguments);
            }
            error = "Stage 38 OOP lowering requires class/interface index expressions to resolve to an indexer accessor";
            return nullptr;
        }

        case ast::AstNodeKind::AssignmentExpression:
        {
            if (expression->tokenKind() == lexer::TokenKind::QuestionQuestionEqual)
            {
                if (addressContext)
                {
                    error = "the result of null-coalescing assignment is a value, not addressable storage";
                    return nullptr;
                }
                const MethodExpressionTypeBinding* targetType = expressionType(expression->first());
                if (targetType == nullptr)
                {
                    error = "T01-F03 null-coalescing assignment target has no semantic type";
                    return nullptr;
                }
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                auto fallback = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (address == nullptr || fallback == nullptr) return nullptr;
                const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
                const ProjectImplicitConversion conversion = classifyImplicit(
                    fallback->typeKey,
                    targetType->typeKey,
                    rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                if (!conversion.exists())
                {
                    error = "T01-F03 null-coalescing assignment RHS cannot convert from '" +
                        fallback->typeKey + "' to '" + targetType->typeKey + "'";
                    return nullptr;
                }
                if (conversion.kind != ProjectConversionKind::Identity)
                    fallback = wrapImplicitConversion(
                        std::move(fallback), conversion, expression->second());
                auto node = makeNode(
                    ProjectOopHirNodeKind::NullCoalescingAssignment,
                    expression,
                    targetType->typeKey);
                node->children.push_back(std::move(address));
                node->children.push_back(std::move(fallback));
                return node;
            }

            if (expression->tokenKind() == lexer::TokenKind::Equal &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::TupleExpression)
            {
                if (addressContext)
                {
                    error = "tuple deconstruction result is a value, not addressable storage";
                    return nullptr;
                }
                const MethodDeconstructionBinding* binding =
                    methodBodyModel_->deconstructionBinding(expression);
                if (binding == nullptr)
                {
                    error = "T02-F03 deconstruction assignment has no semantic binding";
                    return nullptr;
                }
                auto source = lowerDeconstructionSource(
                    expression, expression->second(), memberOrdinal, error);
                if (source == nullptr) return nullptr;
                const std::optional<std::vector<std::string>> sourceElements =
                    ProjectTypeConversion::tupleElementTypeKeys(binding->tupleTypeKey);
                if (!sourceElements.has_value() || objectLayoutModel_ == nullptr ||
                    nameLookup_ == nullptr)
                {
                    error = "T02-F03 deconstruction source has no tuple layout";
                    return nullptr;
                }
                auto node = makeNode(
                    ProjectOopHirNodeKind::TupleDeconstruct, expression, binding->tupleTypeKey);
                node->children.push_back(std::move(source));
                const auto expandTargetTuple = [&] (
                    const ast::AstNode* targetSyntax,
                    const std::string& sourceTypeKey,
                    std::uint64_t byteOffset,
                    std::vector<HirTuplePatternFrame>& children) -> bool
                {
                    const std::optional<std::vector<std::string>> nestedElements =
                        ProjectTypeConversion::tupleElementTypeKeys(sourceTypeKey);
                    const std::optional<language::AggregateLayout> nestedLayout =
                        objectLayoutModel_->tupleLayoutForTypeKey(*nameLookup_, sourceTypeKey);
                    if (!nestedElements.has_value() || !nestedLayout.has_value() ||
                        nestedElements->size() != targetSyntax->children().size() ||
                        nestedLayout->offsets.size() != nestedElements->size())
                    {
                        error = "T02-F03 nested assignment target lost its tuple layout";
                        return false;
                    }
                    children.reserve(nestedElements->size());
                    for (std::size_t index = 0; index < nestedElements->size(); ++index)
                        children.push_back(HirTuplePatternFrame{
                            targetSyntax->children()[index],
                            (*nestedElements)[index],
                            byteOffset + nestedLayout->offsets[index]});
                    return true;
                };
                const auto appendTargetLeaf = [&] (
                    const ast::AstNode* targetSyntax,
                    const std::string& sourceTypeKey,
                    std::uint64_t byteOffset) -> bool
                {
                    if (targetSyntax->kind() == ast::AstNodeKind::IdentifierExpression &&
                        targetSyntax->text() == "_") return true;
                    const MethodExpressionTypeBinding* targetType = expressionType(targetSyntax);
                    if (targetType == nullptr)
                    {
                        error = "T02-F03 deconstruction target has no semantic type";
                        return false;
                    }
                    auto address = lowerExpression(targetSyntax, memberOrdinal, true, error);
                    if (address == nullptr) return false;
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        sourceTypeKey, targetType->typeKey, std::nullopt);
                    if (!conversion.exists())
                    {
                        error = "T02-F03 deconstruction lost its validated element conversion";
                        return false;
                    }
                    auto element = makeNode(
                        ProjectOopHirNodeKind::TupleDeconstructElement,
                        targetSyntax,
                        targetType->typeKey);
                    element->elementTypeKey = sourceTypeKey;
                    element->integerValue = byteOffset;
                    element->conversion = conversion;
                    element->children.push_back(std::move(address));
                    node->children.push_back(std::move(element));
                    return true;
                };
                if (!visitHirTuplePatternLeaves(
                        expression->first(),
                        binding->tupleTypeKey,
                        0,
                        expandTargetTuple,
                        appendTargetLeaf))
                {
                    return nullptr;
                }
                return node;
            }

            const MethodEventBinding* stage42Event = eventBinding(expression);
            if (stage42Event != nullptr &&
                (stage42Event->kind == MethodEventOperationKind::Add ||
                 stage42Event->kind == MethodEventOperationKind::Remove))
            {
                if (addressContext)
                {
                    error = "Stage 42 event subscription expression is not addressable";
                    return nullptr;
                }
                if (stage42Event->customAccessorMemberOrdinal.has_value())
                {
                    // NS304-T09-F08: custom accessors -> instancia una llamada a add_/remove_.
                    auto node = makeNode(ProjectOopHirNodeKind::InstanceCall, expression, "builtin:void");
                    node->memberOrdinal = *stage42Event->customAccessorMemberOrdinal;
                    if (!stage42Event->staticEvent)
                    {
                        auto receiver = lowerReceiver(
                            stage42Event->receiverSyntax, stage42Event->receiverTypeKey, memberOrdinal, error);
                        if (receiver == nullptr) return nullptr;
                        node->children.push_back(std::move(receiver));
                    }
                    auto handler = lowerExpression(expression->second(), memberOrdinal, false, error);
                    if (handler == nullptr) return nullptr;
                    if (!stage42Event->argumentConversions.empty() &&
                        stage42Event->argumentConversions.front().kind != ProjectConversionKind::Identity)
                    {
                        handler = wrapImplicitConversion(
                            std::move(handler), stage42Event->argumentConversions.front(), expression->second());
                    }
                    node->children.push_back(std::move(handler));
                    node->argumentParameterOrdinals.push_back(0);
                    node->argumentTypeKeys.push_back(stage42Event->delegateTypeKey);
                    return node;
                }
                auto backingAddress = lowerEventBacking(stage42Event, true);
                auto handler = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (backingAddress == nullptr || handler == nullptr) return nullptr;
                if (!stage42Event->argumentConversions.empty() &&
                    stage42Event->argumentConversions.front().kind != ProjectConversionKind::Identity)
                {
                    handler = wrapImplicitConversion(
                        std::move(handler), stage42Event->argumentConversions.front(), expression->second());
                }
                auto node = makeNode(
                    stage42Event->kind == MethodEventOperationKind::Add
                        ? ProjectOopHirNodeKind::EventAdd : ProjectOopHirNodeKind::EventRemove,
                    expression, "builtin:void");
                node->elementTypeKey = stage42Event->delegateTypeKey;
                node->memberOrdinal = stage42Event->backingMemberOrdinal;
                node->children.push_back(std::move(backingAddress));
                node->children.push_back(std::move(handler));
                return node;
            }

            const MethodCallBinding* setterCall = callBinding(expression);
            if (setterCall != nullptr && setterCall->resolution == MethodCallResolutionKind::Resolved &&
                setterCall->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *setterCall->selectedMemberOrdinal < memberModel_->members().size())
            {
                const ClassMemberSymbol& setter = memberModel_->members()[*setterCall->selectedMemberOrdinal];
                if ((setter.isPropertyAccessor || setter.isIndexerAccessor) && setter.isSetter)
                {
                    std::vector<const ast::AstNode*> arguments;
                    if (setter.isIndexerAccessor && expression->first() != nullptr &&
                        expression->first()->kind() == ast::AstNodeKind::IndexExpression)
                    {
                        const ast::AstNode* indexExpression = expression->first();
                        if (!indexExpression->children().empty())
                        {
                            arguments.insert(
                                arguments.end(), indexExpression->children().begin(), indexExpression->children().end());
                        }
                        else if (indexExpression->second() != nullptr)
                        {
                            arguments.push_back(indexExpression->second());
                        }
                    }
                    arguments.push_back(expression->second());

                    // ID 82. `p.X op= e` es `set_X(p, get_X(p) op e)`. El valor que recibe el
                    // setter se arma AQUI, no se baja de la fuente.
                    //
                    // El operando izquierdo de esa operacion es la llamada al GETTER, y se obtiene
                    // bajando `expression->first()` en contexto de VALOR: los casos
                    // IdentifierExpression/MemberExpression e IndexExpression de esta misma
                    // funcion ya saben convertir ese nodo en la llamada al getter, con su receptor
                    // y sus argumentos de indice. No hay una segunda ruta de resolucion de
                    // accesores aqui; se reusa la unica que hay.
                    //
                    // EL RECEPTOR Y LOS INDICES SE BAJAN DOS VECES -- una dentro de la llamada al
                    // getter y otra para la del setter -- porque el arbol es de `unique_ptr` y un
                    // nodo no se comparte entre dos padres. Eso NO es un descuido: el modelo
                    // semantico ya rechazo toda forma en la que la segunda bajada pudiera dar algo
                    // distinto (isAccessorDestinationReevaluable), asi que lo que llega hasta aqui
                    // son locals, parametros, `this` y constantes. La diferencia con un destino
                    // CON direccion es que aquel si tiene una direccion que bajar una sola vez, y
                    // por eso ese caso lo resuelve `CompoundStore` unas lineas mas abajo.
                    //
                    // El operando derecho se baja UNA sola vez, dentro del `Binary`, DESPUES de la
                    // llamada al getter -- el mismo orden que C#: receptor, indices, getter, lado
                    // derecho, operacion, setter.
                    //
                    // La operacion ocurre entera en el tipo del destino, y la conversion del lado
                    // derecho se clasifica con la MISMA llamada que usa `CompoundStore`. El modelo
                    // semantico ya exigio que exista, asi que si no existe aqui es una discrepancia
                    // entre capas y se reporta como tal, no se ignora.
                    if (expression->tokenKind() != lexer::TokenKind::Equal)
                    {
                        if (addressContext)
                        {
                            error = "the result of a compound assignment is a value, not addressable storage";
                            return nullptr;
                        }
                        const lexer::TokenKind baseOperator =
                            lexer::compoundAssignmentBaseOperator(expression->tokenKind());
                        if (baseOperator == lexer::TokenKind::Invalid)
                        {
                            error = "unsupported compound assignment operator on a property/indexer";
                            return nullptr;
                        }
                        auto current = lowerExpression(expression->first(), memberOrdinal, false, error);
                        if (current == nullptr) return nullptr;
                        auto operand = lowerExpression(expression->second(), memberOrdinal, false, error);
                        if (operand == nullptr) return nullptr;
                        const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
                        const ProjectImplicitConversion conversion = classifyImplicit(
                            operand->typeKey,
                            current->typeKey,
                            rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                        const bool shift = baseOperator == lexer::TokenKind::LessLess ||
                            baseOperator == lexer::TokenKind::GreaterGreater;
                        if (!shift && !conversion.exists())
                        {
                            error = "compound assignment to a property/indexer cannot convert its "
                                "right-hand side from '" + operand->typeKey + "' to '" +
                                current->typeKey + "'";
                            return nullptr;
                        }
                        if (!shift && conversion.kind != ProjectConversionKind::Identity)
                        {
                            operand = wrapImplicitConversion(
                                std::move(operand), conversion, expression->second());
                        }
                        auto combined = makeNode(
                            ProjectOopHirNodeKind::Binary, expression, current->typeKey);
                        combined->operation = baseOperator;
                        combined->children.push_back(std::move(current));
                        combined->children.push_back(std::move(operand));
                        auto setterNode = lowerResolvedCall(
                            setterCall, arguments, std::move(combined));
                        if (setterNode != nullptr) setterNode->returnsAssignedValue = true;
                        return setterNode;
                    }
                    auto setterNode = lowerResolvedCall(setterCall, arguments);
                    if (setterNode != nullptr) setterNode->returnsAssignedValue = true;
                    return setterNode;
                }
            }

            // NS304-T08-F02: an extension-property assignment is a synthetic namespace setter call
            // recorded under the assignment node (`set_Name(receiver, value)`). Emit it as a
            // StaticCall whose first argument is the receiver and whose last argument is the RHS.
            const NamespaceFunctionCallBinding* namespaceSetter =
                namespaceFunctionCallBinding(expression);
            if (namespaceSetter != nullptr &&
                namespaceSetter->resolution == MethodCallResolutionKind::Resolved &&
                namespaceSetter->selectedFunctionOrdinal.has_value() &&
                namespaceFunctionModel_ != nullptr &&
                *namespaceSetter->selectedFunctionOrdinal < namespaceFunctionModel_->functions().size())
            {
                if (addressContext)
                {
                    error = "the result of an extension-property assignment is a value, not addressable storage";
                    return nullptr;
                }
                const std::size_t functionOrdinal = *namespaceSetter->selectedFunctionOrdinal;
                const NamespaceFunctionSymbol& target =
                    namespaceFunctionModel_->functions()[functionOrdinal];
                auto node = makeNode(
                    ProjectOopHirNodeKind::StaticCall, expression, namespaceSetter->resultTypeKey);
                node->namespaceFunctionOrdinal = functionOrdinal;
                node->stringValue = target.mangledName;
                if (namespaceSetter->forwardedReceiver)
                {
                    if (namespaceSetter->receiverSyntax == nullptr || target.parameters.empty())
                    {
                        error = "extension setter lost its synthetic receiver contract";
                        return nullptr;
                    }
                    auto receiver = lowerExpression(
                        namespaceSetter->receiverSyntax, memberOrdinal, false, error);
                    if (receiver == nullptr) return nullptr;
                    if (!namespaceSetter->selectedConversions.empty() &&
                        namespaceSetter->selectedConversions.front().kind !=
                            ProjectConversionKind::Identity)
                    {
                        receiver = wrapImplicitConversion(
                            std::move(receiver), namespaceSetter->selectedConversions.front(),
                            namespaceSetter->receiverSyntax);
                    }
                    node->children.push_back(std::move(receiver));
                    node->argumentParameterOrdinals.push_back(0);
                    node->argumentTypeKeys.push_back(target.parameters[0].typeKey);
                }
                auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                const std::size_t valueParameterOrdinal = namespaceSetter->forwardedReceiver ? 1 : 0;
                if (valueParameterOrdinal >= target.parameters.size())
                {
                    error = "extension setter has no value parameter";
                    return nullptr;
                }
                if (valueParameterOrdinal < namespaceSetter->selectedConversions.size() &&
                    namespaceSetter->selectedConversions[valueParameterOrdinal].kind !=
                        ProjectConversionKind::Identity)
                {
                    value = wrapImplicitConversion(
                        std::move(value),
                        namespaceSetter->selectedConversions[valueParameterOrdinal],
                        expression->second());
                }
                node->children.push_back(std::move(value));
                node->argumentParameterOrdinals.push_back(valueParameterOrdinal);
                node->argumentTypeKeys.push_back(target.parameters[valueParameterOrdinal].typeKey);
                node->returnsAssignedValue = true;
                return node;
            }

            // NS304-T10-F05/F06: the receiver remains the first child so LLVM can branch before
            // lowering the RHS. booleanValue carries the semantic owning-destination contract.
            if (expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression)
            {
                const MethodNameBinding* binding = nameBinding(expression->first());
                if (binding == nullptr || binding->candidateMemberOrdinals.size() != 1 ||
                    binding->kind != MethodNameBindingKind::ExplicitObjectField)
                {
                    error = "T10-F05 null-conditional assignment has no unique instance-field binding";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first()->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::NullConditionalFieldStore, expression, resultType);
                node->operation = expression->tokenKind();
                const MethodExpressionTypeBinding* assignmentType = expressionType(expression);
                node->booleanValue = assignmentType != nullptr &&
                    assignmentType->ownsStorageDestination;
                node->memberOrdinal = binding->candidateMemberOrdinals.front();
                node->volatileAccess = memberModel_ != nullptr &&
                    *node->memberOrdinal < memberModel_->members().size() &&
                    memberModel_->members()[*node->memberOrdinal].isVolatile;
                node->children.push_back(std::move(receiver));
                node->children.push_back(std::move(value));
                return node;
            }

            // NS304-T10-F05: receiver, index, and value remain separate children so only the
            // receiver is evaluated before the null branch.
            if (expression->tokenKind() == lexer::TokenKind::Equal &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::NullConditionalIndexExpression)
            {
                const MethodCoreBinding* core = coreBinding(expression->first());
                if (core == nullptr ||
                    core->kind != MethodCoreOperationKind::NullConditionalArrayElement)
                {
                    error = "T10-F05 null-conditional index store has no native-array element binding";
                    return nullptr;
                }
                auto receiver = lowerExpression(expression->first()->first(), memberOrdinal, false, error);
                if (receiver == nullptr) return nullptr;
                const ast::AstNode* indexSyntax = !expression->first()->children().empty()
                    ? expression->first()->children().front() : expression->first()->second();
                auto index = lowerExpression(indexSyntax, memberOrdinal, false, error);
                if (index == nullptr) return nullptr;
                auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                auto node = makeNode(
                    ProjectOopHirNodeKind::NullConditionalArrayStore, expression, resultType);
                node->elementTypeKey = core->elementTypeKey;
                const MethodExpressionTypeBinding* assignmentType = expressionType(expression);
                node->booleanValue = assignmentType != nullptr &&
                    assignmentType->ownsStorageDestination;
                node->children.push_back(std::move(receiver));
                node->children.push_back(std::move(index));
                node->children.push_back(std::move(value));
                return node;
            }

            auto node = makeNode(ProjectOopHirNodeKind::Assignment, expression, resultType);
            node->operation = expression->tokenKind();
            if (node->operation == lexer::TokenKind::Equal &&
                expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::IdentifierExpression)
            {
                const MethodNameBinding* targetBinding = nameBinding(expression->first());
                if (targetBinding != nullptr && targetBinding->localOrdinal.has_value() &&
                    *targetBinding->localOrdinal < methodBodyModel_->locals().size() &&
                    (methodBodyModel_->locals()[*targetBinding->localOrdinal].isRaii ||
                     methodBodyModel_->locals()[*targetBinding->localOrdinal].requiresValueDrop))
                {
                    node->booleanValue = true;
                    node->localOrdinal = *targetBinding->localOrdinal;
                }
            }
            // Owning replacement is a semantic property of the destination, not an AST pattern.
            // Fields and elements of owned arrays both lower to an address-based store; the
            // semantic binding records which stores own their current value so this layer can
            // request the existing release-before-store path without rediscovering declarations.
            if (!node->booleanValue)
            {
                const MethodExpressionTypeBinding* assignmentType = expressionType(expression);
                node->booleanValue = assignmentType != nullptr &&
                    assignmentType->ownsStorageDestination;
            }
            // T03-F01/T06-F08: a move-only aggregate owns its current active payload even when
            // the replacement is a freshly returned value rather than an explicit move(local).
            // Mark every ordinary store of a drop-bearing value so the backend destroys the
            // displaced value after evaluating the RHS and before writing the replacement.
            if (!node->booleanValue && node->operation == lexer::TokenKind::Equal &&
                memberModel_ != nullptr && memberModel_->typeRequiresValueDrop(resultType))
            {
                node->booleanValue = true;
            }
            if (node->operation != lexer::TokenKind::Equal)
            {
                // ID 81. Los diez operadores compuestos. El destino se baja UNA VEZ en contexto de
                // direccion -- si se desazucarara a `x = x + e` se evaluaria dos veces y
                // `a[f()] += 1` llamaria a `f` dos veces.
                if (addressContext)
                {
                    error = "the result of a compound assignment is a value, not addressable storage";
                    return nullptr;
                }
                // T41: `raii string += value` is an ownership-aware replacement, not a numeric
                // CompoundStore. The RAII destination is bound specially by semantic analysis,
                // so its identifier does not need a separate expression-type record here; the
                // assignment result type is the authoritative resolved type.
                if (node->operation == lexer::TokenKind::PlusEqual &&
                    resultType == "builtin:string")
                {
                    const MethodNameBinding* targetBinding = nameBinding(expression->first());
                    if (targetBinding == nullptr || !targetBinding->localOrdinal.has_value())
                    {
                        error = "string += requires an identifier-backed RAII string destination";
                        return nullptr;
                    }
                    auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                    auto current = lowerExpression(expression->first(), memberOrdinal, false, error);
                    auto suffix = lowerExpression(expression->second(), memberOrdinal, false, error);
                    if (address == nullptr || current == nullptr || suffix == nullptr) return nullptr;

                    auto concatenated = makeNode(
                        ProjectOopHirNodeKind::StringConcat, expression, "builtin:string");
                    concatenated->operation = lexer::TokenKind::Plus;
                    concatenated->children.push_back(std::move(current));
                    concatenated->children.push_back(std::move(suffix));

                    node->operation = lexer::TokenKind::Equal;
                    node->booleanValue = true;
                    node->localOrdinal = *targetBinding->localOrdinal;
                    node->children.push_back(std::move(address));
                    node->children.push_back(std::move(concatenated));
                    return node;
                }
                const MethodExpressionTypeBinding* targetType = expressionType(expression->first());
                if (targetType == nullptr)
                {
                    error = "Stage 34 compound assignment target has no resolved semantic type";
                    return nullptr;
                }
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;
                auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;

                auto compound = makeNode(
                    ProjectOopHirNodeKind::CompoundStore, expression, targetType->typeKey);
                compound->operation = lexer::compoundAssignmentBaseOperator(node->operation);
                if (compound->operation == lexer::TokenKind::Invalid)
                {
                    error = "unsupported compound assignment operator";
                    return nullptr;
                }
                // El valor de `x += e` es el NUEVO, como en C#.
                compound->booleanValue = false;

                if (ProjectTypeConversion::isPointer(targetType->typeKey))
                {
                    compound->elementTypeKey =
                        targetType->typeKey.substr(0, targetType->typeKey.size() - 1);
                }
                else
                {
                    // La operacion ocurre ENTERA en el tipo del destino. El modelo semantico ya
                    // exigio que el operando derecho convierta a ese tipo (ver analyzeAssignment),
                    // asi que aqui no hay ni un estrechamiento silencioso ni una segunda tabla de
                    // promociones distinta de la de `Binary`.
                    const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        value->typeKey,
                        targetType->typeKey,
                        rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
                    const bool shift = compound->operation == lexer::TokenKind::LessLess ||
                        compound->operation == lexer::TokenKind::GreaterGreater;
                    if (!shift && !conversion.exists())
                    {
                        error = "compound assignment cannot convert its right-hand side from '" +
                            value->typeKey + "' to '" + targetType->typeKey + "'";
                        return nullptr;
                    }
                    if (!shift && conversion.kind != ProjectConversionKind::Identity)
                    {
                        value = wrapImplicitConversion(
                            std::move(value), conversion, expression->second());
                    }
                }
                compound->children.push_back(std::move(address));
                compound->children.push_back(std::move(value));
                return compound;
            }
            auto target = lowerExpression(expression->first(), memberOrdinal, true, error);
            auto value = lowerExpression(expression->second(), memberOrdinal, false, error);
            if (target == nullptr || value == nullptr)
            {
                return nullptr;
            }
            const MethodExpressionTypeBinding* rhsType = expressionType(expression->second());
            const ProjectImplicitConversion conversion = classifyImplicit(
                value->typeKey,
                target->typeKey,
                rhsType != nullptr ? rhsType->integerConstant : std::nullopt);
            if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
            {
                value = wrapImplicitConversion(std::move(value), conversion, expression->second());
            }
            node->children.push_back(std::move(target));
            node->children.push_back(std::move(value));
            return node;
        }

        case ast::AstNodeKind::MoveExpression:
        {
            if (addressContext)
            {
                error = "Stage 49 move value is not addressable";
                return nullptr;
            }
            const MethodMoveBinding* move = moveBinding(expression);
            if (move == nullptr)
            {
                error = "Stage 49 move expression has no semantic move binding";
                return nullptr;
            }
            if (move->sourceIsArrayElement)
            {
                // Stage 52: move(items[i]) extracts an owned element out of native-array
                // backing storage (needed by Queue<T>.Dequeue()/Stack<T>.Pop()). Reuse the
                // existing IndexExpression address-lowering path (ArrayElementAddress) to
                // compute the slot's address, then wrap it in a dedicated node so LLVM
                // lowering can load the value and, only for reference-like elements,
                // null out the slot to prevent a later double-free/double-use. Value-typed
                // elements (int, bool, a plain struct, ...) have no ownership to transfer,
                // so the load degrades to a harmless plain read with no null-store.
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;
                auto node = makeNode(ProjectOopHirNodeKind::MoveArrayElement, expression, move->typeKey);
                node->booleanValue = move->clearSourceStorage;
                node->children.push_back(std::move(address));
                return node;
            }
            if (move->sourceIsOwnedField)
            {
                // Bloque 1: move(holder.slot) / move(slot) -- take the owner OUT of an `owned`
                // field. Same address-shaped contract as the array-element case above: compute
                // the slot's address here, load-and-null in the backend. The null-store is what
                // makes the container's own discharge (destruction entry, overwrite release) see
                // an empty slot instead of double-freeing the value that just left.
                auto address = lowerExpression(expression->first(), memberOrdinal, true, error);
                if (address == nullptr) return nullptr;
                auto node = makeNode(ProjectOopHirNodeKind::MoveOwnedField, expression, move->typeKey);
                node->booleanValue = move->clearSourceStorage;
                node->children.push_back(std::move(address));
                return node;
            }
            if (move->sourceIsIteratorCurrent)
            {
                // NS304-T07-F04: move(it.Current) takes the move-only item out of the iterator's
                // item slot (field 2). The child is the iterator's ADDRESS; the backend loads the
                // item slot and clears it so the Drop never double-frees the transferred value.
                auto iterator = lowerExpression(expression->first()->first(), memberOrdinal, true, error);
                if (iterator == nullptr) return nullptr;
                auto node = makeNode(ProjectOopHirNodeKind::MoveIteratorCurrent, expression, move->typeKey);
                node->booleanValue = move->clearSourceStorage;
                node->children.push_back(std::move(iterator));
                return node;
            }
            auto node = makeNode(ProjectOopHirNodeKind::MoveValue, expression, move->typeKey);
            node->localOrdinal = move->sourceLocalOrdinal;
            node->parameterOrdinal = move->sourceParameterOrdinal;
            // Stage 52: whether the source is actually reference-like (needs the load+null-store
            // treatment) or a plain value type closed over a generic T -- see
            // MethodMoveBinding::clearSourceStorage's comment and the MoveValue LLVM
            // lowering case for the bug this fixes.
            node->booleanValue = move->clearSourceStorage;
            if (node->localOrdinal.has_value() == node->parameterOrdinal.has_value())
            {
                error = "Stage 50.8 move value must identify exactly one owner source";
                return nullptr;
            }
            return node;
        }

        case ast::AstNodeKind::ResultPropagationExpression:
        {
            if (addressContext)
            {
                error = "postfix '?' produces a value, not addressable storage";
                return nullptr;
            }
            const MethodResultPropagationBinding* binding =
                resultPropagationBinding(expression);
            const MethodStructuredExitBinding* exit = structuredExitBinding(expression);
            if (binding == nullptr || exit == nullptr ||
                exit->kind != MethodStructuredExitKind::Return)
            {
                error = "T07-F01 Result propagation has no complete semantic binding";
                return nullptr;
            }
            auto operand = lowerExpression(
                binding->operandSyntax, memberOrdinal, false, error);
            if (operand == nullptr) return nullptr;

            auto node = makeNode(
                ProjectOopHirNodeKind::ResultPropagation, expression, resultType);
            node->resultSourceCarrierTypeKey = binding->sourceCarrierTypeKey;
            node->resultSourceErrorTypeKey = binding->sourceErrorTypeKey;
            node->resultReturnCarrierTypeKey = binding->returnCarrierTypeKey;
            node->resultReturnErrorTypeKey = binding->returnErrorTypeKey;
            node->resultSuccessTag = binding->sourceSuccessTag;
            node->resultErrorTag = binding->sourceErrorTag;
            node->resultReturnErrorTag = binding->returnErrorTag;
            node->resultSuccessTestMemberOrdinal =
                binding->sourceSuccessTestMemberOrdinal;
            node->resultSuccessExtractorMemberOrdinal =
                binding->sourceSuccessExtractorMemberOrdinal;
            node->resultErrorExtractorMemberOrdinal =
                binding->sourceErrorExtractorMemberOrdinal;
            node->resultErrorFactoryMemberOrdinal =
                binding->returnErrorFactoryMemberOrdinal;
            node->resultErrorConversionMemberOrdinal =
                binding->errorConversionMemberOrdinal;
            node->cleanupRegionOrdinals = exit->cleanupRegionOrdinals;
            node->cleanupContinuationKind = MethodCleanupContinuationKind::Return;
            node->children.push_back(std::move(operand));
            return node;
        }

        case ast::AstNodeKind::CallExpression:
            return lowerCallExpression(expression, memberOrdinal, addressContext, error);

        // CC1 fase 4B -- `stackalloc T[n]`. Un alloca de arreglo y nada mas: sin cabecera de
        // longitud, sin comprobacion de rango, sin liberacion. Es el nivel crudo, igual que
        // `p[i]`, y la validacion de que el elemento puede vivir ahi ya la hizo el modelo
        // semantico (sin destructor y sin campos `owned`).
        case ast::AstNodeKind::StackallocExpression:
        case ast::AstNodeKind::NewExpression:
        case ast::AstNodeKind::CollectionExpression:
            return lowerArrayConstructionExpression(
                expression, memberOrdinal, addressContext, error);

        case ast::AstNodeKind::ObjectCreationExpression:
        case ast::AstNodeKind::PlacementConstructionExpression:
            return lowerObjectConstructionExpression(
                expression, memberOrdinal, addressContext, error);

        case ast::AstNodeKind::TupleExpression:
        case ast::AstNodeKind::AggregateInitializerExpression:
        case ast::AstNodeKind::WithExpression:
            return lowerAggregateOrLayoutExpression(expression, memberOrdinal, error);
        case ast::AstNodeKind::DeleteExpression:
        case ast::AstNodeKind::DestroyExpression:
            return lowerDestructionExpression(expression, memberOrdinal, error);
        case ast::AstNodeKind::TypeTestExpression:
        case ast::AstNodeKind::SafeCastExpression:
        case ast::AstNodeKind::CastExpression:
            return lowerConversionExpression(
                expression, memberOrdinal, addressContext, error, typeBinding, resultType);

        case ast::AstNodeKind::SizeofTypeExpression:
        case ast::AstNodeKind::SizeofExpression:
        case ast::AstNodeKind::AlignofTypeExpression:
        case ast::AstNodeKind::AlignofExpression:
        case ast::AstNodeKind::OffsetofExpression:
            return lowerAggregateOrLayoutExpression(expression, memberOrdinal, error);
        // ID 80, CERRADA. Aqui habia un rechazo con mensaje propio para `p->campo`: era la ultima
        // forma de expresion con la mitad delantera del pipeline presente y la trasera ausente,
        // despues de sizeof, stackalloc, raw new y la aritmetica de punteros.
        //
        // Se cerro donde tocaba, que no era aqui: el parser convierte `p->x` en `(*p).x` (ver
        // Parser::parsePostfixExpression), asi que este nodo ya no se construye y este caso ya no
        // se alcanza. Anadir en su lugar un lowering propio habria significado una SEGUNDA ruta de
        // acceso a miembro que mantener de acuerdo con la de `.` -- que es como se abrio el hueco.
        //
        // NOTA sobre como se hizo el inventario que la encontro: un barrido de las 30 formas de
        // expresion del AST contra los `case` de esta funcion daba TAMBIEN `LambdaExpression`. Es
        // un falso positivo del barrido: las lambdas se bajan por binding ANTES del switch, no por
        // un `case`. Comprobado antes de escribirlo -- el barrido mide lo que mide, no lo que uno
        // quiere que mida.

        case ast::AstNodeKind::AwaitExpression:
            error = "await expression escaped iterative Stage 34 OOP HIR lowering";
            return nullptr;

        default:
            error = "unsupported Stage 34 OOP expression kind: " +
                std::string(ast::astNodeKindName(expression->kind()));
            return nullptr;
    }
}


// NSC-LIMIT-008. Is this name actually a GETTER CALL?
//
// `source.Current` and `table[key]` are written like storage and parse like storage, but if the
// semantic model bound them to a property or indexer accessor they are calls: they RETURN a value,
// and that value has no home. Syntax cannot tell the two apart -- `point.X` is a field on one type
// and a property on another, and the difference is only known after member resolution.
//
// Two places need this answer and they used to disagree. lowerExpression asked it (the
// MemberExpression case, which rejects `addressContext` for a getter) while isNaturallyAddressable
// did not, and answered `true` for every MemberExpression. That disagreement is the whole defect:
// `source.Current.Read()` classified the getter as addressable, so lowerReceiver never entered its
// TemporaryAddress path and the struct method received the address of something that was never
// stored.
const MethodCallBinding* ProjectOopHirModel::accessorGetterBinding(
    const ast::AstNode* expression) const noexcept
{
    if (expression == nullptr || memberModel_ == nullptr) return nullptr;
    switch (expression->kind())
    {
        // The three spellings a getter can wear. `Current` unqualified inside its own class is an
        // IdentifierExpression, `source.Current` a MemberExpression, `table[key]` an
        // IndexExpression -- and all three are calls once the model has resolved them.
        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
        case ast::AstNodeKind::NullConditionalMemberExpression:
        case ast::AstNodeKind::IndexExpression:
            break;
        default:
            return nullptr;
    }
    const MethodCallBinding* accessorCall = callBinding(expression);
    if (accessorCall == nullptr ||
        accessorCall->resolution != MethodCallResolutionKind::Resolved ||
        !accessorCall->selectedMemberOrdinal.has_value() ||
        *accessorCall->selectedMemberOrdinal >= memberModel_->members().size())
    {
        // No binding at all is the ORDINARY case, not an error: a real field, a real array element
        // and a real fixed-array element never produce one. They keep their own address.
        return nullptr;
    }
    const ClassMemberSymbol& selected = memberModel_->members()[*accessorCall->selectedMemberOrdinal];
    // PROPIEDAD *O* INDEXADOR. `isIndexerAccessor` es una bandera distinta de `isPropertyAccessor`,
    // no un caso suyo, y mirar solo la primera dejaba fuera `obj[i].MetodoDeStruct()`: el
    // IndexExpression entraba por el switch de arriba, resolvia a un getter de indexador, y este
    // filtro lo dejaba pasar como si fuera almacenamiento. Exactamente el mismo defecto que este
    // ID cierra, en la otra mitad de la sintaxis.
    if (!selected.isPropertyAccessor && !selected.isIndexerAccessor) return nullptr;
    if (!selected.isGetter) return nullptr;
    return accessorCall;
}

// C6 "struct members": does this expression denote storage that already exists? An identifier, a
// field of one, an element of one, and a dereference all do -- and a parenthesized form of any of
// them does too. A call, an operator, a conversion or a `new` does not: its result is a value with
// no home. Only the second group needs a temporary made for it.
//
// NSC-LIMIT-008 added the one case where syntax lies: a property or indexer getter is written like
// a field or an element and is a call. See accessorGetterBinding.
bool ProjectOopHirModel::isNaturallyAddressable(const ast::AstNode* expression) const noexcept
{
    if (expression == nullptr) return false;
    switch (expression->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
        case ast::AstNodeKind::IndexExpression:
        {
            // A real field or element is storage. An ordinary accessor returns a value with no
            // home, while a ref-returning accessor deliberately returns existing storage.
            const MethodCallBinding* accessor = accessorGetterBinding(expression);
            if (accessor == nullptr) return true;
            return accessor->selectedMemberOrdinal.has_value() && memberModel_ != nullptr &&
                *accessor->selectedMemberOrdinal < memberModel_->members().size() &&
                memberModel_->members()[*accessor->selectedMemberOrdinal].isReferenceReturn;
        }
        case ast::AstNodeKind::ThisExpression:
        case ast::AstNodeKind::BaseExpression:
        case ast::AstNodeKind::MatchValueExpression:
            return true;
        case ast::AstNodeKind::UnaryExpression:
            // `*p` is storage; `-v` is a value.
            return expression->tokenKind() == lexer::TokenKind::Star;
        default:
            return false;
    }
}


}
