#include "MirNodeMachine.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
std::uint64_t stableTypeId(std::string_view value) noexcept
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : value)
    {
        const auto byte = static_cast<unsigned char>(character);
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}
}

void ProjectOopMirModel::MirNodeMachine::copyIdentity(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result)
{
    result->syntax = node->syntax;
    result->typeKey = node->typeKey;
    result->elementTypeKey = node->elementTypeKey;
    result->stringValue = node->stringValue;
    result->debugDisplayName = node->debugDisplayName;
    result->operation = node->operation;
    result->memberOrdinal = node->memberOrdinal;
    result->namespaceFunctionOrdinal = node->namespaceFunctionOrdinal;
    result->namespaceStateOrdinal = node->namespaceStateOrdinal;
    result->secondaryMemberOrdinal = node->secondaryMemberOrdinal;
    result->resultSuccessTestMemberOrdinal = node->resultSuccessTestMemberOrdinal;
    result->resultSuccessExtractorMemberOrdinal =
        node->resultSuccessExtractorMemberOrdinal;
    result->resultErrorExtractorMemberOrdinal =
        node->resultErrorExtractorMemberOrdinal;
    result->resultErrorFactoryMemberOrdinal = node->resultErrorFactoryMemberOrdinal;
    result->resultErrorConversionMemberOrdinal =
        node->resultErrorConversionMemberOrdinal;
    result->localOrdinal = node->localOrdinal;
    result->allocatorContextLocalOrdinal = node->allocatorContextLocalOrdinal;
    result->allocatorDeallocateMemberOrdinal = node->allocatorDeallocateMemberOrdinal;
    result->allocatorTypeOrdinal = node->allocatorTypeOrdinal;
    if (node->allocatorDeallocateMemberOrdinal.has_value() && model_.hirModel_ != nullptr)
    {
        const ProjectOopHirFunction* target =
            model_.hirModel_->functionForMember(*node->allocatorDeallocateMemberOrdinal);
        if (target != nullptr) result->allocatorDeallocateCalleeName = target->mangledName;
    }
}

void ProjectOopMirModel::MirNodeMachine::copyControlState(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result)
{
    result->parameterOrdinal = node->parameterOrdinal;
    result->constructionOrdinal = node->constructionOrdinal;
    result->targetTypeOrdinal = node->targetTypeOrdinal;
    result->exceptionTypeOrdinal = node->exceptionTypeOrdinal;
    result->cleanupRegionOrdinal = node->cleanupRegionOrdinal;
    result->mirRegionOrdinal = node->hirRegionOrdinal;
    result->exceptionTargetRegionOrdinal = node->exceptionTargetRegionOrdinal;
    result->targetScopeOrdinal = node->targetScopeOrdinal;
    result->disposeContractMemberOrdinal = node->disposeContractMemberOrdinal;
    result->disposeImplementationMemberOrdinal = node->disposeImplementationMemberOrdinal;
    result->cleanupRegionOrdinals = node->cleanupRegionOrdinals;
    result->cleanupContinuationKind = node->cleanupContinuationKind;
    result->borrowKind = node->borrowKind;
    result->localBorrowSourceKind = node->localBorrowSourceKind;
    result->captureMode = node->captureMode;
    if (node->conversion.has_value())
    {
        result->conversionKind = node->conversion->kind;
    }
    // SwitchSection only. Copied verbatim: the values were folded once, in the body model.
    result->switchLabels = node->switchLabels;
    result->switchHasDefault = node->switchHasDefault;
    result->integerValue = node->integerValue;
    result->secondaryImmediate = node->secondaryIntegerValue;
    result->resultSuccessTag = node->resultSuccessTag;
    result->resultErrorTag = node->resultErrorTag;
    result->resultReturnErrorTag = node->resultReturnErrorTag;
    result->resultSourceCarrierTypeKey = node->resultSourceCarrierTypeKey;
    result->resultSourceErrorTypeKey = node->resultSourceErrorTypeKey;
    result->resultReturnCarrierTypeKey = node->resultReturnCarrierTypeKey;
    result->resultReturnErrorTypeKey = node->resultReturnErrorTypeKey;
}

void ProjectOopMirModel::MirNodeMachine::copyAttributes(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result)
{
    result->bitOffset = node->bitOffset;
    result->bitWidth = node->bitWidth;
    result->bitStorageSize = node->bitStorageSize;
    result->floatingValue = node->floatingValue;
    result->booleanValue = node->booleanValue;
    result->collectionSegments = node->collectionSegments;
    result->referenceLocal = node->referenceLocal;
    result->referenceMutable = node->referenceMutable;
    result->staticLocal = node->staticLocal;
    result->pinnedStorage = node->pinnedStorage;
    result->referenceResult = node->referenceResult;
    result->governingReference = node->governingReference;
    result->nullConditionalCall = node->nullConditionalCall;
    result->returnsAssignedValue = node->returnsAssignedValue;
    result->mustTailCall = node->mustTailCall;
    result->ownership = node->ownership;
    result->automaticDestruction = node->automaticDestruction;
    result->variableLengthArray = node->variableLengthArray;
    result->variableLengthArrayRank = node->variableLengthArrayRank;
    result->nativeArrayRank = node->nativeArrayRank;
    result->placementConstructed = node->placementConstructed;
    result->stackPromotedObject = node->stackPromotedObject;
    result->volatileAccess = node->volatileAccess;
    result->baseReceiver = node->baseReceiver;
    result->targetIsInterface = node->targetIsInterface;
    result->dynamicDestruction = node->dynamicDestruction;
    result->valueTypeNoOpDeletion = node->valueTypeNoOpDeletion;
    result->objectLifetimeOwnership = node->objectLifetimeOwnership;
    result->storageOwnership = node->storageOwnership;
    result->catchAll = node->catchAll;
    result->catchHasFilter = node->catchHasFilter;
    result->catchExhaustive = node->catchExhaustive;
    result->catchMatchedErrorTypeKeys = node->catchMatchedErrorTypeKeys;
    result->interfaceCleanupDispatch = node->interfaceCleanupDispatch;
    result->argumentCount = node->argumentCount;
    result->argumentParameterOrdinals = node->argumentParameterOrdinals;
    result->argumentTypeKeys = node->argumentTypeKeys;

}

int ProjectOopMirModel::MirNodeMachine::lowerInlineAssembly(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result)
{
    result->kind = ProjectOopMirNodeKind::InlineAssembly;
    result->inlineAssemblyHasSideEffects = true;
    for (const auto& child : node->children)
    {
        if (child == nullptr ||
            child->kind != ProjectOopHirNodeKind::InlineAssemblyClobber) continue;
        result->inlineAssemblyClobbersMemory =
            result->inlineAssemblyClobbersMemory || child->elementTypeKey == "memory";
        result->inlineAssemblyClobbersFlags =
            result->inlineAssemblyClobbersFlags || child->elementTypeKey == "flags";
    }
    return 1;
}

