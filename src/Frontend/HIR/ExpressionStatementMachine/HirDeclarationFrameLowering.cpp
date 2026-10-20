#include "../HirLoweringSupport.h"
#include "../HirTuplePatternFrame.h"
#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <utility>

namespace nativesharp::frontend
{
using internal::DeconstructionDeclarationFrameStage;
using internal::DeconstructionDeclarationFrameState;
using internal::InlineAssemblyFrameState;
using internal::IterativeExpressionFrame;
using internal::VariableDeclarationFrameMode;
using internal::VariableDeclarationFrameState;

std::optional<bool> ProjectOopHirModel::prepareSetterReceiver(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] std::string& error)
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
}

bool ProjectOopHirModel::initializeVariableDeclarationFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] std::string& error)
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
    if (!initializeVariableCleanup(variable, frame.syntax, error)) return false;
    if (!initializeVariableMode(variable, frame.syntax, error)) return false;
    frame.variableDeclaration.emplace(std::move(variable));
    return true;
}

bool ProjectOopHirModel::initializeVariableCleanup(
    VariableDeclarationFrameState& variable,
    const ast::AstNode* syntax,
    [[maybe_unused]] std::string& error)
{
    const MethodLocalSymbol& local = *variable.local;
    if (local.isStatic ||
        (!local.isRaii && !local.isScope && !local.requiresValueDrop))
    {
        return true;
    }
    const MethodCleanupRegion* region = cleanupRegion(syntax);
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
    return true;
}

bool ProjectOopHirModel::initializeVariableMode(
    VariableDeclarationFrameState& variable,
    const ast::AstNode* syntax,
    [[maybe_unused]] std::string& error)
{
    const MethodLocalSymbol& local = *variable.local;
    if (!local.isVariableLengthArray)
    {
        variable.mode = syntax->first() != nullptr
            ? VariableDeclarationFrameMode::Initializer
            : VariableDeclarationFrameMode::NoChild;
        return true;
    }
    if (local.variableLengthArrayCountSyntax == nullptr ||
        local.variableLengthArrayElementTypeKey.empty())
    {
        error = "NativeSharp 2.0 VLA local lost its element type or runtime extent";
        return false;
    }
    variable.node->variableLengthArray = true;
    variable.node->variableLengthArrayRank = std::max<std::size_t>(
        1U, local.variableLengthArrayExtentSyntaxes.size());
    variable.node->elementTypeKey = local.variableLengthArrayElementTypeKey;
    if (local.variableLengthArrayExtentSyntaxes.size() <= 1U)
    {
        variable.mode = VariableDeclarationFrameMode::VlaSingle;
        return true;
    }
    if (local.variableLengthArrayExtentLocalOrdinals.size() !=
        local.variableLengthArrayExtentSyntaxes.size())
    {
        error = "multidimensional VLA lost its captured extent locals";
        return false;
    }
    variable.vlaBlock = makeNode(
        ProjectOopHirNodeKind::Block, syntax, "builtin:void");
    variable.mode = VariableDeclarationFrameMode::VlaExtents;
    return true;
}

bool ProjectOopHirModel::nextVariableDeclarationChild(
    [[maybe_unused]] const IterativeExpressionFrame& frame,
    const ast::AstNode*& syntax,
    bool& addressContext)
{
    const VariableDeclarationFrameState& variable = *frame.variableDeclaration;
    if (variable.mode == VariableDeclarationFrameMode::Initializer)
    {
        syntax = frame.syntax->first();
        addressContext = variable.local->isReference;
        return true;
    }
    if (variable.mode == VariableDeclarationFrameMode::VlaSingle)
    {
        syntax = variable.local->variableLengthArrayCountSyntax;
        addressContext = false;
        return true;
    }
    if (variable.mode == VariableDeclarationFrameMode::VlaExtents)
    {
        syntax = variable.local->variableLengthArrayExtentSyntaxes[
            variable.nextExtent];
        addressContext = false;
        return true;
    }
    return false;
}

void ProjectOopHirModel::completeVariableAllocation(
    [[maybe_unused]] IterativeExpressionFrame& frame)
{
    VariableDeclarationFrameState& variable = *frame.variableDeclaration;
    auto allocation = makeNode(
        ProjectOopHirNodeKind::StackAllocation,
        frame.syntax,
        variable.local->typeKey);
    allocation->elementTypeKey = variable.local->variableLengthArrayElementTypeKey;
    for (auto& count : variable.allocationCounts)
        allocation->children.push_back(std::move(count));
    variable.node->children.push_back(std::move(allocation));
    if (variable.vlaBlock != nullptr)
        variable.vlaBlock->children.push_back(std::move(variable.node));
    variable.mode = VariableDeclarationFrameMode::Ready;
}

