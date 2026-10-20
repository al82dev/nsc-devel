#include "../HirLoweringSupport.h"
#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Language/ExpressionErgonomics.h"

#include <algorithm>
#include <optional>

namespace nativesharp::frontend
{
bool ProjectOopHirModel::isInterpolationHole(
    const language::InterpolationPart& part) noexcept
{
    return part.kind == language::InterpolationPartKind::Hole;
}

bool ProjectOopHirModel::isSpreadElement(const ast::AstNode* child) noexcept
{
    return child != nullptr &&
        child->kind() == ast::AstNodeKind::SpreadElementExpression;
}

using internal::IterativeExpressionKind;

// Clasifica una expresion para la maquina iterativa sin alterar el orden de las pruebas.
// Classifies an expression for the iterative machine without changing test order.
std::optional<IterativeExpressionKind> ProjectOopHirModel::iterativeExpressionKind(
    const ast::AstNode* syntax,
    bool address)
{
    if (syntax == nullptr) return std::nullopt;

    const auto classifiers = {
        &ProjectOopHirModel::iterativeDelegateAndCallKind,
        &ProjectOopHirModel::iterativeCoreCallKind,
        &ProjectOopHirModel::iterativeSpecialCallKind,
        &ProjectOopHirModel::iterativeConstructionKind,
        &ProjectOopHirModel::iterativeOperatorKind,
        &ProjectOopHirModel::iterativeControlFlowKind,
        &ProjectOopHirModel::iterativeSetterAssignmentKind,
        &ProjectOopHirModel::iterativeExitAndDestructionKind,
        &ProjectOopHirModel::iterativeStorageKind,
        &ProjectOopHirModel::iterativeMemberKind,
        &ProjectOopHirModel::iterativeIndexKind,
        &ProjectOopHirModel::iterativeUserIndexAndFieldKind};
    for (const auto classifier : classifiers)
    {
        const std::optional<IterativeExpressionKind> kind =
            (this->*classifier)(syntax, address);
        if (kind.has_value()) return kind;
    }
    return std::nullopt;
}

// Conserva primero la clasificacion de delegados y llamadas con receptor.
// Preserves delegate and receiver-call classification first.
std::optional<IterativeExpressionKind> ProjectOopHirModel::iterativeDelegateAndCallKind(
    const ast::AstNode* syntax,
    bool)
{
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
    if (syntax->kind() != ast::AstNodeKind::CallExpression) return std::nullopt;

    const NamespaceFunctionCallBinding* namespaceCall =
        namespaceFunctionCallBinding(syntax);
    if (namespaceCall != nullptr &&
        (namespaceCall->forwardedReceiver || !syntax->children().empty()))
    {
        return IterativeExpressionKind::NamespaceCall;
    }
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
    return std::nullopt;
}

// Conserva la seleccion de llamadas core que requieren frames dedicados.
// Preserves selection of core calls that require dedicated frames.
std::optional<IterativeExpressionKind> ProjectOopHirModel::iterativeCoreCallKind(
    const ast::AstNode* syntax,
    bool address)
{
    if (syntax->kind() != ast::AstNodeKind::CallExpression) return std::nullopt;

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
    const MethodCoreBinding* atomicCore = coreBinding(syntax);
    if (atomicCore != nullptr && hirAtomicCallPlan(atomicCore->kind).has_value())
        return IterativeExpressionKind::AtomicCoreCall;
    const MethodCoreBinding* monitorCore = coreBinding(syntax);
    if (monitorCore != nullptr && hirMonitorCallPlan(monitorCore->kind).has_value())
        return IterativeExpressionKind::MonitorCoreCall;
    return std::nullopt;
}

// Conserva las llamadas core terminales y la llamada ordinaria posterior.
// Preserves terminal core calls followed by ordinary-call classification.
std::optional<IterativeExpressionKind> ProjectOopHirModel::iterativeSpecialCallKind(
    const ast::AstNode* syntax,
    bool)
{
    if (syntax->kind() != ast::AstNodeKind::CallExpression) return std::nullopt;

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
    if (core != nullptr || namespaceFunctionCallBinding(syntax) != nullptr ||
        eventBinding(syntax) != nullptr || delegateBinding(syntax) != nullptr)
    {
        return std::nullopt;
    }
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
    return std::nullopt;
}

// Conserva el orden de construccion, espera y agregados.
// Preserves construction, await, and aggregate ordering.
std::optional<IterativeExpressionKind> ProjectOopHirModel::iterativeConstructionKind(
    const ast::AstNode* syntax,
    bool address)
{
    if (!address && syntax->kind() == ast::AstNodeKind::PlacementConstructionExpression)
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
        if (core != nullptr && core->kind == MethodCoreOperationKind::CollectionBuilder)
            return IterativeExpressionKind::CollectionBuilder;
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
        return IterativeExpressionKind::AggregateCrossLowering;
    if (syntax->kind() == ast::AstNodeKind::LoopExpression)
        return IterativeExpressionKind::LoopExpression;
    if (!address && syntax->kind() == ast::AstNodeKind::InterpolatedStringExpression &&
        !syntax->children().empty())
    {
        const MethodCoreBinding* core = coreBinding(syntax);
        if (core != nullptr && core->kind == MethodCoreOperationKind::InterpolationHandler)
            return IterativeExpressionKind::InterpolationHandler;
    }
    return std::nullopt;
}

// Conserva la clasificacion de operadores y conversiones.
// Preserves operator and conversion classification.
std::optional<IterativeExpressionKind> ProjectOopHirModel::iterativeOperatorKind(
    const ast::AstNode* syntax,
    bool address)
{
    if (syntax->kind() == ast::AstNodeKind::OverflowPolicyExpression &&
        callBinding(syntax) == nullptr && delegateBinding(syntax) == nullptr &&
        lambdaBinding(syntax) == nullptr)
        return IterativeExpressionKind::OverflowPolicy;
    if (syntax->kind() == ast::AstNodeKind::UnaryExpression &&
        syntax->tokenKind() != lexer::TokenKind::Amp &&
        syntax->tokenKind() != lexer::TokenKind::Star &&
        syntax->tokenKind() != lexer::TokenKind::PlusPlus &&
        syntax->tokenKind() != lexer::TokenKind::MinusMinus &&
        callBinding(syntax) == nullptr && delegateBinding(syntax) == nullptr &&
        lambdaBinding(syntax) == nullptr)
        return IterativeExpressionKind::BuiltinUnary;
    if (syntax->kind() == ast::AstNodeKind::TypeTestExpression ||
        syntax->kind() == ast::AstNodeKind::SafeCastExpression ||
        syntax->kind() == ast::AstNodeKind::CastExpression)
    {
        const MethodRuntimeTypeBinding* runtime = runtimeTypeBinding(syntax);
        if ((syntax->kind() == ast::AstNodeKind::TypeTestExpression ||
             syntax->kind() == ast::AstNodeKind::SafeCastExpression) && runtime == nullptr)
            return std::nullopt;
        if (syntax->kind() == ast::AstNodeKind::TypeTestExpression ||
            syntax->kind() == ast::AstNodeKind::SafeCastExpression ||
            (runtime != nullptr && runtime->kind == MethodRuntimeTypeOperationKind::CheckedCast))
            return IterativeExpressionKind::Conversion;
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
        return IterativeExpressionKind::BuiltinBinary;
    if (!address && syntax->kind() == ast::AstNodeKind::BinaryExpression &&
        syntax->first() != nullptr && syntax->second() != nullptr)
    {
        const MethodCoreBinding* core = coreBinding(syntax);
        if (core != nullptr &&
            (core->kind == MethodCoreOperationKind::StructEquals ||
             core->kind == MethodCoreOperationKind::StringConcat ||
             core->kind == MethodCoreOperationKind::StringEquals ||
             core->kind == MethodCoreOperationKind::PointerArithmetic))
            return IterativeExpressionKind::CoreBinary;
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
    return std::nullopt;
}

// Conserva las formas de control y las asignaciones con contrato especializado.
// Preserves control forms and assignments with specialized contracts.
std::optional<IterativeExpressionKind> ProjectOopHirModel::iterativeControlFlowKind(
    const ast::AstNode* syntax,
    bool address)
{
    if (syntax->kind() == ast::AstNodeKind::MatchExpression)
        return IterativeExpressionKind::Match;
    if (!address && syntax->kind() == ast::AstNodeKind::ConditionalExpression &&
        coreBinding(syntax) == nullptr && callBinding(syntax) == nullptr &&
        delegateBinding(syntax) == nullptr && lambdaBinding(syntax) == nullptr)
        return IterativeExpressionKind::Conditional;
    if (!address && syntax->kind() == ast::AstNodeKind::NullCoalescingExpression &&
        syntax->first() != nullptr && syntax->second() != nullptr)
        return IterativeExpressionKind::NullCoalescing;
    if (!address && syntax->kind() == ast::AstNodeKind::AssignmentExpression &&
        syntax->tokenKind() == lexer::TokenKind::QuestionQuestionEqual &&
        syntax->first() != nullptr && syntax->second() != nullptr &&
        expressionType(syntax->first()) != nullptr)
        return IterativeExpressionKind::NullCoalescingAssignment;
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
            return IterativeExpressionKind::EventSubscription;
    }
    return std::nullopt;
}

// Conserva la prioridad de setters y asignaciones directas.
// Preserves setter and direct-assignment priority.
std::optional<IterativeExpressionKind> ProjectOopHirModel::iterativeSetterAssignmentKind(
    const ast::AstNode* syntax,
    bool)
{
    if (syntax->kind() != ast::AstNodeKind::AssignmentExpression ||
        syntax->second() == nullptr)
        return std::nullopt;

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
    const NamespaceFunctionCallBinding* setter = namespaceFunctionCallBinding(syntax);
    if (setter != nullptr &&
        setter->resolution == MethodCallResolutionKind::Resolved &&
        setter->selectedFunctionOrdinal.has_value() && namespaceFunctionModel_ != nullptr &&
        *setter->selectedFunctionOrdinal < namespaceFunctionModel_->functions().size())
        return IterativeExpressionKind::NamespaceExtensionSetter;
    if (syntax->first() != nullptr &&
        syntax->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression)
    {
        const MethodNameBinding* binding = nameBinding(syntax->first());
        if (binding != nullptr && binding->candidateMemberOrdinals.size() == 1U &&
            binding->kind == MethodNameBindingKind::ExplicitObjectField)
            return IterativeExpressionKind::NullConditionalFieldAssignment;
    }
    if (syntax->first() != nullptr && syntax->tokenKind() == lexer::TokenKind::Equal &&
        syntax->first()->kind() == ast::AstNodeKind::NullConditionalIndexExpression)
    {
        const MethodCoreBinding* binding = coreBinding(syntax->first());
        if (binding != nullptr &&
            binding->kind == MethodCoreOperationKind::NullConditionalArrayElement)
            return IterativeExpressionKind::NullConditionalArrayAssignment;
    }
    if (syntax->first() == nullptr ||
        syntax->tokenKind() == lexer::TokenKind::QuestionQuestionEqual ||
        syntax->first()->kind() == ast::AstNodeKind::TupleExpression ||
        syntax->first()->kind() == ast::AstNodeKind::NullConditionalMemberExpression ||
        syntax->first()->kind() == ast::AstNodeKind::NullConditionalIndexExpression ||
        eventBinding(syntax) != nullptr || callBinding(syntax) != nullptr ||
        namespaceFunctionCallBinding(syntax) != nullptr ||
        (methodBodyModel_ != nullptr &&
         methodBodyModel_->deconstructionBinding(syntax) != nullptr))
        return std::nullopt;
    if (syntax->tokenKind() == lexer::TokenKind::Equal)
        return IterativeExpressionKind::DirectAssignment;
    const MethodExpressionTypeBinding* assignmentType = expressionType(syntax);
    if (syntax->tokenKind() == lexer::TokenKind::PlusEqual &&
        assignmentType != nullptr && assignmentType->typeKey == "builtin:string")
        return IterativeExpressionKind::StringCompoundAssignment;
    return IterativeExpressionKind::CompoundAssignment;
}

// Conserva propagacion, destruccion y stack allocation despues de assignments.
// Preserves propagation, destruction, and stack allocation after assignments.
std::optional<IterativeExpressionKind> ProjectOopHirModel::iterativeExitAndDestructionKind(
    const ast::AstNode* syntax,
    bool address)
{
    if (!address && syntax->kind() == ast::AstNodeKind::ResultPropagationExpression)
    {
        const MethodResultPropagationBinding* binding = resultPropagationBinding(syntax);
        const MethodStructuredExitBinding* exit = structuredExitBinding(syntax);
        if (binding != nullptr && binding->operandSyntax != nullptr && exit != nullptr &&
            exit->kind == MethodStructuredExitKind::Return)
            return IterativeExpressionKind::ResultPropagation;
    }
    if ((syntax->kind() == ast::AstNodeKind::DeleteExpression ||
         syntax->kind() == ast::AstNodeKind::DestroyExpression) &&
        syntax->first() != nullptr)
    {
        const MethodExpressionTypeBinding* operandType = expressionType(syntax->first());
        const bool validDeleteArray = syntax->kind() != ast::AstNodeKind::DeleteExpression ||
            !syntax->flag() ||
            (operandType != nullptr && ProjectTypeConversion::isNativeArray(operandType->typeKey));
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
    return std::nullopt;
}

// Conserva operaciones sobre storage, move y miembros core.
// Preserves storage, move, and core-member operations.
std::optional<IterativeExpressionKind> ProjectOopHirModel::iterativeStorageKind(
    const ast::AstNode* syntax,
    bool address)
{
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
        if (userOperator) return IterativeExpressionKind::UserUnaryOperator;
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
            return IterativeExpressionKind::StorageUnary;
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
         syntax->kind() == ast::AstNodeKind::MemberExpression) && syntax->first() != nullptr)
    {
        const MethodCoreBinding* core = coreBinding(syntax);
        if (core != nullptr &&
            (core->kind == MethodCoreOperationKind::ArrayLength ||
             core->kind == MethodCoreOperationKind::StringLength) && !address)
            return IterativeExpressionKind::CoreMember;
        if (core != nullptr && core->kind == MethodCoreOperationKind::IteratorCurrent)
        {
            const bool itemByReference = core->resultTypeKey.starts_with("ref:") ||
                core->resultTypeKey.starts_with("refmut:");
            if (!address || itemByReference)
                return IterativeExpressionKind::CoreMember;
        }
    }
    return std::nullopt;
}

// Conserva getters, extension properties y range slices en su orden original.
// Preserves getters, extension properties, and range slices in original order.
std::optional<IterativeExpressionKind> ProjectOopHirModel::iterativeMemberKind(
    const ast::AstNode* syntax,
    bool address)
{
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
            const bool nullConditional =
                syntax->kind() == ast::AstNodeKind::NullConditionalMemberExpression;
            const bool addressAllowed = !address ||
                (!nullConditional && accessor.isReferenceReturn);
            if (accessor.isPropertyAccessor && !accessor.isIndexerAccessor &&
                accessor.isGetter && addressAllowed && getter->receiverSyntax != nullptr &&
                getter->receiverSyntax->kind() != ast::AstNodeKind::BaseExpression &&
                !getter->receiverTypeKey.empty())
                return IterativeExpressionKind::PropertyGetter;
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
            return IterativeExpressionKind::NullConditionalField;
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
            return IterativeExpressionKind::RangeSlice;
    }
    return std::nullopt;
}

