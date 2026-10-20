#include "../HirLoweringSupport.h"
#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace nativesharp::frontend
{
using internal::BoundCallFrameStage;
using internal::BoundCallFrameState;
using internal::IterativeExpressionFrame;
using internal::IterativeExpressionKind;
using internal::OrdinaryCallFrameStage;
using internal::OrdinaryCallFrameState;
using internal::ReceiverFrameState;

bool ProjectOopHirModel::isBoundCallKind(
    IterativeExpressionKind kind) noexcept
{
    return kind == IterativeExpressionKind::UserUnaryOperator ||
        kind == IterativeExpressionKind::UserBinaryOperator ||
        kind == IterativeExpressionKind::UserConversion ||
        kind == IterativeExpressionKind::PropertyGetter ||
        kind == IterativeExpressionKind::UserIndexerGetter;
}

std::optional<bool> ProjectOopHirModel::initializeReceiverFrame(
    ReceiverFrameState& receiver,
    const ast::AstNode* receiverSyntax,
    std::string_view receiverTypeKey,
    std::string& error)
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
        receiver.materializeTemporary = isStructValueType(receiverTypeKey) &&
            !isNaturallyAddressable(receiverSyntax);
        return false;
    }
    receiver.lowered = lowerImplicitReceiver(
        receiverSyntax, receiverTypeKey, error);
    if (receiver.lowered == nullptr) return std::nullopt;
    return true;
}

void ProjectOopHirModel::acceptReceiverChild(
    ReceiverFrameState& receiver,
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
}

bool ProjectOopHirModel::initializeBoundCallFrame(
    IterativeExpressionFrame& frame,
    const MethodCallBinding* binding,
    std::vector<const ast::AstNode*> arguments,
    bool nullConditional,
    std::string& error)
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
        call.target.receiverTypeKey,
        error);
    if (!receiverReady.has_value()) return false;
    call.stage = *receiverReady
        ? (call.argumentSyntaxes.empty()
            ? BoundCallFrameStage::Ready
            : BoundCallFrameStage::Arguments)
        : BoundCallFrameStage::Receiver;
    frame.boundCall.emplace(std::move(call));
    return true;
}

bool ProjectOopHirModel::nextBoundCallChild(
    const BoundCallFrameState& call,
    const ast::AstNode*& syntax,
    bool& addressContext)
{
    if (call.stage == BoundCallFrameStage::Receiver)
    {
        syntax = call.receiver.syntax;
        addressContext = isStructValueType(call.receiver.typeKey) &&
            !call.receiver.materializeTemporary;
        return true;
    }
    if (call.stage != BoundCallFrameStage::Arguments ||
        call.nextArgument >= call.argumentSyntaxes.size())
    {
        return false;
    }
    const ClassMemberSymbol* target = callableSymbol(call.target.memberOrdinal);
    syntax = call.argumentSyntaxes[call.nextArgument];
    addressContext = target != nullptr && target->isUserOperator &&
        boundCallArgumentNeedsAddress(call.target.memberOrdinal, call.nextArgument);
    return true;
}

void ProjectOopHirModel::acceptBoundCallChild(
    BoundCallFrameState& call,
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
}

bool ProjectOopHirModel::validateOrdinaryCallMember(
    const OrdinaryCallFrameState& call,
    std::string& error) const
{
    if (memberModel_ == nullptr ||
        call.selectedMemberOrdinal >= memberModel_->members().size())
    {
        error = "NSC-LIMIT-020 selected call has no callable metadata";
        return false;
    }
    return true;
}

bool ProjectOopHirModel::initializeOrdinaryCallFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
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
    const bool selectedReceiverIsStruct = memberModel_ != nullptr &&
        call.selectedMemberOrdinal < memberModel_->members().size() &&
        memberModel_->members()[call.selectedMemberOrdinal].declaringTypeIsStruct;
    call.receiverNeedsAddress = !call.forwardedReceiver &&
        (selectedReceiverIsStruct || isStructValueType(call.receiver.typeKey));
    bool interfaceDispatch = binding->interfaceCall;
    redirectOrdinaryInterfaceCall(call, interfaceDispatch);
    initializeOrdinaryCallNode(call, frame, interfaceDispatch);
    if (!scheduleOrdinaryCallFrame(call, frame, error)) return false;
    frame.ordinaryCall.emplace(std::move(call));
    return true;
}

