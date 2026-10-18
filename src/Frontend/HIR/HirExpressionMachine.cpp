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

// Executes expression lowering with an optional bottom frame that owns the
// statement continuation. Ordinary expression callers pass no such frame.
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerExpressionWithStatementFrame(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    const ast::AstNode* expressionStatement,
    std::string& error)
{
    std::unique_ptr<ProjectOopHirNode> lowered;
    enum class IterativeExpressionKind : std::uint8_t
    {
        ExpressionStatement,
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
        std::vector<language::InterpolationPart> interpolationParts;
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
        if (syntax->kind() == ast::AstNodeKind::AwaitExpression)
            return IterativeExpressionKind::AwaitExpression;
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

    const std::optional<IterativeExpressionKind> rootKind =
        iterativeKind(expression, addressContext);
    if (rootKind.has_value() || expressionStatement != nullptr)
    {
        std::vector<IterativeExpressionFrame> frames;
        if (expressionStatement != nullptr)
        {
            IterativeExpressionFrame frame;
            frame.kind = IterativeExpressionKind::ExpressionStatement;
            frame.syntax = expressionStatement;
            frames.push_back(std::move(frame));
        }
        const ast::AstNode* current = expression;
        bool currentAddressContext = addressContext;
        std::size_t currentMemberOrdinal = memberOrdinal;
        IterativeInputKind currentInputKind = IterativeInputKind::Expression;
        const std::size_t rootCleanupDepth = activeHirCleanupRegions_.size();
        const auto rollbackMatchCleanups = [&]()
        {
            bool loopBlockActive = false;
            for (auto frame = frames.rbegin(); frame != frames.rend(); ++frame)
            {
                if (frame->kind == IterativeExpressionKind::Match && frame->cleanupActive)
                    activeHirCleanupRegions_.resize(frame->cleanupDepth);
                if (frame->kind == IterativeExpressionKind::LoopBlock)
                    loopBlockActive = true;
            }
            if (loopBlockActive) activeHirCleanupRegions_.resize(rootCleanupDepth);
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
        enum class LoopBlockAdvance : std::uint8_t
        {
            Descend,
            Complete,
            Error,
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

                std::unique_ptr<ProjectOopHirNode> statementNode =
                    lowerStatement(statement, frame.memberOrdinal, error);
                if (statementNode == nullptr) return LoopBlockAdvance::Error;
                if (statementNode->kind == ProjectOopHirNodeKind::LocalDeclaration &&
                    statementNode->cleanupRegionOrdinal.has_value())
                {
                    activeHirCleanupRegions_.push_back(
                        *statementNode->cleanupRegionOrdinal);
                }
                frame.loopBlockNode->children.push_back(std::move(statementNode));
            }
        };
        while (true)
        {
            if (currentInputKind == IterativeInputKind::LoopBlock)
            {
                if (current == nullptr || current->kind() != ast::AstNodeKind::BlockStatement)
                {
                    error = "Stage 34 OOP HIR requires block statement body";
                    if (!lastErrorRange_.isValid() && current != nullptr)
                        lastErrorRange_ = current->range();
                    rollbackMatchCleanups();
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
                    return nullptr;
                }
            }
            else
            {
                const std::optional<IterativeExpressionKind> currentKind =
                    iterativeKind(current, currentAddressContext);
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
                        return nullptr;
                    }
                    frame.loopExpressionBinding = methodBodyModel_ != nullptr
                        ? methodBodyModel_->loopExpressionBinding(current) : nullptr;
                    if (frame.loopExpressionBinding == nullptr)
                    {
                        error = "T07-F05 loop expression has no semantic binding";
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
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
                        return nullptr;
                    }
                }
                else if (*currentKind == IterativeExpressionKind::NamespaceCall)
                {
                    if (!initializeNamespaceCallFrame(frame))
                    {
                        if (!lastErrorRange_.isValid()) lastErrorRange_ = current->range();
                        rollbackMatchCleanups();
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
                continue;
            }

            const bool isExpressionStatementRoot =
                frames.size() == 1u &&
                frames.back().kind == IterativeExpressionKind::ExpressionStatement;
            lowered = isExpressionStatementRoot
                ? lowerExpressionImpl(
                    current, currentMemberOrdinal, currentAddressContext, error)
                : lowerExpression(
                    current, currentMemberOrdinal, currentAddressContext, error);
            if (lowered == nullptr)
            {
                rollbackMatchCleanups();
                if (!lastErrorRange_.isValid() && !frames.empty() &&
                    frames.back().kind != IterativeExpressionKind::ExpressionStatement)
                    lastErrorRange_ = frames.back().syntax->range();
                break;
            }
            }

            currentInputKind = IterativeInputKind::Expression;
            bool descend = false;
            while (!frames.empty())
            {
                IterativeExpressionFrame& frame = frames.back();
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
                    const auto beginValue = [&]() -> bool
                    {
                        const ast::AstNode* armSyntax =
                            frame.syntax->children()[frame.armIndex];
                        frame.cleanupDepth = activeHirCleanupRegions_.size();
                        frame.cleanupActive = true;
                        for (const ast::AstNode* declaration : armSyntax->children())
                        {
                            auto declarationNode = lowerStatement(
                                declaration, frame.memberOrdinal, error);
                            if (declarationNode == nullptr)
                            {
                                activeHirCleanupRegions_.resize(frame.cleanupDepth);
                                frame.cleanupActive = false;
                                return false;
                            }
                            if (declarationNode->kind ==
                                    ProjectOopHirNodeKind::LocalDeclaration &&
                                declarationNode->cleanupRegionOrdinal.has_value())
                            {
                                activeHirCleanupRegions_.push_back(
                                    *declarationNode->cleanupRegionOrdinal);
                            }
                            frame.matchArm->children.push_back(std::move(declarationNode));
                        }
                        frame.stage = 3U;
                        current = armSyntax->third();
                        currentMemberOrdinal = frame.memberOrdinal;
                        currentAddressContext = frame.matchBinding->referenceResult;
                        return true;
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
            if (!descend) break;
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
    if (expressionStatement != nullptr && loweredExpression != nullptr &&
        loweredExpression->kind == ProjectOopHirNodeKind::ExpressionStatement &&
        loweredExpression->children.size() == 1U)
    {
        loweredExpression = loweredExpression->children.front().get();
    }
    if (lowered != nullptr && effects.allows(sema::Effect::NoHeap) &&
        loweredExpression != nullptr && directlyAllocatesHeap(loweredExpression->kind))
    {
        error = "NS3030: noheap callable '" + activeMember->name +
            "' cannot contain direct heap allocation '" +
            std::string(projectOopHirNodeKindName(loweredExpression->kind)) + "'";
        lowered.reset();
    }
    if (lowered != nullptr && effects.allows(sema::Effect::NoHeap))
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