// Valida un indice compuesto sin una lambda local.
// Validates a composed index without a local lambda.
bool ProjectOopHirModel::validIterativeIndexSyntax(const ast::AstNode* index) noexcept
{
    return index != nullptr &&
        (index->kind() != ast::AstNodeKind::FromEndExpression ||
         index->first() != nullptr);
}

// Valida un indice raw sin cambiar la aceptacion previa.
// Validates a raw index without changing prior acceptance.
bool ProjectOopHirModel::validRawIterativeIndexSyntax(const ast::AstNode* index) noexcept
{
    return index != nullptr;
}

// Conserva la clasificacion de indices core y raw.
// Preserves core and raw index classification.
std::optional<IterativeExpressionKind> ProjectOopHirModel::iterativeIndexKind(
    const ast::AstNode* syntax,
    bool address)
{
    if (syntax->kind() != ast::AstNodeKind::IndexExpression &&
        syntax->kind() != ast::AstNodeKind::NullConditionalIndexExpression)
        return std::nullopt;

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
        ? syntax->children().front()
        : syntax->second();
    const bool validIndices = syntax->children().empty()
        ? validIterativeIndexSyntax(firstIndex)
        : std::all_of(
            syntax->children().begin(),
            syntax->children().end(),
            &ProjectOopHirModel::validIterativeIndexSyntax);
    if ((ordinaryElement || nullConditionalElement) && validAddress &&
        syntax->first() != nullptr && validIndices)
        return IterativeExpressionKind::CoreIndex;

    if (core == nullptr || core->kind != MethodCoreOperationKind::RawElement)
        return std::nullopt;
    const bool hasIndices = !syntax->children().empty() || syntax->second() != nullptr;
    const bool validRawIndices = syntax->children().empty()
        ? syntax->second() != nullptr
        : std::all_of(
            syntax->children().begin(),
            syntax->children().end(),
            &ProjectOopHirModel::validRawIterativeIndexSyntax);
    const MethodVariableArrayIndexBinding* variableArrayIndex = methodBodyModel_ != nullptr
        ? methodBodyModel_->variableArrayIndexBinding(syntax)
        : nullptr;
    const std::size_t indexCount = syntax->children().empty()
        ? (syntax->second() != nullptr ? 1U : 0U)
        : syntax->children().size();
    const bool validVariableRank = variableArrayIndex == nullptr ||
        indexCount == variableArrayIndex->extentLocalOrdinals.size();
    if (syntax->first() != nullptr && hasIndices && validRawIndices && validVariableRank)
        return IterativeExpressionKind::RawIndex;
    return std::nullopt;
}