void ProjectOopHirModel::redirectOrdinaryInterfaceCall(
    OrdinaryCallFrameState& call,
    bool& interfaceDispatch) const
{
    const MethodCallBinding& binding = *call.binding;
    if (!interfaceDispatch || binding.forwardedReceiver ||
        interfaceModel_ == nullptr || memberModel_ == nullptr ||
        !binding.receiverTypeOrdinal.has_value() || call.receiver.syntax == nullptr ||
        (call.receiver.syntax->kind() != ast::AstNodeKind::CastExpression &&
         call.receiver.syntax->kind() != ast::AstNodeKind::SafeCastExpression) ||
        call.receiver.syntax->first() == nullptr ||
        (call.receiver.syntax->first()->kind() != ast::AstNodeKind::NewExpression &&
         call.receiver.syntax->first()->kind() !=
            ast::AstNodeKind::ObjectCreationExpression))
    {
        return;
    }
    const MethodExpressionTypeBinding* concreteType =
        expressionType(call.receiver.syntax->first());
    const std::optional<std::size_t> concreteTypeOrdinal = concreteType != nullptr
        ? interfaceModel_->typeOrdinalForTypeKey(concreteType->typeKey)
        : std::nullopt;
    const ClassInterfaceTablePlan* plan = concreteTypeOrdinal.has_value()
        ? interfaceModel_->tablePlan(
            *concreteTypeOrdinal, *binding.receiverTypeOrdinal)
        : nullptr;
    if (plan == nullptr) return;
    const auto contract = std::find(
        plan->interfaceMethodMemberOrdinals.begin(),
        plan->interfaceMethodMemberOrdinals.end(),
        call.selectedMemberOrdinal);
    if (contract == plan->interfaceMethodMemberOrdinals.end()) return;
    const std::size_t slot = static_cast<std::size_t>(
        contract - plan->interfaceMethodMemberOrdinals.begin());
    if (slot >= plan->implementationMemberOrdinals.size() ||
        !plan->implementationMemberOrdinals[slot].has_value()) return;
    call.selectedMemberOrdinal = *plan->implementationMemberOrdinals[slot];
    call.receiver.syntax = call.receiver.syntax->first();
    call.receiver.typeKey = concreteType->typeKey;
    interfaceDispatch = false;
}

void ProjectOopHirModel::initializeOrdinaryCallNode(
    OrdinaryCallFrameState& call,
    IterativeExpressionFrame& frame,
    bool interfaceDispatch)
{
    const MethodCallBinding& binding = *call.binding;
    call.node = makeNode(
        binding.staticCall ? ProjectOopHirNodeKind::StaticCall
                           : ProjectOopHirNodeKind::InstanceCall,
        frame.syntax,
        binding.resultTypeKey);
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
    call.node->baseReceiver = binding.baseReceiver;
    call.node->interfaceDispatch = interfaceDispatch;
    if (interfaceDispatch)
        call.node->targetTypeOrdinal = binding.receiverTypeOrdinal;
}

bool ProjectOopHirModel::scheduleOrdinaryCallFrame(
    OrdinaryCallFrameState& call,
    IterativeExpressionFrame& frame,
    std::string& error)
{
    if (call.binding->forwardedReceiver)
    {
        call.stage = OrdinaryCallFrameStage::Receiver;
        return true;
    }
    if (call.binding->staticCall)
    {
        if (!validateOrdinaryCallMember(call, error)) return false;
        call.stage = frame.syntax->children().empty()
            ? OrdinaryCallFrameStage::Ready
            : OrdinaryCallFrameStage::Arguments;
        return true;
    }
    const std::optional<bool> receiverReady = initializeReceiverFrame(
        call.receiver, call.receiver.syntax, call.receiver.typeKey, error);
    if (!receiverReady.has_value()) return false;
    if (!*receiverReady)
    {
        call.receiver.materializeTemporary = call.receiverNeedsAddress &&
            !isNaturallyAddressable(call.receiver.syntax);
        call.stage = OrdinaryCallFrameStage::Receiver;
        return true;
    }
    call.node->children.push_back(std::move(call.receiver.lowered));
    if (!validateOrdinaryCallMember(call, error)) return false;
    call.stage = frame.syntax->children().empty()
        ? OrdinaryCallFrameStage::Ready
        : OrdinaryCallFrameStage::Arguments;
    return true;
}

