#include "../HirLoweringSupport.h"
#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <algorithm>
#include <ranges>
#include <utility>

namespace nativesharp::frontend
{
using internal::AllocatorConstructionFrameStage;
using internal::AllocatorConstructionFrameState;
using internal::IterativeExpressionFrame;
using internal::OrdinaryConstructionFrameStage;
using internal::OrdinaryConstructionFrameState;

bool ProjectOopHirModel::prepareOrdinaryConstructionTail(
    OrdinaryConstructionFrameState& construction,
    [[maybe_unused]] std::string& error)
{
    if (!appendOrdinaryConstructionDefaults(construction, error)) return false;
    if (!appendEmptyOrdinaryConstructionPack(construction, error)) return false;
    construction.node->argumentCount =
        construction.node->argumentParameterOrdinals.size();
    collectImplicitOrdinaryConstructionFields(construction);
    construction.stage = construction.fieldMemberOrdinals.empty()
        ? OrdinaryConstructionFrameStage::Ready
        : OrdinaryConstructionFrameStage::FieldInitializers;
    return true;
}

bool ProjectOopHirModel::appendOrdinaryConstructionDefaults(
    OrdinaryConstructionFrameState& construction,
    [[maybe_unused]] std::string& error)
{
    const ObjectConstructionBinding& binding = *construction.binding;
    if (binding.selectedDefaultArguments.empty()) return true;
    if (!binding.selectedConstructorMemberOrdinal.has_value() ||
        memberModel_ == nullptr ||
        *binding.selectedConstructorMemberOrdinal >= memberModel_->members().size())
    {
        error = "NSC-LIMIT-020 selected construction has no callable metadata";
        return false;
    }
    const ClassMemberSymbol& selectedConstructor =
        memberModel_->members()[*binding.selectedConstructorMemberOrdinal];
    for (std::size_t index = 0;
        index < binding.selectedDefaultArguments.size(); ++index)
    {
        if (index >= binding.selectedDefaultArgumentParameterOrdinals.size())
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
    return true;
}

bool ProjectOopHirModel::appendEmptyOrdinaryConstructionPack(
    OrdinaryConstructionFrameState& construction,
    [[maybe_unused]] std::string& error)
{
    const ObjectConstructionBinding& binding = *construction.binding;
    if (!binding.selectedSynthesizesEmptyParameterPack) return true;
    if (!binding.selectedConstructorMemberOrdinal.has_value() ||
        memberModel_ == nullptr ||
        *binding.selectedConstructorMemberOrdinal >= memberModel_->members().size() ||
        !binding.selectedParameterPackOrdinal.has_value())
    {
        error = "T01-F02 empty construction parameter pack has no constructor metadata";
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
        error = "T01-F02 empty construction parameter pack lost its element type";
        return false;
    }
    auto pack = makeNode(
        ProjectOopHirNodeKind::ParameterPack,
        construction.node->syntax,
        constructor.parameters[parameterIndex].typeKey);
    pack->elementTypeKey = elementType;
    construction.node->children.push_back(std::move(pack));
    construction.node->argumentParameterOrdinals.push_back(parameterIndex);
    return true;
}

void ProjectOopHirModel::collectImplicitOrdinaryConstructionFields(
    OrdinaryConstructionFrameState& construction)
{
    const ObjectConstructionBinding& binding = *construction.binding;
    if (!binding.selectedImplicitDefault || memberModel_ == nullptr) return;
    for (const std::size_t typeOrdinal : implicitInitializerTypes(
             binding.targetTypeOrdinal,
             binding.implicitDefaultBaseConstructorMemberOrdinal))
    {
        for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
        {
            if (memberOrdinal >= memberModel_->members().size()) continue;
            const ClassMemberSymbol& field = memberModel_->members()[memberOrdinal];
            if (field.kind == ClassMemberKind::Field && !field.isStatic &&
                field.syntax != nullptr && field.syntax->first() != nullptr)
            {
                construction.fieldMemberOrdinals.push_back(memberOrdinal);
            }
        }
    }
}

bool ProjectOopHirModel::initializeOrdinaryConstructionFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] std::string& error)
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
        construction.stage = OrdinaryConstructionFrameStage::Storage;
    else if (!frame.syntax->children().empty())
        construction.stage = OrdinaryConstructionFrameStage::Arguments;
    else if (!prepareOrdinaryConstructionTail(construction, error))
        return false;
    frame.ordinaryConstruction.emplace(std::move(construction));
    return true;
}