bool ProjectOopHirModel::acceptVariableDeclarationChild(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode> child,
    [[maybe_unused]] std::string& error)
{
    VariableDeclarationFrameState& variable = *frame.variableDeclaration;
    if (variable.mode == VariableDeclarationFrameMode::Initializer)
        return acceptVariableInitializer(frame, std::move(child));
    if (variable.mode == VariableDeclarationFrameMode::VlaSingle)
    {
        variable.allocationCounts.push_back(std::move(child));
        completeVariableAllocation(frame);
        return true;
    }
    if (variable.mode == VariableDeclarationFrameMode::VlaExtents)
    {
        acceptVariableExtent(frame, std::move(child));
        return true;
    }
    error = "Stage 34 variable declaration frame accepted an unexpected child";
    return false;
}

bool ProjectOopHirModel::acceptVariableInitializer(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode> child)
{
    VariableDeclarationFrameState& variable = *frame.variableDeclaration;
    const MethodLocalSymbol& local = *variable.local;
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
    if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
    {
        child = wrapImplicitConversion(
            std::move(child), conversion, frame.syntax->first());
    }
    if (local.ownership == sema::OwnershipKind::Borrowed &&
        local.localBorrowBindingOrdinal.has_value() &&
        *local.localBorrowBindingOrdinal < methodBodyModel_->localBorrowBindings().size())
    {
        const MethodLocalBorrowBinding& borrow =
            methodBodyModel_->localBorrowBindings()[*local.localBorrowBindingOrdinal];
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

void ProjectOopHirModel::acceptVariableExtent(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode> child)
{
    VariableDeclarationFrameState& variable = *frame.variableDeclaration;
    const MethodLocalSymbol& local = *variable.local;
    const std::size_t dimension = variable.nextExtent;
    const std::size_t extentOrdinal =
        local.variableLengthArrayExtentLocalOrdinals[dimension];
    const std::string& extentType = methodBodyModel_->locals()[extentOrdinal].typeKey;
    auto extentDeclaration = makeNode(
        ProjectOopHirNodeKind::LocalDeclaration,
        local.variableLengthArrayExtentSyntaxes[dimension],
        extentType);
    extentDeclaration->stringValue = methodBodyModel_->locals()[extentOrdinal].name;
    extentDeclaration->debugDisplayName =
        local.name + ".Length[" + std::to_string(dimension) + "]";
    extentDeclaration->localOrdinal = extentOrdinal;
    extentDeclaration->children.push_back(std::move(child));
    variable.vlaBlock->children.push_back(std::move(extentDeclaration));
    auto extentValue = makeNode(
        ProjectOopHirNodeKind::LocalValue,
        local.variableLengthArrayExtentSyntaxes[dimension],
        extentType);
    extentValue->localOrdinal = extentOrdinal;
    variable.allocationCounts.push_back(std::move(extentValue));
    ++variable.nextExtent;
    if (variable.nextExtent == local.variableLengthArrayExtentSyntaxes.size())
        completeVariableAllocation(frame);
}

std::unique_ptr<ProjectOopHirNode>
ProjectOopHirModel::finishVariableDeclarationFrame([[maybe_unused]] IterativeExpressionFrame& frame)
{
    VariableDeclarationFrameState& variable = *frame.variableDeclaration;
    if (variable.mode == VariableDeclarationFrameMode::NoChild)
        variable.mode = VariableDeclarationFrameMode::Ready;
    return variable.vlaBlock != nullptr
        ? std::move(variable.vlaBlock)
        : std::move(variable.node);
}

bool ProjectOopHirModel::initializeDeconstructionDeclarationFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] std::string& error)
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
}

bool ProjectOopHirModel::nextDeconstructionDeclarationChild(
    [[maybe_unused]] const IterativeExpressionFrame& frame,
    const ast::AstNode*& syntax,
    bool& addressContext)
{
    const DeconstructionDeclarationFrameState& state =
        *frame.deconstructionDeclaration;
    if (state.stage != DeconstructionDeclarationFrameStage::Source) return false;
    syntax = frame.syntax->first();
    addressContext = state.binding->selectedMemberOrdinal.has_value() &&
        isStructValueType(state.binding->sourceTypeKey) &&
        !state.materializeReceiverTemporary;
    return true;
}