int ProjectOopMirModel::MirNodeMachine::lowerMatchExpression(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    result->kind = ProjectOopMirNodeKind::MatchExpression;
    if (node->booleanValue)
    {
        if (!node->secondaryMemberOrdinal.has_value())
        {
            error = "T03-F03 switch match lost its variant tag member";
            return -1;
        }
        const ObjectFieldLayout* tagLayout = model_.fieldLayout(*node->secondaryMemberOrdinal);
        if (tagLayout == nullptr)
        {
            error = "T03-F03 switch match cannot resolve its variant tag offset";
            return -1;
        }
        result->immediate = tagLayout->offset;
        result->alignment = tagLayout->alignment;
    }
    return 1;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup01(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::Block: result->kind = ProjectOopMirNodeKind::Block; break;
        case ProjectOopHirNodeKind::BlockExpression: result->kind = ProjectOopMirNodeKind::BlockExpression; break;
        case ProjectOopHirNodeKind::LocalDeclaration: result->kind = ProjectOopMirNodeKind::LocalDeclaration; break;
        case ProjectOopHirNodeKind::ExpressionStatement:
            result->kind = ProjectOopMirNodeKind::ExpressionStatement;
            break;
        case ProjectOopHirNodeKind::InlineAssembly:
            return lowerInlineAssembly(node, result);
        case ProjectOopHirNodeKind::InlineAssemblyLine:
            result->kind = ProjectOopMirNodeKind::InlineAssemblyLine;
            break;
        case ProjectOopHirNodeKind::InlineAssemblyInput:
            result->kind = ProjectOopMirNodeKind::InlineAssemblyInput;
            break;
        case ProjectOopHirNodeKind::InlineAssemblyOutput:
            result->kind = ProjectOopMirNodeKind::InlineAssemblyOutput;
            break;
        case ProjectOopHirNodeKind::InlineAssemblyClobber:
            result->kind = ProjectOopMirNodeKind::InlineAssemblyClobber;
            break;
        case ProjectOopHirNodeKind::If: result->kind = ProjectOopMirNodeKind::If; break;
        case ProjectOopHirNodeKind::While: result->kind = ProjectOopMirNodeKind::While; break;
        case ProjectOopHirNodeKind::DoWhile: result->kind = ProjectOopMirNodeKind::DoWhile; break;
        case ProjectOopHirNodeKind::Conditional: result->kind = ProjectOopMirNodeKind::Conditional; break;
        case ProjectOopHirNodeKind::LoopExpression:
            result->kind = ProjectOopMirNodeKind::LoopExpression;
            break;
        case ProjectOopHirNodeKind::MatchExpression:
            return lowerMatchExpression(node, result, error);
        case ProjectOopHirNodeKind::MatchArm:
            result->kind = ProjectOopMirNodeKind::MatchArm;
            break;
        case ProjectOopHirNodeKind::MatchValue:
            result->kind = ProjectOopMirNodeKind::MatchValue;
            break;
        case ProjectOopHirNodeKind::OverflowPolicy:
            result->kind = ProjectOopMirNodeKind::OverflowPolicy;
            result->stringValue = node->stringValue;
            break;
        case ProjectOopHirNodeKind::NullCoalescing:
            result->kind = ProjectOopMirNodeKind::NullCoalescing;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup02(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::OptionalCoalescing:
        {
            if (!node->memberOrdinal.has_value() ||
                !node->secondaryMemberOrdinal.has_value() || model_.hirModel_ == nullptr)
            {
                error = "E017 Optional coalescing lost its public protocol member identities";
                return -1;
            }
            const ProjectOopHirFunction* hasValue =
                model_.hirModel_->functionForMember(*node->memberOrdinal);
            const ProjectOopHirFunction* uncheckedValue =
                model_.hirModel_->functionForMember(*node->secondaryMemberOrdinal);
            if (hasValue == nullptr || uncheckedValue == nullptr)
            {
                error = "E017 Optional coalescing protocol methods have no HIR callables";
                return -1;
            }
            result->kind = ProjectOopMirNodeKind::OptionalCoalescing;
            result->calleeName = hasValue->mangledName;
            result->secondaryCalleeName = uncheckedValue->mangledName;
            break;
        }
        case ProjectOopHirNodeKind::NullCoalescingAssignment:
            result->kind = ProjectOopMirNodeKind::NullCoalescingStore;
            break;
        case ProjectOopHirNodeKind::For: result->kind = ProjectOopMirNodeKind::For; break;
        case ProjectOopHirNodeKind::Switch: result->kind = ProjectOopMirNodeKind::Switch; break;
        case ProjectOopHirNodeKind::SwitchSection:
            result->kind = ProjectOopMirNodeKind::SwitchSection;
            break;
        case ProjectOopHirNodeKind::Break: result->kind = ProjectOopMirNodeKind::Break; break;
        case ProjectOopHirNodeKind::Continue: result->kind = ProjectOopMirNodeKind::Continue; break;
        case ProjectOopHirNodeKind::Return: result->kind = ProjectOopMirNodeKind::Return; break;
        case ProjectOopHirNodeKind::TryRegion: result->kind = ProjectOopMirNodeKind::TryRegion; break;
        case ProjectOopHirNodeKind::CatchRegion: result->kind = ProjectOopMirNodeKind::CatchRegion; break;
        case ProjectOopHirNodeKind::FinallyRegion: result->kind = ProjectOopMirNodeKind::FinallyRegion; break;
        case ProjectOopHirNodeKind::UsingRegion: result->kind = ProjectOopMirNodeKind::UsingRegion; break;
        case ProjectOopHirNodeKind::Throw: result->kind = ProjectOopMirNodeKind::Throw; break;
        case ProjectOopHirNodeKind::Rethrow: result->kind = ProjectOopMirNodeKind::Rethrow; break;
        case ProjectOopHirNodeKind::Assert: result->kind = ProjectOopMirNodeKind::Assert; break;
        case ProjectOopHirNodeKind::Unreachable: result->kind = ProjectOopMirNodeKind::Unreachable; break;
        case ProjectOopHirNodeKind::Assume: result->kind = ProjectOopMirNodeKind::Assume; break;
        case ProjectOopHirNodeKind::Panic: result->kind = ProjectOopMirNodeKind::Panic; break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerResultPropagation(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    if (model_.hirModel_ == nullptr ||
        !node->resultSuccessTestMemberOrdinal.has_value() ||
        !node->resultSuccessExtractorMemberOrdinal.has_value() ||
        !node->resultErrorExtractorMemberOrdinal.has_value() ||
        !node->resultErrorFactoryMemberOrdinal.has_value())
    {
        error = "T07-F01 Result propagation lost a protocol member identity";
        return -1;
    }
    const ProjectOopHirFunction* successTest = model_.hirModel_->functionForMember(
        *node->resultSuccessTestMemberOrdinal);
    const ProjectOopHirFunction* successExtractor = model_.hirModel_->functionForMember(
        *node->resultSuccessExtractorMemberOrdinal);
    const ProjectOopHirFunction* errorExtractor = model_.hirModel_->functionForMember(
        *node->resultErrorExtractorMemberOrdinal);
    const ProjectOopHirFunction* errorFactory = model_.hirModel_->functionForMember(
        *node->resultErrorFactoryMemberOrdinal);
    const ProjectOopHirFunction* errorConversion =
        node->resultErrorConversionMemberOrdinal.has_value()
            ? model_.hirModel_->functionForMember(*node->resultErrorConversionMemberOrdinal)
            : nullptr;
    if (successTest == nullptr || successTest->isStatic ||
        successExtractor == nullptr || successExtractor->isStatic ||
        errorExtractor == nullptr || errorExtractor->isStatic ||
        errorFactory == nullptr || !errorFactory->isStatic ||
        (node->resultErrorConversionMemberOrdinal.has_value() &&
         (errorConversion == nullptr || !errorConversion->isStatic)))
    {
        error = "T07-F01 Result protocol members have incompatible callable shapes";
        return -1;
    }
    result->kind = ProjectOopMirNodeKind::ResultPropagation;
    result->resultSuccessTestCalleeName = successTest->mangledName;
    result->resultSuccessExtractorCalleeName = successExtractor->mangledName;
    result->resultErrorExtractorCalleeName = errorExtractor->mangledName;
    result->resultErrorFactoryCalleeName = errorFactory->mangledName;
    if (errorConversion != nullptr)
        result->resultErrorConversionCalleeName = errorConversion->mangledName;
    return 1;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup03(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::ResultPropagation:
            return lowerResultPropagation(node, result, error);
        case ProjectOopHirNodeKind::CleanupContinuation:
            result->kind = ProjectOopMirNodeKind::CleanupContinuation;
            break;
        case ProjectOopHirNodeKind::OwnedParameterCleanup:
            result->kind = ProjectOopMirNodeKind::OwnedParameterCleanup;
            break;
        case ProjectOopHirNodeKind::MoveValue:
            result->kind = ProjectOopMirNodeKind::MoveValue;
            break;
        case ProjectOopHirNodeKind::MoveArrayElement:
            result->kind = ProjectOopMirNodeKind::MoveArrayElement;
            break;
        case ProjectOopHirNodeKind::MoveOwnedField:
            result->kind = ProjectOopMirNodeKind::MoveOwnedField;
            break;
        case ProjectOopHirNodeKind::MoveIteratorCurrent:
            result->kind = ProjectOopMirNodeKind::MoveIteratorCurrent;
            break;
        case ProjectOopHirNodeKind::OwnedTemporaryBorrow:
            result->kind = ProjectOopMirNodeKind::OwnedTemporaryBorrow;
            break;
        case ProjectOopHirNodeKind::FullExpressionCleanup:
            result->kind = ProjectOopMirNodeKind::FullExpressionCleanup;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerRawElementAddress(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    // C2.1 "fixed arrays": the element SIZE and ALIGNMENT the GEP needs. Same layout
    // question the managed-array path above already answers, asked through the same
    // helper, so a struct element gets its real size rather than a pointer's.
    if (model_.nameLookup_ == nullptr || model_.objectLayoutModel_ == nullptr)
    {
        error = "C2.1 raw element lowering requires name lookup and value layouts";
        return -1;
    }
    const auto layout = model_.elementLayout(node->elementTypeKey);
    if (!layout.has_value() || layout->first == 0)
    {
        error = "C2.1 cannot determine raw element layout for '" + node->elementTypeKey + "'";
        return -1;
    }
    result->immediate = layout->first;
    result->alignment = layout->second;
    // NS304-T13-F06: the flexible array's `Length` bounds-check data, resolved by the HIR
    // from [TrailingStorage(...)]; the raw element address emits `index < Length`.
    result->hasTrailingLengthBounds = node->hasTrailingLengthBounds;
    result->trailingLengthRelativeOffset = node->trailingLengthRelativeOffset;
    result->kind = ProjectOopMirNodeKind::RawElementAddress;
    return 1;
}

int ProjectOopMirModel::MirNodeMachine::lowerRawStorage(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    // CC1 fases 4B, 4C y 4D. Las tres necesitan LO MISMO -- cuanto mide un elemento -- y las
    // tres lo piden por `elementLayout`, que es el mismo helper del que sale el stride de
    // `p[i]` y el offset de un campo. Ni una tabla de tamanos nueva: dos reglas de tamano que
    // pudieran divergir es exactamente el defecto que este milestone existe para quitar.
    if (model_.nameLookup_ == nullptr || model_.objectLayoutModel_ == nullptr)
    {
        error = "CC1 raw storage lowering requires name lookup and value layouts";
        return -1;
    }
    const auto layout = model_.elementLayout(node->elementTypeKey);
    if (!layout.has_value() || layout->first == 0)
    {
        error = "CC1 cannot determine the element layout of '" + node->elementTypeKey + "'";
        return -1;
    }
    result->immediate = layout->first;
    result->alignment = layout->second;
    result->kind = node->kind == ProjectOopHirNodeKind::StackAllocation
        ? ProjectOopMirNodeKind::StackAllocation
        : (node->kind == ProjectOopHirNodeKind::RawAllocation
               ? ProjectOopMirNodeKind::RawAllocation
               : ProjectOopMirNodeKind::PointerArithmetic);
    return 1;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup04(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::RawElementAddress:
            return lowerRawElementAddress(node, result, error);
        case ProjectOopHirNodeKind::StackAllocation:
        case ProjectOopHirNodeKind::RawAllocation:
        case ProjectOopHirNodeKind::PointerArithmetic:
            return lowerRawStorage(node, result, error);
        case ProjectOopHirNodeKind::PointerLoad:
            result->kind = ProjectOopMirNodeKind::PointerLoad;
            break;
        case ProjectOopHirNodeKind::AddressValue:
            result->kind = ProjectOopMirNodeKind::AddressValue;
            break;
        case ProjectOopHirNodeKind::BorrowValue:
            result->kind = ProjectOopMirNodeKind::BorrowValue;
            break;
        case ProjectOopHirNodeKind::ThisValue: result->kind = ProjectOopMirNodeKind::ThisValue; break;
        case ProjectOopHirNodeKind::ParameterValue: result->kind = ProjectOopMirNodeKind::ParameterValue; break;
        case ProjectOopHirNodeKind::LocalValue: result->kind = ProjectOopMirNodeKind::LocalValue; break;
        case ProjectOopHirNodeKind::ParameterAddress: result->kind = ProjectOopMirNodeKind::ParameterAddress; break;
        case ProjectOopHirNodeKind::LocalAddress: result->kind = ProjectOopMirNodeKind::LocalAddress; break;
        case ProjectOopHirNodeKind::IntegerConstant: result->kind = ProjectOopMirNodeKind::IntegerConstant; break;
        case ProjectOopHirNodeKind::FloatingConstant: result->kind = ProjectOopMirNodeKind::FloatingConstant; break;
        case ProjectOopHirNodeKind::BooleanConstant: result->kind = ProjectOopMirNodeKind::BooleanConstant; break;
        case ProjectOopHirNodeKind::NullConstant: result->kind = ProjectOopMirNodeKind::NullConstant; break;
        case ProjectOopHirNodeKind::DefaultValue: result->kind = ProjectOopMirNodeKind::DefaultValue; break;
        case ProjectOopHirNodeKind::StringLiteral:
            result->kind = ProjectOopMirNodeKind::StringLiteral;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerUtf8StringLiteral(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result)
{
    result->kind = ProjectOopMirNodeKind::Utf8StringLiteral;
    result->typeKey = node->typeKey;
    result->elementTypeKey = node->elementTypeKey;
    result->stringValue = node->stringValue;
    result->integerValue = node->integerValue;
    return 1;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup05(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string&)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::Utf8StringLiteral:
            return lowerUtf8StringLiteral(node, result);
        case ProjectOopHirNodeKind::ArrayLength:
            result->kind = ProjectOopMirNodeKind::ArrayLength;
            break;
        case ProjectOopHirNodeKind::StringLength:
            result->kind = ProjectOopMirNodeKind::StringLength;
            break;
        case ProjectOopHirNodeKind::StringElementValue:
            result->kind = ProjectOopMirNodeKind::StringElementLoad;
            result->immediate = 1;
            result->alignment = 1;
            break;
        case ProjectOopHirNodeKind::EnumName:
            result->kind = ProjectOopMirNodeKind::EnumName;
            break;
        case ProjectOopHirNodeKind::StringConcat:
            result->kind = ProjectOopMirNodeKind::StringConcat;
            break;
        case ProjectOopHirNodeKind::StringEquals:
            result->kind = ProjectOopMirNodeKind::StringEquals;
            break;
        case ProjectOopHirNodeKind::StringCharacterPointer:
            result->kind = ProjectOopMirNodeKind::StringCharacterPointer;
            break;
        case ProjectOopHirNodeKind::StringSubstring:
            result->kind = ProjectOopMirNodeKind::StringSubstring;
            break;
        case ProjectOopHirNodeKind::StringFromCharArray:
            result->kind = ProjectOopMirNodeKind::StringFromCharArray;
            break;
        case ProjectOopHirNodeKind::StringViewFromString:
            result->kind = ProjectOopMirNodeKind::StringViewFromString;
            break;
        case ProjectOopHirNodeKind::SpanFromArray:
            result->kind = ProjectOopMirNodeKind::SpanFromArray;
            break;
        case ProjectOopHirNodeKind::ReadOnlySpanFromArray:
            result->kind = ProjectOopMirNodeKind::ReadOnlySpanFromArray;
            break;
        case ProjectOopHirNodeKind::SpanFromFixedArray:
            result->kind = ProjectOopMirNodeKind::SpanFromFixedArray;
            break;
        case ProjectOopHirNodeKind::ReadOnlySpanFromFixedArray:
            result->kind = ProjectOopMirNodeKind::ReadOnlySpanFromFixedArray;
            break;
        // SHR-022. La mascara de dos bits que el HIR fijo pasa a `immediate`, que es de donde el
        // backend saca los operandos constantes. El prologo generico de esta funcion ya copia
        // `integerValue`, pero ESE campo no lo mira nadie aguas abajo para este nodo: sin esta
        // linea la mascara se queda en el HIR y la llamada al runtime sale con lo que hubiera en
        // `immediate` -- cero -- asi que las cuatro salidas escribirian por salida estandar y sin
        // salto de linea, en silencio.
        case ProjectOopHirNodeKind::CoreConsoleWriteLine:
            result->kind = ProjectOopMirNodeKind::CoreConsoleWriteLine;
            result->immediate = node->integerValue;
            break;
        case ProjectOopHirNodeKind::CoreConsoleReadLine:
            result->kind = ProjectOopMirNodeKind::CoreConsoleReadLine;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerAggregateOperation(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    // C8 struct half: the aggregate's size and alignment, so the backend can spill the
    // operands into slots it can walk. The leaves themselves already carry their offsets
    // from the HIR pass and are copied generically with the rest of the children.
    if (model_.nameLookup_ == nullptr || model_.objectLayoutModel_ == nullptr)
    {
        error = "C8 struct root member requires name lookup and value layouts";
        return -1;
    }
    const auto layout = model_.elementLayout(node->elementTypeKey);
    if (!layout.has_value() || layout->first == 0)
    {
        error = "C8 cannot determine the layout of struct '" + node->elementTypeKey + "'";
        return -1;
    }
    result->kind = node->kind == ProjectOopHirNodeKind::AggregateEquals
        ? ProjectOopMirNodeKind::AggregateEquals
        : ProjectOopMirNodeKind::AggregateHash;
    result->immediate = layout->first;
    result->alignment = layout->second;
    return 1;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup06(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::AggregateEquals:
        case ProjectOopHirNodeKind::AggregateHash:
            return lowerAggregateOperation(node, result, error);
        case ProjectOopHirNodeKind::CoreObjectTypeName:
            result->kind = ProjectOopMirNodeKind::CoreObjectTypeName;
            break;
        case ProjectOopHirNodeKind::CoreObjectReferenceHash:
            result->kind = ProjectOopMirNodeKind::CoreObjectReferenceHash;
            break;
        case ProjectOopHirNodeKind::CoreClockQueryCounter:
            result->kind = ProjectOopMirNodeKind::CoreClockQueryCounter;
            break;
        case ProjectOopHirNodeKind::CoreClockQueryFrequency:
            result->kind = ProjectOopMirNodeKind::CoreClockQueryFrequency;
            break;
        case ProjectOopHirNodeKind::CoreClockQuerySystemFileTime:
            result->kind = ProjectOopMirNodeKind::CoreClockQuerySystemFileTime;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentTickCount64:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentTickCount64;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentProcessorCount:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentProcessorCount;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentExit:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentExit;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentGetVariable:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentGetVariable;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentSetVariable:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentSetVariable;
            break;
        case ProjectOopHirNodeKind::CoreFileExists:
            result->kind = ProjectOopMirNodeKind::CoreFileExists;
            break;
        case ProjectOopHirNodeKind::CoreFileDelete:
            result->kind = ProjectOopMirNodeKind::CoreFileDelete;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryExists:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryExists;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryCreate:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryCreate;
            break;
        case ProjectOopHirNodeKind::CoreFileReadAllText:
            result->kind = ProjectOopMirNodeKind::CoreFileReadAllText;
            break;
        case ProjectOopHirNodeKind::CoreFileWriteAllText:
            result->kind = ProjectOopMirNodeKind::CoreFileWriteAllText;
            break;
        case ProjectOopHirNodeKind::CoreFileAppendAllText:
            result->kind = ProjectOopMirNodeKind::CoreFileAppendAllText;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup07FirstHalf(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::CoreDirectoryDelete:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryDelete;
            break;
        case ProjectOopHirNodeKind::CorePathGetFullPath:
            result->kind = ProjectOopMirNodeKind::CorePathGetFullPath;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentGetCommandLineArgCount:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgCount;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentGetCommandLineArgs:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentGetCommandLineArgs;
            break;
        case ProjectOopHirNodeKind::CoreFileGetSize:
            result->kind = ProjectOopMirNodeKind::CoreFileGetSize;
            break;
        case ProjectOopHirNodeKind::CoreFileReadAllBytes:
            result->kind = ProjectOopMirNodeKind::CoreFileReadAllBytes;
            break;
        case ProjectOopHirNodeKind::CoreFileReadBytes:
            result->kind = ProjectOopMirNodeKind::CoreFileReadBytes;
            break;
        case ProjectOopHirNodeKind::CoreFileWriteAllBytes:
            result->kind = ProjectOopMirNodeKind::CoreFileWriteAllBytes;
            break;
        case ProjectOopHirNodeKind::CoreEnvironmentCurrentDirectory:
            result->kind = ProjectOopMirNodeKind::CoreEnvironmentCurrentDirectory;
            break;
        case ProjectOopHirNodeKind::CoreProcessRun:
            result->kind = ProjectOopMirNodeKind::CoreProcessRun;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryGetFileCount:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryGetFileCount;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryGetFiles:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryGetFiles;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup07(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string&)
{
    const int firstHalf = lowerGroup07FirstHalf(node, result);
    if (firstHalf != 0) return firstHalf;
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::CoreDirectoryGetDirectoryCount:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryGetDirectoryCount;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryGetDirectories:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryGetDirectories;
            break;
        case ProjectOopHirNodeKind::CoreThreadCreate:
            result->kind = ProjectOopMirNodeKind::CoreThreadCreate;
            break;
        case ProjectOopHirNodeKind::CoreThreadJoin:
            result->kind = ProjectOopMirNodeKind::CoreThreadJoin;
            break;
        case ProjectOopHirNodeKind::CoreThreadDetach:
            result->kind = ProjectOopMirNodeKind::CoreThreadDetach;
            break;
        case ProjectOopHirNodeKind::CoreAtomicLoadInt32:
            result->kind = ProjectOopMirNodeKind::CoreAtomicLoadInt32;
            break;
        case ProjectOopHirNodeKind::CoreAtomicStoreInt32:
            result->kind = ProjectOopMirNodeKind::CoreAtomicStoreInt32;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedIncrementInt32:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedIncrementInt32;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedDecrementInt32:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedDecrementInt32;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedAddInt32:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedAddInt32;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedExchangeInt32:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedExchangeInt32;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedCompareExchangeInt32:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt32;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup08FirstHalf(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::CoreAtomicLoadInt64:
            result->kind = ProjectOopMirNodeKind::CoreAtomicLoadInt64;
            break;
        case ProjectOopHirNodeKind::CoreAtomicStoreInt64:
            result->kind = ProjectOopMirNodeKind::CoreAtomicStoreInt64;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedIncrementInt64:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedIncrementInt64;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedDecrementInt64:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedDecrementInt64;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedAddInt64:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedAddInt64;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedExchangeInt64:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedExchangeInt64;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedCompareExchangeInt64:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt64;
            break;
        case ProjectOopHirNodeKind::CoreAtomicLoadPointer:
            result->kind = ProjectOopMirNodeKind::CoreAtomicLoadPointer;
            break;
        case ProjectOopHirNodeKind::CoreAtomicStorePointer:
            result->kind = ProjectOopMirNodeKind::CoreAtomicStorePointer;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedExchangePointer:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedExchangePointer;
            break;
        case ProjectOopHirNodeKind::CoreInterlockedCompareExchangePointer:
            result->kind = ProjectOopMirNodeKind::CoreInterlockedCompareExchangePointer;
            break;
        case ProjectOopHirNodeKind::CoreAtomicThreadFence:
            result->kind = ProjectOopMirNodeKind::CoreAtomicThreadFence;
            break;
        case ProjectOopHirNodeKind::CoreAtomicCompilerFence:
            result->kind = ProjectOopMirNodeKind::CoreAtomicCompilerFence;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup08(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string&)
{
    const int firstHalf = lowerGroup08FirstHalf(node, result);
    if (firstHalf != 0) return firstHalf;
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::CoreSimdFloat32x4Add:
            result->kind = ProjectOopMirNodeKind::CoreSimdFloat32x4Add;
            break;
        case ProjectOopHirNodeKind::CoreSimdFloat32x4Subtract:
            result->kind = ProjectOopMirNodeKind::CoreSimdFloat32x4Subtract;
            break;
        case ProjectOopHirNodeKind::CoreSimdFloat32x4Multiply:
            result->kind = ProjectOopMirNodeKind::CoreSimdFloat32x4Multiply;
            break;
        case ProjectOopHirNodeKind::CoreSimdInt128Add:
            result->kind = ProjectOopMirNodeKind::CoreSimdInt128Add;
            break;
        case ProjectOopHirNodeKind::CoreSimdInt128Subtract:
            result->kind = ProjectOopMirNodeKind::CoreSimdInt128Subtract;
            break;
        case ProjectOopHirNodeKind::CoreSimdInt128Multiply:
            result->kind = ProjectOopMirNodeKind::CoreSimdInt128Multiply;
            break;
        case ProjectOopHirNodeKind::CoreSimdFloat256Add:
            result->kind = ProjectOopMirNodeKind::CoreSimdFloat256Add;
            break;
        case ProjectOopHirNodeKind::CoreSimdFloat256Subtract:
            result->kind = ProjectOopMirNodeKind::CoreSimdFloat256Subtract;
            break;
        case ProjectOopHirNodeKind::CoreSimdFloat256Multiply:
            result->kind = ProjectOopMirNodeKind::CoreSimdFloat256Multiply;
            break;
        case ProjectOopHirNodeKind::CoreSimdInt256Add:
            result->kind = ProjectOopMirNodeKind::CoreSimdInt256Add;
            break;
        case ProjectOopHirNodeKind::CoreSimdInt256Subtract:
            result->kind = ProjectOopMirNodeKind::CoreSimdInt256Subtract;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup09FirstHalf(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::CoreSimdInt256Multiply:
            result->kind = ProjectOopMirNodeKind::CoreSimdInt256Multiply;
            break;
        case ProjectOopHirNodeKind::CoreSimdLoadAligned128:
            result->kind = ProjectOopMirNodeKind::CoreSimdLoadAligned128;
            break;
        case ProjectOopHirNodeKind::CoreSimdStoreAligned128:
            result->kind = ProjectOopMirNodeKind::CoreSimdStoreAligned128;
            break;
        case ProjectOopHirNodeKind::CoreSimdLoadUnaligned128:
            result->kind = ProjectOopMirNodeKind::CoreSimdLoadUnaligned128;
            break;
        case ProjectOopHirNodeKind::CoreSimdStoreUnaligned128:
            result->kind = ProjectOopMirNodeKind::CoreSimdStoreUnaligned128;
            break;
        case ProjectOopHirNodeKind::CoreHalfFromFloat:
            result->kind = ProjectOopMirNodeKind::CoreHalfFromFloat;
            break;
        case ProjectOopHirNodeKind::CoreHalfToFloat:
            result->kind = ProjectOopMirNodeKind::CoreHalfToFloat;
            break;
        case ProjectOopHirNodeKind::CoreSimdSwizzleWzyx:
            result->kind = ProjectOopMirNodeKind::CoreSimdSwizzleWzyx;
            break;
        case ProjectOopHirNodeKind::CoreSimdShuffleLowHigh:
            result->kind = ProjectOopMirNodeKind::CoreSimdShuffleLowHigh;
            break;
        case ProjectOopHirNodeKind::CoreSimdSqrt:
            result->kind = ProjectOopMirNodeKind::CoreSimdSqrt;
            break;
        case ProjectOopHirNodeKind::CoreSimdReciprocalSqrt:
            result->kind = ProjectOopMirNodeKind::CoreSimdReciprocalSqrt;
            break;
        case ProjectOopHirNodeKind::CoreSimdFusedMultiplyAdd:
            result->kind = ProjectOopMirNodeKind::CoreSimdFusedMultiplyAdd;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup09(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string&)
{
    const int firstHalf = lowerGroup09FirstHalf(node, result);
    if (firstHalf != 0) return firstHalf;
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::CoreBitPopulationCount:
            result->kind = ProjectOopMirNodeKind::CoreBitPopulationCount;
            break;
        case ProjectOopHirNodeKind::CoreBitLeadingZeroCount:
            result->kind = ProjectOopMirNodeKind::CoreBitLeadingZeroCount;
            break;
        case ProjectOopHirNodeKind::CoreBitTrailingZeroCount:
            result->kind = ProjectOopMirNodeKind::CoreBitTrailingZeroCount;
            break;
        case ProjectOopHirNodeKind::CoreBitRotateLeft:
            result->kind = ProjectOopMirNodeKind::CoreBitRotateLeft;
            break;
        case ProjectOopHirNodeKind::CoreBitRotateRight:
            result->kind = ProjectOopMirNodeKind::CoreBitRotateRight;
            break;
        case ProjectOopHirNodeKind::CoreBitByteSwap:
            result->kind = ProjectOopMirNodeKind::CoreBitByteSwap;
            break;
        case ProjectOopHirNodeKind::CoreCpuPrefetchRead:
            result->kind = ProjectOopMirNodeKind::CoreCpuPrefetchRead;
            break;
        case ProjectOopHirNodeKind::CoreCpuPause:
            result->kind = ProjectOopMirNodeKind::CoreCpuPause;
            break;
        case ProjectOopHirNodeKind::CoreCpuReadTimestamp:
            result->kind = ProjectOopMirNodeKind::CoreCpuReadTimestamp;
            break;
        case ProjectOopHirNodeKind::CoreMutexCreate:
            result->kind = ProjectOopMirNodeKind::CoreMutexCreate;
            break;
        case ProjectOopHirNodeKind::CoreMutexCreateNamed:
            result->kind = ProjectOopMirNodeKind::CoreMutexCreateNamed;
            break;
        case ProjectOopHirNodeKind::CoreMutexLock:
            result->kind = ProjectOopMirNodeKind::CoreMutexLock;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup10FirstHalf(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::CoreMutexUnlock:
            result->kind = ProjectOopMirNodeKind::CoreMutexUnlock;
            break;
        case ProjectOopHirNodeKind::CoreMutexClose:
            result->kind = ProjectOopMirNodeKind::CoreMutexClose;
            break;
        // Stage 54.4 "Condition Variables". Enter/Exit/Pulse/PulseAll are trivial passthrough --
        // their single implicit field address is a real FieldAddress CHILD node (built in
        // ProjectOopHirModel.cpp), converted generically by the recursive children loop at the
        // bottom of this function, exactly like CoreInterlockedIncrementInt32 above.
        case ProjectOopHirNodeKind::CoreMonitorEnter:
            result->kind = ProjectOopMirNodeKind::CoreMonitorEnter;
            break;
        case ProjectOopHirNodeKind::CoreMonitorExit:
            result->kind = ProjectOopMirNodeKind::CoreMonitorExit;
            break;
        case ProjectOopHirNodeKind::CoreMonitorPulse:
            result->kind = ProjectOopMirNodeKind::CoreMonitorPulse;
            break;
        case ProjectOopHirNodeKind::CoreMonitorPulseAll:
            result->kind = ProjectOopMirNodeKind::CoreMonitorPulseAll;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerMonitorWait(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    if (!node->memberOrdinal.has_value() || !node->secondaryMemberOrdinal.has_value())
    {
        error = "Stage 54.4 Monitor.Wait HIR node is missing one of its two field ordinals";
        return -1;
    }
    const ObjectFieldLayout* cvLayout = model_.fieldLayout(*node->memberOrdinal);
    const ObjectFieldLayout* lockLayout = model_.fieldLayout(*node->secondaryMemberOrdinal);
    if (cvLayout == nullptr || lockLayout == nullptr)
    {
        error = "Stage 54.4 cannot find Stage 29 field offset for Monitor.Wait's cv_/lock_ fields";
        return -1;
    }
    result->kind = ProjectOopMirNodeKind::CoreMonitorWait;
    result->immediate = cvLayout->offset;
    result->secondaryImmediate = lockLayout->offset;
    return 1;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup10(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    const int firstHalf = lowerGroup10FirstHalf(node, result);
    if (firstHalf != 0) return firstHalf;
    bool handled = true;
    switch (node->kind)
    {
        // CoreMonitorWait is the one case in this round that is NOT trivial passthrough: its
        // memberOrdinal/secondaryMemberOrdinal (copied generically above) hold RAW FIELD ORDINALS
        // (cv_, lock_), not yet resolved byte offsets -- resolve both here, exactly like the
        // ordinary FieldAddress case elsewhere in this function does for its single ordinal, and
        // store them as `immediate`/`secondaryImmediate` for ProjectOopLlvmLowerer.cpp to GEP
        // directly off the single lowered receiver child (see that file's CoreMonitorWait case).
        case ProjectOopHirNodeKind::CoreMonitorWait:
            return lowerMonitorWait(node, result, error);
        // Stage 54.5 "Otras primitivas de sincronizacion". Todas trivial passthrough, mismo
        // shape que CoreMutexCreate/Lock/Unlock/Close arriba -- sin receptor, sin FieldAddress.
        case ProjectOopHirNodeKind::CoreSemaphoreCreate:
            result->kind = ProjectOopMirNodeKind::CoreSemaphoreCreate;
            break;
        case ProjectOopHirNodeKind::CoreSemaphoreCreateNamed:
            result->kind = ProjectOopMirNodeKind::CoreSemaphoreCreateNamed;
            break;
        case ProjectOopHirNodeKind::CoreSemaphoreWait:
            result->kind = ProjectOopMirNodeKind::CoreSemaphoreWait;
            break;
        case ProjectOopHirNodeKind::CoreSemaphoreRelease:
            result->kind = ProjectOopMirNodeKind::CoreSemaphoreRelease;
            break;
        case ProjectOopHirNodeKind::CoreSemaphoreClose:
            result->kind = ProjectOopMirNodeKind::CoreSemaphoreClose;
            break;
        case ProjectOopHirNodeKind::CoreEventCreate:
            result->kind = ProjectOopMirNodeKind::CoreEventCreate;
            break;
        case ProjectOopHirNodeKind::CoreEventCreateNamed:
            result->kind = ProjectOopMirNodeKind::CoreEventCreateNamed;
            break;
        case ProjectOopHirNodeKind::CoreEventSet:
            result->kind = ProjectOopMirNodeKind::CoreEventSet;
            break;
        case ProjectOopHirNodeKind::CoreEventReset:
            result->kind = ProjectOopMirNodeKind::CoreEventReset;
            break;
        case ProjectOopHirNodeKind::CoreEventWait:
            result->kind = ProjectOopMirNodeKind::CoreEventWait;
            break;
        case ProjectOopHirNodeKind::CoreEventClose:
            result->kind = ProjectOopMirNodeKind::CoreEventClose;
            break;
        // Stage 55.1/55.2/55.3 -- trivial passthrough, exactly like the Interlocked/Semaphore/
        // Event cases above: no memberOrdinal/immediate resolution needed, children (if any --
        // only CoreProcessSetPriorityClass/CoreDebugOutputString have one) are copied generically
        // by the loop that calls this function.
        case ProjectOopHirNodeKind::CoreProcessGetCurrentProcessId:
            result->kind = ProjectOopMirNodeKind::CoreProcessGetCurrentProcessId;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup11FirstHalf(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::CoreProcessGetWorkingSetSize:
            result->kind = ProjectOopMirNodeKind::CoreProcessGetWorkingSetSize;
            break;
        case ProjectOopHirNodeKind::CoreProcessGetKernelTime:
            result->kind = ProjectOopMirNodeKind::CoreProcessGetKernelTime;
            break;
        case ProjectOopHirNodeKind::CoreProcessGetUserTime:
            result->kind = ProjectOopMirNodeKind::CoreProcessGetUserTime;
            break;
        case ProjectOopHirNodeKind::CoreProcessGetPriorityClass:
            result->kind = ProjectOopMirNodeKind::CoreProcessGetPriorityClass;
            break;
        case ProjectOopHirNodeKind::CoreProcessSetPriorityClass:
            result->kind = ProjectOopMirNodeKind::CoreProcessSetPriorityClass;
            break;
        case ProjectOopHirNodeKind::CoreSystemInfoGetPageSize:
            result->kind = ProjectOopMirNodeKind::CoreSystemInfoGetPageSize;
            break;
        case ProjectOopHirNodeKind::CoreSystemInfoGetProcessorArchitecture:
            result->kind = ProjectOopMirNodeKind::CoreSystemInfoGetProcessorArchitecture;
            break;
        case ProjectOopHirNodeKind::CoreSystemInfoGetAllocationGranularity:
            result->kind = ProjectOopMirNodeKind::CoreSystemInfoGetAllocationGranularity;
            break;
        case ProjectOopHirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes:
            result->kind = ProjectOopMirNodeKind::CoreSystemInfoGetTotalPhysicalMemoryBytes;
            break;
        case ProjectOopHirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes:
            result->kind = ProjectOopMirNodeKind::CoreSystemInfoGetAvailablePhysicalMemoryBytes;
            break;
        case ProjectOopHirNodeKind::CoreSystemInfoGetMemoryLoadPercent:
            result->kind = ProjectOopMirNodeKind::CoreSystemInfoGetMemoryLoadPercent;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup11(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string&)
{
    const int firstHalf = lowerGroup11FirstHalf(node, result);
    if (firstHalf != 0) return firstHalf;
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::CoreDebugIsDebuggerPresent:
            result->kind = ProjectOopMirNodeKind::CoreDebugIsDebuggerPresent;
            break;
        case ProjectOopHirNodeKind::CoreDebugCaptureStackFrameCount:
            result->kind = ProjectOopMirNodeKind::CoreDebugCaptureStackFrameCount;
            break;
        case ProjectOopHirNodeKind::CoreDebugOutputString:
            result->kind = ProjectOopMirNodeKind::CoreDebugOutputString;
            break;
        case ProjectOopHirNodeKind::CoreThreadSleep:
            result->kind = ProjectOopMirNodeKind::CoreThreadSleep;
            break;
        case ProjectOopHirNodeKind::CoreFileCopy:
            result->kind = ProjectOopMirNodeKind::CoreFileCopy;
            break;
        case ProjectOopHirNodeKind::CoreFileMove:
            result->kind = ProjectOopMirNodeKind::CoreFileMove;
            break;
        case ProjectOopHirNodeKind::CoreDirectoryMove:
            result->kind = ProjectOopMirNodeKind::CoreDirectoryMove;
            break;
        case ProjectOopHirNodeKind::LambdaCreate:
            result->kind = ProjectOopMirNodeKind::LambdaCreate;
            result->calleeName = node->stringValue;
            result->immediate = node->integerValue;
            break;
        case ProjectOopHirNodeKind::ClosureCaptureValue:
            result->kind = ProjectOopMirNodeKind::ClosureCaptureLoad;
            result->immediate = node->integerValue;
            break;
        case ProjectOopHirNodeKind::ClosureCaptureAddress:
            result->kind = ProjectOopMirNodeKind::ClosureCaptureAddress;
            result->immediate = node->integerValue;
            break;
        case ProjectOopHirNodeKind::CaptureCellReference:
            result->kind = ProjectOopMirNodeKind::CaptureCellReference;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup12(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::ClosureCaptureCellReference:
            result->kind = ProjectOopMirNodeKind::ClosureCaptureCellReference;
            result->immediate = node->integerValue;
            break;
        case ProjectOopHirNodeKind::DelegateCreate:
        {
            if (!node->memberOrdinal.has_value() || model_.hirModel_ == nullptr)
            {
                error = "Stage 42 delegate creation has no target callable";
                return -1;
            }
            result->kind = ProjectOopMirNodeKind::DelegateCreate;
            if (node->interfaceDispatch)
            {
                if (!node->targetTypeOrdinal.has_value() || model_.interfaceModel_ == nullptr)
                {
                    error = "T08-F05 interface method value is missing its static interface type";
                    return -1;
                }
                const InterfaceTypeInfo* info = model_.interfaceModel_->interfaceInfo(*node->targetTypeOrdinal);
                if (info == nullptr)
                {
                    error = "T08-F05 method-value receiver is not an interface";
                    return -1;
                }
                const auto found = std::find(
                    info->effectiveMethodMemberOrdinals.begin(),
                    info->effectiveMethodMemberOrdinals.end(),
                    *node->memberOrdinal);
                if (found == info->effectiveMethodMemberOrdinals.end())
                {
                    error = "T08-F05 cannot find the selected interface method-value slot";
                    return -1;
                }
                result->targetTypeOrdinal = node->targetTypeOrdinal;
                result->interfaceSlot = static_cast<std::size_t>(
                    found - info->effectiveMethodMemberOrdinals.begin());
                result->immediate = stableTypeId(
                    model_.interfaceModel_->typeKeyForOrdinal(*node->targetTypeOrdinal));
                break;
            }
            const std::optional<std::size_t> slot = node->booleanValue && model_.virtualDispatchModel_ != nullptr
                ? model_.virtualDispatchModel_->slotForMember(*node->memberOrdinal)
                : std::nullopt;
            if (node->booleanValue && !slot.has_value())
            {
                error = "T08-F05 virtual method value has no vtable slot";
                return -1;
            }
            result->virtualSlot = slot;
            if (!slot.has_value())
            {
                const ProjectOopHirFunction* target = model_.hirModel_->functionForMember(*node->memberOrdinal);
                if (target == nullptr)
                {
                    error = "Stage 42 delegate cannot bind an abstract or body-less direct target";
                    return -1;
                }
                result->calleeName = target->mangledName;
            }
            break;
        }
        case ProjectOopHirNodeKind::DelegateInvoke:
            if (!node->stringValue.empty())
            {
                // T08-F02: the nominal lambda value has no payload. Its generated function is the
                // callee, so this becomes the same direct static call used by an ordinary method.
                result->kind = ProjectOopMirNodeKind::DirectStaticCall;
                result->calleeName = node->stringValue;
            }
            else
            {
                result->kind = ProjectOopMirNodeKind::DelegateInvoke;
            }
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup13(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::EventAdd:
            result->kind = ProjectOopMirNodeKind::EventAdd;
            break;
        case ProjectOopHirNodeKind::EventRemove:
            result->kind = ProjectOopMirNodeKind::EventRemove;
            break;
        case ProjectOopHirNodeKind::EventInvoke:
            result->kind = ProjectOopMirNodeKind::EventInvoke;
            break;
        case ProjectOopHirNodeKind::ArrayAllocation:
        case ProjectOopHirNodeKind::RangeArrayAllocation:
        case ProjectOopHirNodeKind::ArrayElementValue:
        case ProjectOopHirNodeKind::ArrayElementAddress:
        case ProjectOopHirNodeKind::RangeArrayElementValue:
        case ProjectOopHirNodeKind::RangeArrayElementAddress:
        {
            if (model_.nameLookup_ == nullptr || model_.objectLayoutModel_ == nullptr)
            {
                error = "Stage 41 array lowering requires name lookup and value layouts";
                return -1;
            }
            const auto layout = model_.elementLayout(node->elementTypeKey);
            if (!layout.has_value() || layout->first == 0)
            {
                error = "Stage 41 cannot determine native array element layout for '" + node->elementTypeKey + "'";
                return -1;
            }
            result->immediate = layout->first;
            result->alignment = layout->second;
            if (node->kind == ProjectOopHirNodeKind::ArrayAllocation)
                result->kind = ProjectOopMirNodeKind::ArrayAllocate;
            else if (node->kind == ProjectOopHirNodeKind::RangeArrayAllocation)
                result->kind = ProjectOopMirNodeKind::RangeArrayAllocate;
            else if (node->kind == ProjectOopHirNodeKind::ArrayElementValue)
                result->kind = ProjectOopMirNodeKind::ArrayElementLoad;
            else if (node->kind == ProjectOopHirNodeKind::ArrayElementAddress)
                result->kind = ProjectOopMirNodeKind::ArrayElementAddress;
            else if (node->kind == ProjectOopHirNodeKind::RangeArrayElementValue)
                result->kind = ProjectOopMirNodeKind::RangeArrayElementLoad;
            else result->kind = ProjectOopMirNodeKind::RangeArrayElementAddress;
            break;
        }
        case ProjectOopHirNodeKind::NullConditionalArrayLoad:
        {
            if (model_.nameLookup_ == nullptr || model_.objectLayoutModel_ == nullptr)
            {
                error = "Stage 41 null-conditional array lowering requires name lookup and value layouts";
                return -1;
            }
            const auto layout = model_.elementLayout(node->elementTypeKey);
            if (!layout.has_value() || layout->first == 0)
            {
                error = "Stage 41 cannot determine native array element layout for '" +
                    node->elementTypeKey + "'";
                return -1;
            }
            result->kind = ProjectOopMirNodeKind::NullConditionalArrayLoad;
            result->immediate = layout->first;
            result->alignment = layout->second;
            break;
        }
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerNullConditionalArrayStore(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    if (model_.nameLookup_ == nullptr || model_.objectLayoutModel_ == nullptr)
    {
        error = "Stage 41 null-conditional array store requires name lookup and value layouts";
        return -1;
    }
    const auto layout = model_.elementLayout(node->elementTypeKey);
    if (!layout.has_value() || layout->first == 0)
    {
        error = "Stage 41 cannot determine native array element layout for '" +
            node->elementTypeKey + "'";
        return -1;
    }
    result->kind = ProjectOopMirNodeKind::NullConditionalArrayStore;
    result->immediate = layout->first;
    result->alignment = layout->second;
    return 1;
}

int ProjectOopMirModel::MirNodeMachine::lowerRangeSlice(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    if (!node->memberOrdinal.has_value() ||
        !node->secondaryMemberOrdinal.has_value() || model_.hirModel_ == nullptr)
    {
        error = "T01-F04 range slice has no selected Length/Slice protocol methods";
        return -1;
    }
    const ProjectOopHirFunction* slice =
        model_.hirModel_->functionForMember(*node->memberOrdinal);
    const ProjectOopHirFunction* length =
        model_.hirModel_->functionForMember(*node->secondaryMemberOrdinal);
    if (slice == nullptr || length == nullptr || slice->isStatic || length->isStatic)
    {
        error = "T01-F04 range slice protocol methods are not direct instance call targets";
        return -1;
    }
    result->kind = ProjectOopMirNodeKind::RangeSlice;
    result->calleeName = slice->mangledName;
    result->secondaryCalleeName = length->mangledName;
    return 1;
}

int ProjectOopMirModel::MirNodeMachine::lowerCompoundStore(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    result->kind = ProjectOopMirNodeKind::CompoundStore;
    if (!node->elementTypeKey.empty())
    {
        if (model_.nameLookup_ == nullptr || model_.objectLayoutModel_ == nullptr)
        {
            error = "compound pointer update requires name lookup and value layouts";
            return -1;
        }
        const auto layout = model_.elementLayout(node->elementTypeKey);
        if (!layout.has_value() || layout->first == 0)
        {
            error = "compound pointer update cannot determine the element layout of '" +
                node->elementTypeKey + "'";
            return -1;
        }
        result->immediate = layout->first;
        result->alignment = layout->second;
    }
    return 1;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup14(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::NullConditionalArrayStore:
            return lowerNullConditionalArrayStore(node, result, error);
        case ProjectOopHirNodeKind::RangeSlice:
            return lowerRangeSlice(node, result, error);
        case ProjectOopHirNodeKind::Unary: result->kind = ProjectOopMirNodeKind::Unary; break;
        case ProjectOopHirNodeKind::Binary: result->kind = ProjectOopMirNodeKind::Binary; break;
        case ProjectOopHirNodeKind::Assignment: result->kind = ProjectOopMirNodeKind::Store; break;
        case ProjectOopHirNodeKind::TupleDeconstruct:
            result->kind = ProjectOopMirNodeKind::TupleDeconstruct;
            break;
        case ProjectOopHirNodeKind::TupleDeconstructElement:
            result->kind = ProjectOopMirNodeKind::TupleDeconstructElementStore;
            result->immediate = node->integerValue;
            break;
        // IDs 79 y 81. Un destino numerico no necesita paso; un destino puntero si, y lo resuelve
        // el MISMO elementLayout que ya usan PointerArithmetic, el stride de un arreglo y sizeof.
        case ProjectOopHirNodeKind::CompoundStore:
            return lowerCompoundStore(node, result, error);
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup15(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::Conversion: result->kind = ProjectOopMirNodeKind::Convert; break;
        case ProjectOopHirNodeKind::RuntimeTypeTest:
        case ProjectOopHirNodeKind::SafeReferenceCast:
        case ProjectOopHirNodeKind::CheckedReferenceCast:
        {
            if (!node->targetTypeOrdinal.has_value() || model_.nameLookup_ == nullptr)
            {
                error = "Stage 39 RTTI HIR node has no target type";
                return -1;
            }
            result->kind = node->kind == ProjectOopHirNodeKind::RuntimeTypeTest
                ? ProjectOopMirNodeKind::RuntimeTypeTest
                : node->kind == ProjectOopHirNodeKind::SafeReferenceCast
                    ? ProjectOopMirNodeKind::SafeReferenceCast
                    : ProjectOopMirNodeKind::CheckedReferenceCast;
            result->targetIsInterface = node->targetIsInterface;
            result->immediate = stableTypeId(
                "type:" + model_.nameLookup_->namedTypes()[*node->targetTypeOrdinal].identity.key());
            break;
        }
        case ProjectOopHirNodeKind::Deallocation: result->kind = ProjectOopMirNodeKind::Deallocate; break;
        case ProjectOopHirNodeKind::ObjectDestruction: result->kind = ProjectOopMirNodeKind::ObjectDestroy; break;

        case ProjectOopHirNodeKind::StaticFieldValue:
            result->kind = ProjectOopMirNodeKind::StaticFieldLoad;
            break;
        case ProjectOopHirNodeKind::StaticFieldAddress:
            result->kind = ProjectOopMirNodeKind::StaticFieldAddress;
            break;

        case ProjectOopHirNodeKind::NamespaceStateValue:
            result->kind = ProjectOopMirNodeKind::NamespaceStateLoad;
            break;
        case ProjectOopHirNodeKind::NamespaceStateAddress:
            result->kind = ProjectOopMirNodeKind::NamespaceStateAddress;
            break;
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup16(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::FieldValue:
        case ProjectOopHirNodeKind::NullConditionalFieldValue:
        case ProjectOopHirNodeKind::FieldAddress:
        {
            if (!node->memberOrdinal.has_value())
            {
                error = "Stage 34 field HIR node has no member ordinal";
                return -1;
            }
            const ObjectFieldLayout* layout = model_.fieldLayout(*node->memberOrdinal);
            if (layout == nullptr)
            {
                error = "Stage 34 cannot find Stage 29 field offset for member #" +
                    std::to_string(*node->memberOrdinal);
                return -1;
            }
            const bool bitField = layout->bitOffset.has_value() || layout->bitWidth.has_value();
            if (bitField && (!layout->bitOffset.has_value() || !layout->bitWidth.has_value() ||
                             *layout->bitWidth == 0 || layout->size == 0))
            {
                error = "item 329 field layout has incomplete bit-field coordinates";
                return -1;
            }
            result->kind = node->kind == ProjectOopHirNodeKind::NullConditionalFieldValue
                ? ProjectOopMirNodeKind::NullConditionalFieldLoad
                : node->kind == ProjectOopHirNodeKind::FieldValue
                ? (bitField ? ProjectOopMirNodeKind::BitFieldLoad
                            : ProjectOopMirNodeKind::FieldLoad)
                : (bitField ? ProjectOopMirNodeKind::BitFieldAddress
                            : ProjectOopMirNodeKind::FieldAddress);
            result->immediate = layout->offset;
            result->alignment = layout->alignment;
            result->bitOffset = layout->bitOffset;
            result->bitWidth = layout->bitWidth;
            result->bitStorageSize = bitField ? layout->size : 0;
            if (node->kind == ProjectOopHirNodeKind::NullConditionalFieldValue && bitField)
            {
                error = "T01-F03 null-conditional access to a bit field has no nullable value carrier";
                return -1;
            }
            break;
        }

        case ProjectOopHirNodeKind::FutureFieldAddress:
        {
            if (!node->memberOrdinal.has_value() || node->elementTypeKey.empty())
            {
                error = "future field address requires a field index and a future type key";
                return -1;
            }
            const ProjectOopMirStructLayout* layout = nullptr;
            for (const ProjectOopMirStructLayout& candidate : structLayouts_)
            {
                if (candidate.typeKey == node->elementTypeKey)
                {
                    layout = &candidate;
                    break;
                }
            }
            if (layout == nullptr || *node->memberOrdinal >= layout->fields.size())
            {
                error = "future field address has no field layout for '" + node->elementTypeKey +
                    "' index " + std::to_string(*node->memberOrdinal);
                return -1;
            }
            result->kind = ProjectOopMirNodeKind::FutureFieldAddress;
            result->immediate = layout->fields[*node->memberOrdinal].offset;
            result->typeKey = node->typeKey;
            break;
        }

        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerAwaitFuture(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    const ProjectOopMirStructLayout* layout = nullptr;
    for (const ProjectOopMirStructLayout& candidate : structLayouts_)
    {
        if (candidate.typeKey == node->elementTypeKey)
        {
            layout = &candidate;
            break;
        }
    }
    if (layout == nullptr || layout->fields.size() < 3)
    {
        error = "await future has no result field layout for '" + node->elementTypeKey + "'";
        return -1;
    }
    result->kind = ProjectOopMirNodeKind::AwaitFuture;
    result->typeKey = node->typeKey;
    result->elementTypeKey = node->elementTypeKey;
    result->calleeName = node->stringValue;
    result->immediate = layout->fields[2].offset;
    result->integerValue = node->integerValue;
    return 1;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup17(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::AwaitFuture:
            return lowerAwaitFuture(node, result, error);

        case ProjectOopHirNodeKind::SuspendPoint:
            result->kind = ProjectOopMirNodeKind::SuspendPoint;
            result->typeKey = node->typeKey;
            result->integerValue = node->integerValue;
            break;

        case ProjectOopHirNodeKind::PollAddress:
            result->kind = ProjectOopMirNodeKind::PollAddress;
            result->typeKey = node->typeKey;
            result->stringValue = node->stringValue;
            break;

        case ProjectOopHirNodeKind::YieldValue:
            result->kind = ProjectOopMirNodeKind::YieldValue;
            result->typeKey = node->typeKey;
            result->integerValue = node->integerValue;
            break;

        case ProjectOopHirNodeKind::YieldBreak:
            result->kind = ProjectOopMirNodeKind::YieldBreak;
            result->typeKey = node->typeKey;
            break;

        case ProjectOopHirNodeKind::IteratorCurrent:
            result->kind = ProjectOopMirNodeKind::IteratorCurrent;
            result->typeKey = node->typeKey;
            result->elementTypeKey = node->elementTypeKey;
            break;

        case ProjectOopHirNodeKind::IteratorMoveNext:
            result->kind = ProjectOopMirNodeKind::IteratorMoveNext;
            result->typeKey = node->typeKey;
            result->elementTypeKey = node->elementTypeKey;
            break;

        case ProjectOopHirNodeKind::IteratorGetEnumerator:
            result->kind = ProjectOopMirNodeKind::IteratorGetEnumerator;
            result->typeKey = node->typeKey;
            result->elementTypeKey = node->elementTypeKey;
            break;

        case ProjectOopHirNodeKind::IteratorDrop:
            result->kind = ProjectOopMirNodeKind::IteratorDrop;
            result->typeKey = node->typeKey;
            result->elementTypeKey = node->elementTypeKey;
            break;

        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup18(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::IteratorMoveNextAsync:
            result->kind = ProjectOopMirNodeKind::IteratorMoveNextAsync;
            result->typeKey = node->typeKey;
            result->elementTypeKey = node->elementTypeKey;
            break;

        case ProjectOopHirNodeKind::IteratorCancel:
            result->kind = ProjectOopMirNodeKind::IteratorCancel;
            result->typeKey = node->typeKey;
            result->elementTypeKey = node->elementTypeKey;
            break;

        case ProjectOopHirNodeKind::AsyncYieldValue:
            result->kind = ProjectOopMirNodeKind::AsyncYieldValue;
            result->typeKey = node->typeKey;
            result->elementTypeKey = node->elementTypeKey;
            break;

        case ProjectOopHirNodeKind::DropFrame:
            result->kind = ProjectOopMirNodeKind::DropFrame;
            result->typeKey = node->typeKey;
            break;

        case ProjectOopHirNodeKind::NullConditionalFieldStore:
        {
            if (!node->memberOrdinal.has_value())
            {
                error = "Stage 34 null-conditional store HIR node has no member ordinal";
                return -1;
            }
            const ObjectFieldLayout* layout = model_.fieldLayout(*node->memberOrdinal);
            if (layout == nullptr)
            {
                error = "Stage 34 cannot find Stage 29 field offset for member #" +
                    std::to_string(*node->memberOrdinal);
                return -1;
            }
            if (layout->bitOffset.has_value() || layout->bitWidth.has_value())
            {
                error = "T10-F05 null-conditional store to a bit field is not supported";
                return -1;
            }
            result->kind = ProjectOopMirNodeKind::NullConditionalFieldStore;
            result->immediate = layout->offset;
            result->alignment = layout->alignment;
            break;
        }

        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup19(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::InstanceCall:
        case ProjectOopHirNodeKind::ConstructorInitializerCall:
        {
            if (!node->memberOrdinal.has_value() || model_.hirModel_ == nullptr)
            {
                error = "Macro A instance/base call has no selected target method";
                return -1;
            }
            if (node->interfaceDispatch)
            {
                if (!node->targetTypeOrdinal.has_value() || model_.interfaceModel_ == nullptr)
                {
                    error = "Stage 36 interface call is missing its static interface type";
                    return -1;
                }
                const InterfaceTypeInfo* info = model_.interfaceModel_->interfaceInfo(*node->targetTypeOrdinal);
                if (info == nullptr)
                {
                    error = "Stage 36 interface call target is not an interface";
                    return -1;
                }
                const auto found = std::find(
                    info->effectiveMethodMemberOrdinals.begin(),
                    info->effectiveMethodMemberOrdinals.end(),
                    *node->memberOrdinal);
                if (found == info->effectiveMethodMemberOrdinals.end())
                {
                    error = "Stage 36 interface call cannot find the selected contract slot";
                    return -1;
                }
                result->kind = ProjectOopMirNodeKind::InterfaceInstanceCall;
                result->targetTypeOrdinal = node->targetTypeOrdinal;
                result->interfaceSlot = static_cast<std::size_t>(
                    found - info->effectiveMethodMemberOrdinals.begin());
                result->immediate = stableTypeId(
                    model_.interfaceModel_->typeKeyForOrdinal(*node->targetTypeOrdinal));
                break;
            }
            const std::optional<std::size_t> slot = model_.virtualDispatchModel_ != nullptr
                ? model_.virtualDispatchModel_->slotForMember(*node->memberOrdinal)
                : std::nullopt;
            const bool dynamicDispatch =
                node->kind == ProjectOopHirNodeKind::InstanceCall && !node->baseReceiver && slot.has_value();
            const ProjectOopHirFunction* target = model_.hirModel_->functionForMember(*node->memberOrdinal);
            if (!dynamicDispatch && target == nullptr)
            {
                error = "Stage 35 direct/base call cannot target an abstract method";
                return -1;
            }
            result->kind = dynamicDispatch
                ? ProjectOopMirNodeKind::VirtualInstanceCall
                : ProjectOopMirNodeKind::DirectInstanceCall;
            if (target != nullptr)
            {
                result->calleeName = target->mangledName;
            }
            result->virtualSlot = dynamicDispatch ? slot : std::nullopt;
            break;
        }

        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup20(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::StaticCall:
        {
            if (model_.hirModel_ == nullptr)
            {
                error = "Stage 37 static call has no HIR model";
                return -1;
            }
            const ProjectOopHirFunction* target = nullptr;
            if (node->namespaceFunctionOrdinal.has_value())
            {
                target = model_.hirModel_->functionForNamespaceFunction(
                    *node->namespaceFunctionOrdinal);
            }
            else if (node->memberOrdinal.has_value())
            {
                target = model_.hirModel_->functionForMember(*node->memberOrdinal);
            }
            else
            {
                error = "Stage 37 static call has no selected target";
                return -1;
            }
            if (target == nullptr || !target->isStatic)
            {
                error = "Stage 37 static call target is missing or is not static";
                return -1;
            }
            result->kind = ProjectOopMirNodeKind::DirectStaticCall;
            result->calleeName = target->mangledName;
            break;
        }
        case ProjectOopHirNodeKind::TemporaryAddress:
            result->kind = ProjectOopMirNodeKind::TemporaryAddress;
            break;
        case ProjectOopHirNodeKind::AggregateInitializer:
        {
            // C7 "aggregate initializer": the storage the elements are written into. Its size and
            // alignment come from the same value layout every other consumer of this type reads,
            // so `V v = {1, 2};` occupies exactly the bytes a C host sees.
            if (model_.nameLookup_ == nullptr || model_.objectLayoutModel_ == nullptr)
            {
                error = "C7 aggregate initializer requires name lookup and value layouts";
                return -1;
            }
            const auto layout = model_.elementLayout(node->typeKey);
            if (!layout.has_value() || layout->first == 0)
            {
                error = "C7 cannot determine the layout of aggregate '" + node->typeKey + "'";
                return -1;
            }
            result->kind = ProjectOopMirNodeKind::AggregateInitialize;
            result->immediate = layout->first;
            result->alignment = layout->second;
            result->zeroInitialize = true;
            break;
        }
        case ProjectOopHirNodeKind::AggregateSpread:
            result->kind = ProjectOopMirNodeKind::AggregateSpreadStore;
            break;
        case ProjectOopHirNodeKind::AggregateElement:
        {
            result->kind = ProjectOopMirNodeKind::AggregateElementStore;
            result->immediate = node->integerValue;
            if (node->bitWidth.has_value()) result->alignment = node->bitStorageAlignment;
            // C8: the element's WIDTH IN BYTES, resolved here rather than in the backend, because
            // this is where the value layouts live. It is what lets the struct root members read
            // every leaf -- a float, a reference, an enum -- as a plain integer of the right size,
            // with no cast instructions at all. (C7's initializer elements carry it too and simply
            // do not use it: one rule for what an aggregate element is, not two.)
            if (model_.nameLookup_ != nullptr && model_.objectLayoutModel_ != nullptr)
            {
                const auto leafLayout = model_.elementLayout(node->typeKey);
                if (leafLayout.has_value())
                {
                    // Root-member descriptors have no value child, while initializer elements
                    // do. Both still need the scalar byte width in secondaryImmediate because
                    // the LLVM equality/hash reader consumes that field. Keep bitStorageSize as
                    // well for childless descriptors used by the aggregate metadata path.
                    result->secondaryImmediate = leafLayout->first;
                    if (node->children.empty()) result->bitStorageSize = leafLayout->first;
                }
            }
            break;
        }
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup21(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::ParameterPack:
        {
            if (model_.nameLookup_ == nullptr || model_.memberModel_ == nullptr || model_.objectLayoutModel_ == nullptr)
            {
                error = "T01-F02 parameter pack requires name, member, and layout models";
                return -1;
            }
            const auto element = model_.elementLayout(node->elementTypeKey);
            const auto span = model_.elementLayout(node->typeKey);
            if (!element.has_value() || element->first == 0 || !span.has_value() || span->first == 0)
            {
                error = "T01-F02 cannot determine parameter-pack value layouts";
                return -1;
            }
            std::optional<std::size_t> spanTypeOrdinal;
            if (node->typeKey.starts_with("type:"))
            {
                const std::string_view identity(node->typeKey.data() + 5U, node->typeKey.size() - 5U);
                for (std::size_t ordinal = 0; ordinal < model_.nameLookup_->namedTypes().size(); ++ordinal)
                {
                    if (model_.nameLookup_->namedTypes()[ordinal].identity.key() == identity)
                    {
                        spanTypeOrdinal = ordinal;
                        break;
                    }
                }
            }
            const StructValueLayout* spanLayout = spanTypeOrdinal.has_value()
                ? model_.objectLayoutModel_->structLayoutForType(*spanTypeOrdinal) : nullptr;
            if (spanLayout == nullptr || spanLayout->fields.size() < 2U ||
                spanLayout->fields[0].typeKey != node->elementTypeKey + "*" ||
                spanLayout->fields[1].typeKey != "builtin:int")
            {
                error = "T01-F02 ReadOnlySpan<T> layout requires leading T* data and int length fields";
                return -1;
            }
            result->kind = ProjectOopMirNodeKind::ParameterPack;
            result->elementTypeKey = node->elementTypeKey;
            result->immediate = element->first;
            result->alignment = element->second;
            result->secondaryImmediate = span->first;
            result->integerValue = spanLayout->fields[0].offset;
            result->bitStorageSize = spanLayout->fields[1].offset;
            break;
        }
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup22(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::ValueConstruction:
        {
            // C6 "struct members": a struct has no allocation plan -- there is nothing to
            // allocate. Its size and alignment come straight from the value layout the object
            // layout model already computed for it, which is the same layout it crosses the C
            // boundary with.
            if (!node->targetTypeOrdinal.has_value() || model_.objectLayoutModel_ == nullptr)
            {
                error = "C6 struct construction has no target type ordinal";
                return -1;
            }
            const StructValueLayout* layout =
                model_.objectLayoutModel_->structLayoutForType(*node->targetTypeOrdinal);
            if (layout == nullptr)
            {
                error = "C6 struct construction has no value layout";
                return -1;
            }
            result->kind = ProjectOopMirNodeKind::ValueConstruct;
            result->immediate = layout->size;
            result->alignment = layout->alignment;
            result->zeroInitialize = true;
            if (node->memberOrdinal.has_value())
            {
                const ProjectOopHirFunction* constructor =
                    model_.hirModel_->functionForMember(*node->memberOrdinal);
                if (constructor == nullptr)
                {
                    error = "C6 struct constructor has no OOP HIR function";
                    return -1;
                }
                result->calleeName = constructor->mangledName;
                result->memberOrdinal = node->memberOrdinal;
    result->namespaceFunctionOrdinal = node->namespaceFunctionOrdinal;
            }
            break;
        }
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup23(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::PlacementConstruction:
        {
            if (!node->targetTypeOrdinal.has_value() || model_.objectLayoutModel_ == nullptr ||
                model_.nameLookup_ == nullptr || *node->targetTypeOrdinal >= model_.nameLookup_->namedTypes().size())
            {
                error = "item 318 placement construction has no target layout identity";
                return -1;
            }
            const NamedTypeDeclaration& target =
                model_.nameLookup_->namedTypes()[*node->targetTypeOrdinal];
            if (target.kind == NamedTypeDeclarationKind::Struct)
            {
                const StructValueLayout* layout =
                    model_.objectLayoutModel_->structLayoutForType(*node->targetTypeOrdinal);
                if (layout == nullptr)
                {
                    error = "item 318 placement struct construction has no value layout";
                    return -1;
                }
                result->immediate = layout->size;
                result->alignment = layout->alignment;
            }
            else
            {
                const ClassObjectLayout* layout =
                    model_.objectLayoutModel_->layoutForType(*node->targetTypeOrdinal);
                if (layout == nullptr)
                {
                    error = "item 318 placement class construction has no object layout";
                    return -1;
                }
                result->immediate = layout->size;
                result->alignment = layout->alignment;
            }
            result->kind = ProjectOopMirNodeKind::PlacementConstruct;
            result->zeroInitialize = true;
            if (node->memberOrdinal.has_value())
            {
                const ProjectOopHirFunction* constructor =
                    model_.hirModel_->functionForMember(*node->memberOrdinal);
                if (constructor == nullptr)
                {
                    error = "item 318 placement constructor has no OOP HIR function";
                    return -1;
                }
                result->calleeName = constructor->mangledName;
                result->memberOrdinal = node->memberOrdinal;
    result->namespaceFunctionOrdinal = node->namespaceFunctionOrdinal;
            }
            break;
        }
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerGroup24(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    bool handled = true;
    switch (node->kind)
    {
        case ProjectOopHirNodeKind::ObjectConstruction:
        {
            if (!node->constructionOrdinal.has_value())
            {
                error = "Stage 34 object construction has no construction ordinal";
                return -1;
            }
            const ObjectAllocationPlan* plan = model_.allocationPlan(*node->constructionOrdinal);
            if (plan == nullptr)
            {
                error = "Stage 34 cannot find Stage 29 allocation plan";
                return -1;
            }
            result->kind = ProjectOopMirNodeKind::AllocateAndConstruct;
            result->immediate = plan->objectSize;
            result->alignment = plan->objectAlignment;
            result->allocationFailureReturnsNull = plan->allocationFailureReturnsNull;
            result->zeroInitialize = plan->zeroInitializesStorage;
            if (plan->constructorEntryMemberOrdinal.has_value())
            {
                const ProjectOopHirFunction* constructor = model_.hirModel_->functionForMember(
                    *plan->constructorEntryMemberOrdinal);
                if (constructor == nullptr)
                {
                    error = "Macro A constructor entry has no OOP HIR function";
                    return -1;
                }
                result->calleeName = constructor->mangledName;
                result->memberOrdinal = plan->constructorEntryMemberOrdinal;
            }
            break;
        }
        default:
            handled = false;
            break;
    }
    return handled ? 1 : 0;
}

int ProjectOopMirModel::MirNodeMachine::lowerKindGroups01To12(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    int status = lowerGroup01(node, result, error);
    if (status != 0) return status;
    status = lowerGroup02(node, result, error);
    if (status != 0) return status;
    status = lowerGroup03(node, result, error);
    if (status != 0) return status;
    status = lowerGroup04(node, result, error);
    if (status != 0) return status;
    status = lowerGroup05(node, result, error);
    if (status != 0) return status;
    status = lowerGroup06(node, result, error);
    if (status != 0) return status;
    status = lowerGroup07(node, result, error);
    if (status != 0) return status;
    status = lowerGroup08(node, result, error);
    if (status != 0) return status;
    status = lowerGroup09(node, result, error);
    if (status != 0) return status;
    status = lowerGroup10(node, result, error);
    if (status != 0) return status;
    status = lowerGroup11(node, result, error);
    if (status != 0) return status;
    status = lowerGroup12(node, result, error);
    return status;
}

int ProjectOopMirModel::MirNodeMachine::lowerKindGroups13To24(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    int status = lowerGroup13(node, result, error);
    if (status != 0) return status;
    status = lowerGroup14(node, result, error);
    if (status != 0) return status;
    status = lowerGroup15(node, result, error);
    if (status != 0) return status;
    status = lowerGroup16(node, result, error);
    if (status != 0) return status;
    status = lowerGroup17(node, result, error);
    if (status != 0) return status;
    status = lowerGroup18(node, result, error);
    if (status != 0) return status;
    status = lowerGroup19(node, result, error);
    if (status != 0) return status;
    status = lowerGroup20(node, result, error);
    if (status != 0) return status;
    status = lowerGroup21(node, result, error);
    if (status != 0) return status;
    status = lowerGroup22(node, result, error);
    if (status != 0) return status;
    status = lowerGroup23(node, result, error);
    if (status != 0) return status;
    status = lowerGroup24(node, result, error);
    return status;
}

bool ProjectOopMirModel::MirNodeMachine::lowerKind(
    const ProjectOopHirNode* node,
    ProjectOopMirNode* result,
    std::string& error)
{
    int status = lowerKindGroups01To12(node, result, error);
    if (status != 0) return status > 0;
    status = lowerKindGroups13To24(node, result, error);
    if (status != 0) return status > 0;
    return true;
}

std::unique_ptr<ProjectOopMirNode> ProjectOopMirModel::MirNodeMachine::lower(
    const ProjectOopHirNode* node,
    std::string& error)
{
    if (node == nullptr) return nullptr;
    const auto makeNode = [&](const ProjectOopHirNode* source)
    {
        auto result = std::make_unique<ProjectOopMirNode>();
        copyIdentity(source, result.get());
        copyControlState(source, result.get());
        copyAttributes(source, result.get());
        if (!lowerKind(source, result.get(), error)) result.reset();
        return result;
    };
    std::unique_ptr<ProjectOopMirNode> root = makeNode(node);
    if (root == nullptr) return nullptr;
    std::vector<const ProjectOopHirNode*> sources{node};
    std::vector<std::unique_ptr<ProjectOopMirNode>> results;
    results.push_back(std::move(root));
    std::vector<std::size_t> childIndices{0U};
    while (!sources.empty())
    {
        const std::size_t frame = sources.size() - 1U;
        const ProjectOopHirNode* source = sources[frame];
        if (childIndices[frame] < source->children.size())
        {
            const auto& child = source->children[childIndices[frame]++];
            if (child == nullptr)
            {
                results[frame]->children.push_back(nullptr);
                continue;
            }
            std::unique_ptr<ProjectOopMirNode> lowered = makeNode(child.get());
            if (lowered == nullptr) return nullptr;
            sources.push_back(child.get());
            results.push_back(std::move(lowered));
            childIndices.push_back(0U);
            continue;
        }
        const ProjectOopHirNode* finishedSource = sources.back();
        std::unique_ptr<ProjectOopMirNode> finished = std::move(results.back());
        sources.pop_back();
        results.pop_back();
        childIndices.pop_back();
        if (sources.empty()) return finished;
        if (sources.back()->kind == ProjectOopHirNodeKind::AggregateSpread &&
            finishedSource->kind == ProjectOopHirNodeKind::AggregateElement &&
            finishedSource->children.empty())
            finished->secondaryImmediate = finishedSource->secondaryIntegerValue;
        results.back()->children.push_back(std::move(finished));
    }
    return nullptr;
}

}
