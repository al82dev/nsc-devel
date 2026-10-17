#include "HirLoweringSupport.h"

#include <algorithm>
#include <utility>

namespace nativesharp::frontend
{
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerBoundCall(
    const ast::AstNode* expression,
    std::size_t memberOrdinal,
    bool addressContext,
    std::string& error,
    const MethodExpressionTypeBinding* typeBinding,
    const MethodCallBinding* binding,
    const std::vector<const ast::AstNode*>& argumentSyntaxes,
    std::unique_ptr<ProjectOopHirNode> preloweredLast,
    bool nullConditional)
{
    if (binding == nullptr || binding->resolution != MethodCallResolutionKind::Resolved ||
        !binding->selectedMemberOrdinal.has_value())
    {
        error = "Stage 38 requires a resolved synthetic call binding";
        return nullptr;
    }
    std::size_t selectedMemberOrdinal = *binding->selectedMemberOrdinal;
    const ast::AstNode* receiverSyntax = binding->receiverSyntax;
    std::string_view receiverTypeKey = binding->receiverTypeKey;
    bool interfaceDispatch = binding->interfaceCall;

    // An interface conversion whose operand is a freshly constructed concrete object has one
    // exact dynamic type. Resolve its interface slot now so the ordinary direct-call path can
    // honor the implementation's inlining policy. This is deliberately narrow: locals,
    // parameters, fields, and arbitrary expressions still use runtime interface dispatch.
    if (interfaceDispatch && interfaceModel_ != nullptr && memberModel_ != nullptr &&
        binding->receiverTypeOrdinal.has_value() && receiverSyntax != nullptr &&
        (receiverSyntax->kind() == ast::AstNodeKind::CastExpression ||
         receiverSyntax->kind() == ast::AstNodeKind::SafeCastExpression) &&
        receiverSyntax->first() != nullptr &&
        (receiverSyntax->first()->kind() == ast::AstNodeKind::NewExpression ||
         receiverSyntax->first()->kind() == ast::AstNodeKind::ObjectCreationExpression))
    {
        const MethodExpressionTypeBinding* concreteType = expressionType(receiverSyntax->first());
        const std::optional<std::size_t> concreteTypeOrdinal = concreteType != nullptr
            ? interfaceModel_->typeOrdinalForTypeKey(concreteType->typeKey)
            : std::nullopt;
        const ClassInterfaceTablePlan* plan = concreteTypeOrdinal.has_value()
            ? interfaceModel_->tablePlan(*concreteTypeOrdinal, *binding->receiverTypeOrdinal)
            : nullptr;
        if (plan != nullptr)
        {
            const auto contract = std::find(
                plan->interfaceMethodMemberOrdinals.begin(),
                plan->interfaceMethodMemberOrdinals.end(),
                selectedMemberOrdinal);
            if (contract != plan->interfaceMethodMemberOrdinals.end())
            {
                const std::size_t slot = static_cast<std::size_t>(
                    contract - plan->interfaceMethodMemberOrdinals.begin());
                if (slot < plan->implementationMemberOrdinals.size() &&
                    plan->implementationMemberOrdinals[slot].has_value())
                {
                    selectedMemberOrdinal = *plan->implementationMemberOrdinals[slot];
                    receiverSyntax = receiverSyntax->first();
                    receiverTypeKey = concreteType->typeKey;
                    interfaceDispatch = false;
                }
            }
        }
    }
    auto node = makeNode(
        binding->staticCall ? ProjectOopHirNodeKind::StaticCall : ProjectOopHirNodeKind::InstanceCall,
        expression,
        binding->resultTypeKey);
    // NS304-T08-F04: a namespace-callable adapter (extension operator) targets the namespace
    // function, not a class member (memberOrdinal >= members().size()).
    if (binding->staticCall && memberModel_ != nullptr &&
        selectedMemberOrdinal >= memberModel_->members().size())
    {
        node->namespaceFunctionOrdinal =
            selectedMemberOrdinal - memberModel_->members().size();
        node->memberOrdinal.reset();
    }
    else
    {
        node->memberOrdinal = selectedMemberOrdinal;
    }
    node->baseReceiver = binding->baseReceiver;
    node->interfaceDispatch = interfaceDispatch;
    node->nullConditionalCall = nullConditional;
    node->booleanValue = typeBinding != nullptr &&
        typeBinding->flowActiveVariantCase.has_value();
    if (interfaceDispatch) node->targetTypeOrdinal = binding->receiverTypeOrdinal;
    if (!binding->staticCall)
    {
        auto receiver = lowerReceiver(
            receiverSyntax, receiverTypeKey, memberOrdinal, error);
        if (receiver == nullptr) return nullptr;
        node->children.push_back(std::move(receiver));
    }
    bool hasOwnedTemporary = false;
    for (std::size_t index = 0; index < argumentSyntaxes.size(); ++index)
    {
        const ast::AstNode* argumentSyntax = argumentSyntaxes[index];
        const ClassMemberSymbol* operatorMember = callableSymbol(selectedMemberOrdinal);
        const bool implicitOperatorReference = operatorMember != nullptr &&
            operatorMember->isUserOperator &&
            index < operatorMember->parameters.size() &&
            operatorMember->parameters[index].passKind != ParameterPassKind::Value;
        auto argument = preloweredLast != nullptr && index + 1 == argumentSyntaxes.size()
            ? std::move(preloweredLast)
            : lowerExpression(
                argumentSyntax, memberOrdinal, implicitOperatorReference, error);
        if (argument == nullptr) return nullptr;
        if (index < binding->selectedConversions.size() &&
            binding->selectedConversions[index].kind != ProjectConversionKind::Identity)
        {
            argument = wrapImplicitConversion(
                std::move(argument), binding->selectedConversions[index], argumentSyntax);
        }
        argument = wrapCallArgumentBorrow(
            std::move(argument), argumentSyntax, selectedMemberOrdinal, index,
            hasOwnedTemporary);
        node->children.push_back(std::move(argument));
    }
    const ClassMemberSymbol* selectedMemberPtr = callableSymbol(selectedMemberOrdinal);
    if (selectedMemberPtr == nullptr)
    {
        error = "NSC-LIMIT-020 selected call has no callable metadata";
        return nullptr;
    }
    const ClassMemberSymbol& selectedMember = *selectedMemberPtr;
    for (std::size_t index = 0; index < binding->selectedDefaultArguments.size(); ++index)
    {
        const std::size_t parameterIndex = argumentSyntaxes.size() + index;
        if (parameterIndex >= selectedMember.parameters.size())
        {
            error = "NSC-LIMIT-020 selected default argument has no matching parameter";
            return nullptr;
        }
        auto argument = lowerDefaultArgument(
            binding->selectedDefaultArguments[index],
            selectedMember.parameters[parameterIndex].typeKey,
            error);
        if (argument == nullptr) return nullptr;
        node->children.push_back(std::move(argument));
    }
    node->referenceResult = selectedMember.isReferenceReturn;
    auto loweredCall = wrapFullExpressionCleanup(std::move(node), expression, hasOwnedTemporary);
    if (!selectedMember.isReferenceReturn || addressContext) return loweredCall;
    auto load = makeNode(ProjectOopHirNodeKind::PointerLoad, expression, selectedMember.declaredTypeKey);
    load->children.push_back(std::move(loweredCall));
    return load;
}
}