bool ProjectOopHirModel::acceptDeconstructionDeclarationChild(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode> child,
    [[maybe_unused]] std::string& error)
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
}

std::unique_ptr<ProjectOopHirNode>
ProjectOopHirModel::finishDeconstructionDeclarationFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] std::string& error)
{
    DeconstructionDeclarationFrameState& state = *frame.deconstructionDeclaration;
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
        ProjectTypeConversion::tupleElementTypeKeys(state.binding->tupleTypeKey);
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
    auto expandBindingTuple = std::bind_front(
        &ProjectOopHirModel::expandDeconstructionBindingTuple,
        this,
        std::ref(error));
    auto appendBindingLeaf = std::bind_front(
        &ProjectOopHirModel::appendDeconstructionBindingLeaf,
        this,
        std::ref(*block),
        std::ref(*deconstruct),
        std::ref(error));
    for (std::size_t index = 0; index < frame.syntax->children().size(); ++index)
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
}

bool ProjectOopHirModel::expandDeconstructionBindingTuple(
    [[maybe_unused]] std::string& error,
    const ast::AstNode* bindingSyntax,
    const std::string& typeKey,
    std::uint64_t byteOffset,
    std::vector<HirTuplePatternFrame>& children)
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
}

bool ProjectOopHirModel::appendDeconstructionBindingLeaf(
    ProjectOopHirNode& block,
    ProjectOopHirNode& deconstruct,
    [[maybe_unused]] std::string& error,
    const ast::AstNode* bindingSyntax,
    const std::string& typeKey,
    std::uint64_t byteOffset)
{
    if (bindingSyntax->kind() == ast::AstNodeKind::IdentifierExpression &&
        bindingSyntax->text() == "_") return true;
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
    block.children.push_back(std::move(declaration));

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
    deconstruct.children.push_back(std::move(element));
    return true;
}

void ProjectOopHirModel::initializeInlineAssemblyFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame)
{
    InlineAssemblyFrameState state;
    state.node = makeNode(
        ProjectOopHirNodeKind::InlineAssembly,
        frame.syntax,
        "builtin:void");
    state.node->stringValue = frame.syntax->text();
    frame.inlineAssembly.emplace(std::move(state));
}

bool ProjectOopHirModel::advanceInlineAssemblyFrame(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode*& syntax,
    bool& addressContext,
    [[maybe_unused]] std::string& error)
{
    InlineAssemblyFrameState& state = *frame.inlineAssembly;
    while (state.nextRecord < frame.syntax->children().size())
    {
        const ast::AstNode* record = frame.syntax->children()[state.nextRecord++];
        if (record == nullptr)
        {
            error = "integrated assembly contains a null contract record";
            return false;
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
        const bool input =
            record->kind() == ast::AstNodeKind::InlineAssemblyInputOperand;
        const bool output =
            record->kind() == ast::AstNodeKind::InlineAssemblyOutputOperand;
        if (!input && !output)
        {
            error = "integrated assembly contains an unknown contract record";
            return false;
        }
        if (record->first() == nullptr)
        {
            error = "integrated assembly operand lost its NativeSharp expression";
            return false;
        }
        state.pendingOperand = record;
        syntax = record->first();
        addressContext = output || (input && record->auxiliaryText() == "m");
        return true;
    }
    return false;
}

bool ProjectOopHirModel::acceptInlineAssemblyOperand(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode> child,
    [[maybe_unused]] std::string& error)
{
    InlineAssemblyFrameState& state = *frame.inlineAssembly;
    const ast::AstNode* record = state.pendingOperand;
    const MethodExpressionTypeBinding* operandType =
        record != nullptr ? expressionType(record->first()) : nullptr;
    if (operandType == nullptr || operandType->typeKey.empty())
    {
        error = "integrated assembly operand lost its resolved NativeSharp type";
        return false;
    }
    const bool input =
        record->kind() == ast::AstNodeKind::InlineAssemblyInputOperand;
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
}

std::unique_ptr<ProjectOopHirNode>
ProjectOopHirModel::finishInlineAssemblyFrame([[maybe_unused]] IterativeExpressionFrame& frame)
{
    return std::move(frame.inlineAssembly->node);
}
}
