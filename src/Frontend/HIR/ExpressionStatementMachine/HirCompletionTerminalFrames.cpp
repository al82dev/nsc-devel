#include "../HirLoweringSupport.h"
#include "HirExpressionStatementMachineTypes.h"
#include "CppVolatileAliasAlignment.h"

#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/CppImportedEnumNumeric.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <cstdint>
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
ProjectOopHirModel::completeScheduledTerminalFrames(
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
    const IterativeExpressionKind kind = frames.back().kind;
    if (kind == IterativeExpressionKind::ResultPropagation)
    {
        return completeScheduledResultPropagationFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    }
    if (kind == IterativeExpressionKind::Destruction)
    {
        return completeScheduledDestructionFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    }
    if (kind == IterativeExpressionKind::StackAllocation)
    {
        return completeScheduledStackAllocationFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    }
    if (kind == IterativeExpressionKind::StorageUnary)
    {
        return completeScheduledStorageUnaryFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    }
    if (kind == IterativeExpressionKind::MoveProjection)
    {
        return completeScheduledMoveProjectionFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, lowered, error, rootCleanupDepth,
                rootExceptionHandlerDepth);
    }
    return std::nullopt;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledResultPropagationFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::ResultPropagation)
    {
        IterativeExpressionFrame& frame = frames.back();
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
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledDestructionFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::Destruction)
    {
        IterativeExpressionFrame& frame = frames.back();
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
                // ES: Los slots que guardan literales no pertenecen al array: sólo se libera el bloque.
                // EN: Slots holding literals do not belong to the array: only the block is released.
                if (frame.operandType->borrowedElementSlots)
                    deallocation->storageOwnership = sema::OwnershipKind::None;
                // EN: Freeze 3.0 (item 130, F2): a plain local array does not own its references.
                // ES: Congelamiento 3.0 (elemento 130, F2): un local arreglo plano no posee sus referencias.
                if (methodBodyModel_ != nullptr && methodBodyModel_->arrayDeleteKeepsElements(frame.syntax))
                    deallocation->storageOwnership = sema::OwnershipKind::None;
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
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledStackAllocationFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::StackAllocation)
    {
        IterativeExpressionFrame& frame = frames.back();
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
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledStorageUnaryFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::StorageUnary)
    {
        IterativeExpressionFrame& frame = frames.back();
        finishStorageUnaryFrame(frame, lowered);
        frames.pop_back();
        continue;

    }
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