bool ProjectOopHirModel::nextOrdinaryConstructionChild(
    const OrdinaryConstructionFrameState& construction,
    const ast::AstNode*& syntax) const
{
    if (construction.stage == OrdinaryConstructionFrameStage::Storage)
    {
        syntax = construction.node->syntax->first();
        return true;
    }
    if (construction.stage == OrdinaryConstructionFrameStage::Arguments &&
        construction.nextArgument < construction.node->syntax->children().size())
    {
        syntax = construction.node->syntax->children()[construction.nextArgument];
        return true;
    }
    if (construction.stage != OrdinaryConstructionFrameStage::FieldInitializers ||
        construction.nextFieldInitializer >= construction.fieldMemberOrdinals.size() ||
        memberModel_ == nullptr)
    {
        return false;
    }
    const std::size_t memberOrdinal =
        construction.fieldMemberOrdinals[construction.nextFieldInitializer];
    if (memberOrdinal >= memberModel_->members().size()) return false;
    const ClassMemberSymbol& field = memberModel_->members()[memberOrdinal];
    syntax = field.syntax->first();
    return true;
}

bool ProjectOopHirModel::acceptOrdinaryConstructionChild(
    OrdinaryConstructionFrameState& construction,
    std::unique_ptr<ProjectOopHirNode> child,
    [[maybe_unused]] std::string& error)
{
    if (construction.stage == OrdinaryConstructionFrameStage::Storage)
    {
        construction.node->children.push_back(std::move(child));
        if (!construction.node->syntax->children().empty())
        {
            construction.stage = OrdinaryConstructionFrameStage::Arguments;
            return true;
        }
        return prepareOrdinaryConstructionTail(construction, error);
    }
    if (construction.stage == OrdinaryConstructionFrameStage::Arguments)
    {
        return acceptOrdinaryConstructionArgument(
            construction, std::move(child), error);
    }
    if (construction.stage == OrdinaryConstructionFrameStage::FieldInitializers)
    {
        return acceptOrdinaryConstructionFieldInitializer(
            construction, std::move(child), error);
    }
    error = "Stage 34 ordinary-construction frame accepted an unexpected child";
    return false;
}

bool ProjectOopHirModel::acceptOrdinaryConstructionArgument(
    OrdinaryConstructionFrameState& construction,
    std::unique_ptr<ProjectOopHirNode> child,
    [[maybe_unused]] std::string& error)
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
    if (index >= construction.binding->selectedArgumentParameterOrdinals.size())
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
        if (!appendOrdinaryConstructionPackElement(
                construction,
                std::move(child),
                argumentSyntax,
                parameterIndex,
                error))
        {
            return false;
        }
    }
    else
    {
        if (std::ranges::find(
                construction.binding->selectedFullExpressionBorrowArgumentOrdinals,
                index) !=
            construction.binding->selectedFullExpressionBorrowArgumentOrdinals.end())
        {
            child = wrapFreshOwnedTemporaryBorrow(
                std::move(child), argumentSyntax, construction.hasOwnedTemporary);
        }
        construction.node->children.push_back(std::move(child));
        construction.node->argumentParameterOrdinals.push_back(parameterIndex);
    }
    if (construction.nextArgument == construction.node->syntax->children().size())
        return prepareOrdinaryConstructionTail(construction, error);
    return true;
}

bool ProjectOopHirModel::appendOrdinaryConstructionPackElement(
    OrdinaryConstructionFrameState& construction,
    std::unique_ptr<ProjectOopHirNode> child,
    const ast::AstNode* argumentSyntax,
    std::size_t parameterIndex,
    [[maybe_unused]] std::string& error)
{
    if (!construction.binding->selectedConstructorMemberOrdinal.has_value() ||
        memberModel_ == nullptr ||
        *construction.binding->selectedConstructorMemberOrdinal >=
            memberModel_->members().size())
    {
        error = "T01-F02 construction parameter pack has no constructor metadata";
        return false;
    }
    const ClassMemberSymbol& constructor = memberModel_->members()[
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
        error = "T01-F02 construction parameter pack lost its element type";
        return false;
    }
    if (!construction.expandedPackChildIndex.has_value())
    {
        auto pack = makeNode(
            ProjectOopHirNodeKind::ParameterPack,
            argumentSyntax,
            constructor.parameters[parameterIndex].typeKey);
        pack->elementTypeKey = elementType;
        construction.expandedPackChildIndex = construction.node->children.size();
        construction.node->children.push_back(std::move(pack));
        construction.node->argumentParameterOrdinals.push_back(parameterIndex);
    }
    construction.node->children[*construction.expandedPackChildIndex]
        ->children.push_back(std::move(child));
    return true;
}