bool ProjectOopHirModel::nextOrdinaryCallChild(
    const OrdinaryCallFrameState& call,
    const ast::AstNode*& syntax,
    bool& addressContext) const
{
    if (call.stage == OrdinaryCallFrameStage::Receiver)
    {
        syntax = call.receiver.syntax;
        addressContext = !call.forwardedReceiver &&
            call.receiverNeedsAddress &&
            !call.receiver.materializeTemporary;
        return true;
    }
    if (call.stage != OrdinaryCallFrameStage::Arguments ||
        call.nextArgument >= call.node->syntax->children().size())
    {
        return false;
    }
    syntax = call.node->syntax->children()[call.nextArgument];
    addressContext = false;
    return true;
}

bool ProjectOopHirModel::acceptOrdinaryCallChild(
    OrdinaryCallFrameState& call,
    std::unique_ptr<ProjectOopHirNode> child,
    std::string& error)
{
    if (call.stage == OrdinaryCallFrameStage::Receiver)
    {
        return acceptOrdinaryCallReceiverChild(call, std::move(child), error);
    }
    return acceptOrdinaryCallArgumentChild(call, std::move(child), error);
}

bool ProjectOopHirModel::acceptOrdinaryCallReceiverChild(
    OrdinaryCallFrameState& call,
    std::unique_ptr<ProjectOopHirNode> child,
    std::string& error)
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
    if (!validateOrdinaryCallMember(call, error)) return false;
    call.stage = call.node->syntax->children().empty()
        ? OrdinaryCallFrameStage::Ready
        : OrdinaryCallFrameStage::Arguments;
    return true;
}

bool ProjectOopHirModel::acceptOrdinaryCallArgumentChild(
    OrdinaryCallFrameState& call,
    std::unique_ptr<ProjectOopHirNode> child,
    std::string& error)
{
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
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::finishOrdinaryCall(
    IterativeExpressionFrame& frame,
    std::string& error)
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
}

bool ProjectOopHirModel::initializeNamespaceCallFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
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
    internal::NamespaceCallFrameState call;
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
        call.stage = internal::NamespaceCallFrameStage::Receiver;
    }
    else
    {
        call.stage = frame.syntax->children().empty()
            ? internal::NamespaceCallFrameStage::Ready
            : internal::NamespaceCallFrameStage::Arguments;
    }
    frame.namespaceCall.emplace(std::move(call));
    return true;
}

bool ProjectOopHirModel::nextNamespaceCallChild(
    const internal::NamespaceCallFrameState& call,
    const ast::AstNode*& syntax,
    bool& addressContext)
{
    if (call.stage == internal::NamespaceCallFrameStage::Receiver)
    {
        syntax = call.binding->receiverSyntax;
        addressContext = call.receiverNeedsAddress;
        return true;
    }
    if (call.stage != internal::NamespaceCallFrameStage::Arguments ||
        call.nextArgument >= call.node->syntax->children().size())
    {
        return false;
    }
    syntax = call.node->syntax->children()[call.nextArgument];
    addressContext = false;
    return true;
}

bool ProjectOopHirModel::acceptNamespaceCallChild(
    internal::NamespaceCallFrameState& call,
    std::unique_ptr<ProjectOopHirNode> child,
    std::string& error)
{
    if (call.stage == internal::NamespaceCallFrameStage::Receiver)
        return acceptNamespaceCallReceiverChild(call, std::move(child));
    return acceptNamespaceCallArgumentChild(call, std::move(child), error);
}