// Conserva el getter de indexador y el field de instancia al final del clasificador.
// Preserves the indexer getter and instance field at the end of classification.
std::optional<IterativeExpressionKind> ProjectOopHirModel::iterativeUserIndexAndFieldKind(
    const ast::AstNode* syntax,
    bool address)
{
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
                return IterativeExpressionKind::UserIndexerGetter;
        }
    }
    if (syntax->kind() == ast::AstNodeKind::MemberExpression &&
        coreBinding(syntax) == nullptr && accessorGetterBinding(syntax) == nullptr &&
        callBinding(syntax) == nullptr && namespaceFunctionCallBinding(syntax) == nullptr)
    {
        const MethodLambdaValueBinding* lambdaValue = lambdaValueBinding(syntax);
        const bool activeLambdaValue = currentLambda_ != nullptr && lambdaValue != nullptr &&
            lambdaValue->lambdaOrdinal == currentLambda_->lambdaOrdinal;
        const MethodNameBinding* binding = nameBinding(syntax);
        if (!activeLambdaValue && binding != nullptr &&
            binding->kind == MethodNameBindingKind::ExplicitObjectField &&
            binding->receiverSyntax != nullptr && !binding->receiverTypeKey.empty() &&
            !binding->candidateMemberOrdinals.empty())
            return IterativeExpressionKind::InstanceField;
    }
    return std::nullopt;
}
}