// ES: Construye la operación unaria de almacenamiento preservando clase de nodo y constantes.
// EN: Builds the storage unary operation while preserving node kind and constants.
// Parámetros / Parameters: frame aporta sintaxis y contexto; lowered contiene y recibe el valor.
// Retorno / Return: no aplica; consume el mismo hijo; O(1).
void ProjectOopHirModel::finishStorageUnaryFrame(
    IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode>& lowered)
{
    const MethodExpressionTypeBinding* frameType = expressionType(frame.syntax);
    const MethodDelegateBinding* frameDelegate = delegateBinding(frame.syntax);
    const std::string resultType = frameType != nullptr
        ? frameType->typeKey
        : frameDelegate != nullptr ? frameDelegate->resultTypeKey : "<error>";
    bool directCVolatileInt = false;
    std::optional<bool> directCVolatileMode;
    const ProjectOopHirNode* provenance = lowered.get();
    bool indexedCGlobal = false;
    if (provenance && provenance->kind == ProjectOopHirNodeKind::PointerLoad &&
        provenance->typeKey == resultType + "*" && provenance->children.size() == 1)
    {
        provenance = provenance->children.front().get();
        unsigned depth = 0;
        while (provenance && provenance->kind == ProjectOopHirNodeKind::RawElementAddress &&
               provenance->children.size() == 2 && ++depth <= 64)
            provenance = provenance->children.front().get();
        indexedCGlobal = depth > 0 && depth <= 64 && provenance &&
            provenance->kind == ProjectOopHirNodeKind::NamespaceStateAddress;
    }
    if ((resultType == "builtin:bool" || resultType == "builtin:sbyte" || resultType == "builtin:byte" ||
         resultType == "builtin:int" || resultType == "builtin:uint" || resultType == "builtin:short" ||
         resultType == "builtin:ushort" || resultType == "builtin:long" || resultType == "builtin:ulong" ||
         resultType == "builtin:double" || resultType == "builtin:float") && provenance &&
        (provenance->kind == ProjectOopHirNodeKind::NamespaceStateValue || indexedCGlobal) &&
        provenance->namespaceStateOrdinal && namespaceFunctionModel_ &&
        *provenance->namespaceStateOrdinal < namespaceFunctionModel_->states().size())
    {
        const auto& state = namespaceFunctionModel_->states()[*provenance->namespaceStateOrdinal];
        directCVolatileInt = state.isCppExternal && state.cppCGlobal &&
            ((indexedCGlobal && state.declaredTypeKey.starts_with(resultType + "*[")) ||
             (!indexedCGlobal && state.declaredTypeKey == resultType + "*")) &&
            state.cppPointerPointeeVolatile &&
            state.cppMicrosoftVolatileSemantics.has_value();
        if (directCVolatileInt) directCVolatileMode = state.cppMicrosoftVolatileSemantics;
    }
    // EN: A C record field holding a pointer to a volatile scalar follows the record's volatile mode.
    // ES: Un campo de record C con puntero a escalar volatile sigue el modo volatile del record.
    if (!directCVolatileInt && lowered && lowered->kind == ProjectOopHirNodeKind::FieldValue)
        if (const auto mode = internal::cFieldVolatilePointeeMode(memberModel_, *lowered, resultType))
        {
            directCVolatileInt = true;
            directCVolatileMode = mode;
        }
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
                    cppImportedCEnumUnderlying(nameLookup_, targetType->typeKey) ? "builtin:int" : targetType->typeKey);
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
            const auto* operandType = expressionType(frame.syntax->first());
            load->volatileAccess = operandType && operandType->pointerPointeeVolatile;
            // EN: T08-D13. Through a C alias the access follows the volatile mode of the C origin.
            // ES: T08-D13. Por un alias C el acceso sigue el modo volatile del origen C.
            if (load->volatileAccess)
                if (const auto alias = internal::cVolatileAliasAccess(operandType,
                        methodBodyModel_ ? internal::cAliasLocal(methodBodyModel_->locals(), lowered.get()) : nullptr,
                        resultType))
                {
                    load->cppMicrosoftVolatileSemantics = alias->microsoft;
                    load->cppAtomicAlignmentBytes = alias->atomicWidth;
                }
            if (directCVolatileInt && load->volatileAccess)
            {
                load->cppMicrosoftVolatileSemantics = directCVolatileMode;
                load->cppAtomicAlignmentBytes = (resultType == "builtin:bool" || resultType == "builtin:sbyte" || resultType == "builtin:byte") ? 1 :
                    (resultType == "builtin:short" || resultType == "builtin:ushort") ? 2 :
                    (resultType == "builtin:long" || resultType == "builtin:ulong" || resultType == "builtin:double") ? 8 : 4;
            }
            load->children.push_back(std::move(lowered));
            lowered = std::move(load);
        }
        else if (lowered)
        {
            const auto* operandType = expressionType(frame.syntax->first());
            const bool volatilePointee = operandType && operandType->pointerPointeeVolatile;
            if (lowered->volatileAccess || volatilePointee)
            {
                // Qualify the target independently of the pointer value's own load.
                auto address = makeNode(ProjectOopHirNodeKind::AddressValue, frame.syntax, resultType);
                address->volatileAccess = volatilePointee;
                // EN: T08-D13. Through a C alias the access follows the volatile mode of the C origin.
                // ES: T08-D13. Por un alias C el acceso sigue el modo volatile del origen C.
                if (volatilePointee)
                    if (const auto alias = internal::cVolatileAliasAccess(operandType,
                            methodBodyModel_ ? internal::cAliasLocal(methodBodyModel_->locals(), lowered.get())
                                             : nullptr,
                            resultType))
                    {
                        address->cppMicrosoftVolatileSemantics = alias->microsoft;
                        address->cppAtomicAlignmentBytes = alias->atomicWidth;
                    }
                if (directCVolatileInt && volatilePointee)
                {
                    address->cppMicrosoftVolatileSemantics = directCVolatileMode;
                    address->cppAtomicAlignmentBytes = (resultType == "builtin:bool" || resultType == "builtin:sbyte" || resultType == "builtin:byte") ? 1 :
                        (resultType == "builtin:short" || resultType == "builtin:ushort") ? 2 :
                        (resultType == "builtin:long" || resultType == "builtin:ulong" || resultType == "builtin:double") ? 8 : 4;
                }
                address->children.push_back(std::move(lowered));
                lowered = std::move(address);
            }
        }
}

LoopBlockAdvance ProjectOopHirModel::completeScheduledMoveProjectionFrame(
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
    bool descend = false;
    while (!frames.empty() &&
        frames.back().kind == IterativeExpressionKind::MoveProjection)
    {
        IterativeExpressionFrame& frame = frames.back();
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
    if (lowered == nullptr && !error.empty()) return LoopBlockAdvance::Error;
    return descend ? LoopBlockAdvance::Descend : LoopBlockAdvance::Complete;
}

}