bool ProjectOopHirModel::acceptOrdinaryConstructionFieldInitializer(
    OrdinaryConstructionFrameState& construction,
    std::unique_ptr<ProjectOopHirNode> child,
    [[maybe_unused]] std::string& error)
{
    if (memberModel_ == nullptr || inheritanceModel_ == nullptr ||
        construction.nextFieldInitializer >= construction.fieldMemberOrdinals.size())
    {
        error = "Stage 37 cannot lower an invalid instance field initializer";
        return false;
    }
    const std::size_t fieldMemberOrdinal =
        construction.fieldMemberOrdinals[construction.nextFieldInitializer++];
    if (fieldMemberOrdinal >= memberModel_->members().size())
    {
        error = "Stage 37 cannot lower an invalid instance field initializer";
        return false;
    }
    const ClassMemberSymbol& field = memberModel_->members()[fieldMemberOrdinal];
    if (field.kind != ClassMemberKind::Field || field.isStatic ||
        field.syntax == nullptr || field.syntax->first() == nullptr)
    {
        error = "Stage 37 instance initializer helper received a non-instance field";
        return false;
    }
    const MethodExpressionTypeBinding* sourceType =
        expressionType(field.syntax->first());
    const ProjectImplicitConversion conversion = classifyImplicit(
        child->typeKey,
        field.declaredTypeKey,
        sourceType != nullptr ? sourceType->integerConstant : std::nullopt);
    if (!conversion.exists())
    {
        error = "Stage 37 field initializer lost its validated implicit conversion";
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
        inheritanceModel_->typeKeyForOrdinal(field.declaringTypeOrdinal)));
    auto assignment = makeNode(
        ProjectOopHirNodeKind::Assignment,
        field.syntax,
        field.declaredTypeKey);
    assignment->operation = lexer::TokenKind::Equal;
    assignment->children.push_back(std::move(target));
    assignment->children.push_back(std::move(child));
    construction.node->children.push_back(std::move(assignment));
    if (construction.nextFieldInitializer == construction.fieldMemberOrdinals.size())
        construction.stage = OrdinaryConstructionFrameStage::Ready;
    return true;
}

std::unique_ptr<ProjectOopHirNode>
ProjectOopHirModel::finishOrdinaryConstructionFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame)
{
    OrdinaryConstructionFrameState& construction = *frame.ordinaryConstruction;
    return wrapFullExpressionCleanup(
        std::move(construction.node),
        frame.syntax,
        construction.hasOwnedTemporary);
}

bool ProjectOopHirModel::initializeAllocatorConstructionFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] std::string& error)
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
    const bool trailingConstruction = frame.syntax->auxiliaryText() == "trailing";
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
}

bool ProjectOopHirModel::nextAllocatorConstructionChild(
    const AllocatorConstructionFrameState& construction,
    const ast::AstNode*& syntax,
    bool& addressContext)
{
    if (construction.stage == AllocatorConstructionFrameStage::Receiver)
    {
        syntax = construction.receiverSyntax;
        addressContext = construction.allocatorIsStruct;
        return true;
    }
    if (construction.stage != AllocatorConstructionFrameStage::Arguments ||
        construction.nextArgument >= construction.argumentSyntaxes.size())
    {
        return false;
    }
    syntax = construction.argumentSyntaxes[construction.nextArgument];
    addressContext = false;
    return true;
}

bool ProjectOopHirModel::acceptAllocatorConstructionChild(
    AllocatorConstructionFrameState& construction,
    std::unique_ptr<ProjectOopHirNode> child,
    [[maybe_unused]] std::string& error)
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
    if (index >= construction.binding->selectedArgumentParameterOrdinals.size())
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
}

std::unique_ptr<ProjectOopHirNode>
ProjectOopHirModel::finishAllocatorConstructionFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] std::string& error)
{
    AllocatorConstructionFrameState& construction = *frame.allocatorConstruction;
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
}
}