bool ProjectOopHirModel::acceptNamespaceCallReceiverChild(
    internal::NamespaceCallFrameState& call,
    std::unique_ptr<ProjectOopHirNode> child)
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
    call.node->argumentParameterOrdinals.push_back(call.receiverParameterOrdinal);
    call.node->argumentTypeKeys.push_back(
        call.target->parameters[call.receiverParameterOrdinal].typeKey);
    call.stage = call.node->syntax->children().empty()
        ? internal::NamespaceCallFrameStage::Ready
        : internal::NamespaceCallFrameStage::Arguments;
    return true;
}

bool ProjectOopHirModel::acceptNamespaceCallArgumentChild(
    internal::NamespaceCallFrameState& call,
    std::unique_ptr<ProjectOopHirNode> child,
    std::string& error)
{
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
        call.stage = internal::NamespaceCallFrameStage::Ready;
    return true;
}

bool ProjectOopHirModel::initializeEventInvokeFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
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
    internal::EventInvokeFrameState call;
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
            ? internal::EventInvokeFrameStage::Ready
            : internal::EventInvokeFrameStage::Arguments;
    }
    else
    {
        const std::optional<bool> receiverReady = initializeReceiverFrame(
            call.receiver,
            binding->receiverSyntax,
            binding->receiverTypeKey,
            error);
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
                ? internal::EventInvokeFrameStage::Ready
                : internal::EventInvokeFrameStage::Arguments;
        }
        else
        {
            call.stage = internal::EventInvokeFrameStage::Receiver;
        }
    }
    frame.eventInvoke.emplace(std::move(call));
    return true;
}

bool ProjectOopHirModel::nextEventInvokeChild(
    const internal::EventInvokeFrameState& call,
    const ast::AstNode*& syntax,
    bool& addressContext)
{
    if (call.stage == internal::EventInvokeFrameStage::Receiver)
    {
        syntax = call.receiver.syntax;
        addressContext = isStructValueType(call.receiver.typeKey) &&
            !call.receiver.materializeTemporary;
        return true;
    }
    if (call.stage != internal::EventInvokeFrameStage::Arguments ||
        call.nextArgument >= call.node->syntax->children().size())
    {
        return false;
    }
    syntax = call.node->syntax->children()[call.nextArgument];
    addressContext = false;
    return true;
}

bool ProjectOopHirModel::acceptEventInvokeChild(
    IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode> child,
    std::string& error)
{
    internal::EventInvokeFrameState& call = *frame.eventInvoke;
    if (call.stage == internal::EventInvokeFrameStage::Receiver)
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
            ? internal::EventInvokeFrameStage::Ready
            : internal::EventInvokeFrameStage::Arguments;
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
        call.stage = internal::EventInvokeFrameStage::Ready;
    return true;
}

bool ProjectOopHirModel::initializeDelegateInvokeFrame(
    IterativeExpressionFrame& frame,
    std::string& error)
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
    internal::DelegateInvokeFrameState call;
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
        ? internal::DelegateInvokeFrameStage::Callee
        : frame.syntax->children().empty()
            ? internal::DelegateInvokeFrameStage::Ready
            : internal::DelegateInvokeFrameStage::Arguments;
    frame.delegateInvoke.emplace(std::move(call));
    return true;
}

bool ProjectOopHirModel::nextDelegateInvokeChild(
    const IterativeExpressionFrame& frame,
    const ast::AstNode*& syntax)
{
    const internal::DelegateInvokeFrameState& call = *frame.delegateInvoke;
    if (call.stage == internal::DelegateInvokeFrameStage::Callee)
    {
        syntax = frame.syntax->first();
        return true;
    }
    if (call.stage != internal::DelegateInvokeFrameStage::Arguments ||
        call.nextArgument >= frame.syntax->children().size())
    {
        return false;
    }
    syntax = frame.syntax->children()[call.nextArgument];
    return true;
}

void ProjectOopHirModel::acceptDelegateInvokeChild(
    IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode> child)
{
    internal::DelegateInvokeFrameState& call = *frame.delegateInvoke;
    if (call.stage == internal::DelegateInvokeFrameStage::Callee)
    {
        call.node->children.push_back(std::move(child));
        call.stage = frame.syntax->children().empty()
            ? internal::DelegateInvokeFrameStage::Ready
            : internal::DelegateInvokeFrameStage::Arguments;
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
        call.stage = internal::DelegateInvokeFrameStage::Ready;
}
}
