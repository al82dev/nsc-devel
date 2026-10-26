#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Frontend/ProjectCompilationIndexes.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "../Internal/IndexShadowCheckSwitch.h"
#include "../Internal/IndexShadowMismatch.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
using PositionMap = std::unordered_map<std::size_t, std::uint64_t>;
using OrdinalSet = std::unordered_set<std::size_t>;

void collectExceptionFlow(
    ProjectOopMirFunction& function,
    const ProjectOopMirNode& node,
    language::PanicProfile panicProfile)
{
    switch (node.kind)
    {
        case ProjectOopMirNodeKind::TryRegion:
            if (node.mirRegionOrdinal.has_value())
                function.exceptionFlow.protectedRegionOrdinals.push_back(*node.mirRegionOrdinal);
            break;
        case ProjectOopMirNodeKind::CatchRegion:
            ++function.exceptionFlow.catchRegionCount;
            function.exceptionFlow.requiresPersonality = true;
            break;
        case ProjectOopMirNodeKind::FinallyRegion:
            ++function.exceptionFlow.finallyRegionCount;
            function.exceptionFlow.requiresPersonality = true;
            break;
        case ProjectOopMirNodeKind::Throw:
        case ProjectOopMirNodeKind::Rethrow:
            function.exceptionFlow.requiresPersonality = true;
            break;
        case ProjectOopMirNodeKind::Panic:
            if (panicProfile == language::PanicProfile::NativeUnwind)
                function.exceptionFlow.requiresPersonality = true;
            break;
        default: break;
    }
}

std::optional<ProjectOopMirNonlocalExitKind> nonlocalExitKind(
    ProjectOopMirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::Return:
        case ProjectOopMirNodeKind::ResultPropagation:
            return ProjectOopMirNonlocalExitKind::Return;
        case ProjectOopMirNodeKind::Break: return ProjectOopMirNonlocalExitKind::Break;
        case ProjectOopMirNodeKind::Continue: return ProjectOopMirNonlocalExitKind::Continue;
        case ProjectOopMirNodeKind::Throw: return ProjectOopMirNonlocalExitKind::Throw;
        case ProjectOopMirNodeKind::Rethrow: return ProjectOopMirNonlocalExitKind::Rethrow;
        case ProjectOopMirNodeKind::Panic: return ProjectOopMirNonlocalExitKind::Panic;
        case ProjectOopMirNodeKind::CleanupContinuation:
            return ProjectOopMirNonlocalExitKind::CleanupContinuation;
        default: return std::nullopt;
    }
}

std::optional<ProjectOopMirCleanupKind> cleanupKind(ProjectOopMirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::LocalDeclaration:
            return ProjectOopMirCleanupKind::LocalOwner;
        case ProjectOopMirNodeKind::OwnedTemporaryBorrow:
            return ProjectOopMirCleanupKind::TemporaryOwner;
        case ProjectOopMirNodeKind::OwnedParameterCleanup:
            return ProjectOopMirCleanupKind::OwnedParameter;
        case ProjectOopMirNodeKind::UsingRegion:
            return ProjectOopMirCleanupKind::Using;
        case ProjectOopMirNodeKind::FinallyRegion:
            return ProjectOopMirCleanupKind::Finally;
        default: return std::nullopt;
    }
}

bool appendCleanupRegion(
    ProjectOopMirFunction& function,
    const ProjectOopMirNode& node,
    std::string& error)
{
    if (!node.cleanupRegionOrdinal.has_value()) return true;
    const std::optional<ProjectOopMirCleanupKind> kind = cleanupKind(node.kind);
    if (!kind.has_value()) return true;
    const std::size_t ordinal = *node.cleanupRegionOrdinal;
    const bool duplicate = std::ranges::any_of(
        function.cleanupRegions,
        [ordinal](const ProjectOopMirCleanupRegion& region) {
            return region.ordinal == ordinal;
        });
    if (duplicate)
    {
        error = "OOP MIR contains duplicate cleanup-region identity #" +
            std::to_string(ordinal);
        return false;
    }
    function.cleanupRegions.push_back(ProjectOopMirCleanupRegion{
        ordinal, *kind, &node, node.localOrdinal, node.parameterOrdinal, node.typeKey,
        node.ownership, node.automaticDestruction});
    return true;
}

bool collectCleanupContractNodes(
    ProjectOopMirFunction& function,
    const ProjectOopMirNode* root,
    language::PanicProfile panicProfile,
    std::string& error)
{
    std::vector<const ProjectOopMirNode*> work{root};
    while (!work.empty())
    {
        const ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        collectExceptionFlow(function, *node, panicProfile);
        const std::optional<ProjectOopMirNonlocalExitKind> exitKind = nonlocalExitKind(node->kind);
        if (exitKind.has_value())
        {
            function.exceptionFlow.nonlocalExits.push_back(ProjectOopMirNonlocalExit{
                *exitKind, node, node->cleanupRegionOrdinals, node->targetScopeOrdinal,
                node->exceptionTargetRegionOrdinal, node->cleanupContinuationKind});
        }
        if (!appendCleanupRegion(function, *node, error)) return false;
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
    return true;
}

bool validateCleanupReferenceNodes(
    const ProjectOopMirFunction& function,
    const ProjectOopMirNode* root,
    std::string& error)
{
    std::vector<const ProjectOopMirNode*> work{root};
    while (!work.empty())
    {
        const ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        for (const std::size_t ordinal : node->cleanupRegionOrdinals)
        {
            const bool exists = std::ranges::any_of(
                function.cleanupRegions,
                [ordinal](const ProjectOopMirCleanupRegion& region) {
                    return region.ordinal == ordinal;
                });
            if (exists) continue;
            error = "OOP MIR structured exit references missing cleanup region #" +
                std::to_string(ordinal);
            return false;
        }
        if (node->exceptionTargetRegionOrdinal.has_value() &&
            !std::ranges::binary_search(
                function.exceptionFlow.protectedRegionOrdinals,
                *node->exceptionTargetRegionOrdinal))
        {
            error = "OOP MIR nonlocal exit references missing exception region #" +
                std::to_string(*node->exceptionTargetRegionOrdinal);
            return false;
        }
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
    return true;
}

bool finalizeCleanupContract(
    ProjectOopMirFunction& function,
    language::PanicProfile panicProfile,
    std::string& error)
{
    if (!collectCleanupContractNodes(function, function.body.get(), panicProfile, error)) return false;
    std::ranges::sort(function.cleanupRegions, {}, &ProjectOopMirCleanupRegion::ordinal);
    std::ranges::sort(function.exceptionFlow.protectedRegionOrdinals);
    if (std::ranges::adjacent_find(function.exceptionFlow.protectedRegionOrdinals) !=
        function.exceptionFlow.protectedRegionOrdinals.end())
    {
        error = "OOP MIR contains duplicate exception-region identity";
        return false;
    }
    function.exceptionFlow.requiresPersonality =
        function.exceptionFlow.requiresPersonality || !function.cleanupRegions.empty();
    return validateCleanupReferenceNodes(function, function.body.get(), error);
}

void copyFunctionMetadata(
    ProjectOopMirFunction& function,
    const ProjectOopHirFunction& source)
{
    function.memberOrdinal = source.memberOrdinal;
    function.namespaceFunctionOrdinal = source.namespaceFunctionOrdinal;
    function.declaringTypeOrdinal = source.declaringTypeOrdinal;
    function.constructor = source.constructor;
    function.destructor = source.destructor;
    function.isStatic = source.isStatic;
    function.referenceReturn = source.referenceReturn;
    function.isNoHeap = source.isNoHeap;
    function.effects = source.effects;
    function.inliningPolicy = source.inliningPolicy;
    function.isExportedC = source.isExportedC;
    function.isImportedC = source.isImportedC;
    function.isVariadic = source.isVariadic;
    function.isImportedNative = source.isImportedNative;
    function.isTransportedInlineBody = source.isTransportedInlineBody;
    function.isSyntheticImplicitConstructor = source.isSyntheticImplicitConstructor;
    function.importDllName = source.importDllName;
    function.importRequiresRuntimeBinding = source.importRequiresRuntimeBinding;
    function.cAbiName = source.cAbiName;
    function.isLambda = source.isLambda;
    function.lambdaOrdinal = source.lambdaOrdinal;
    function.hasClosureEnvironment = source.hasClosureEnvironment;
    function.captureTypeKeys = source.captureTypeKeys;
    function.captureNames = source.captureNames;
    function.captureSourceKinds = source.captureSourceKinds;
    function.captureModes = source.captureModes;
    function.captureOwnsValues = source.captureOwnsValues;
    function.callableTrait = source.callableTrait;
    function.closureLayoutSize = source.closureLayoutSize;
    function.closureLayoutAlignment = source.closureLayoutAlignment;
    function.capturedLocalOrdinals = source.capturedLocalOrdinals;
    function.capturedParameterOrdinals = source.capturedParameterOrdinals;
    function.capturesThisCell = source.capturesThisCell;
    function.genericSpecializationKey = source.genericSpecializationKey;
    function.mangledName = source.mangledName;
    function.debugDisplayName = source.debugDisplayName;
    function.returnTypeKey = source.returnTypeKey;
    function.errorTypeKey = source.errorTypeKey;
    function.frameStorage = source.frameStorage;
    function.isGeneratorConstructor = source.isGeneratorConstructor;
    function.generatorParameterTypeKeys = source.generatorParameterTypeKeys;
    function.returnOwnership = source.returnOwnership;
    function.parameterNames = source.parameterNames;
    function.parameterTypeKeys = source.parameterTypeKeys;
    function.parameterPassKinds = source.parameterPassKinds;
    function.parameterOwnerships = source.parameterOwnerships;
    function.localOrdinals = source.localOrdinals;
}

/// Struct layout of typeKey among the function-lowering layouts. When the shared MIR layout
/// index is built it was built from exactly these layouts (importLayouts appends every layout
/// before rebuildMirLayouts) and keeps the first layout of a key, so it answers like the scan;
/// without it the scan runs as before.
const ProjectOopMirStructLayout* functionStructLayout(
    const ProjectCompilationIndexes* indexes,
    std::span<const ProjectOopMirStructLayout> layouts,
    std::string_view typeKey) noexcept
{
    if (indexes != nullptr && indexes->hasMirLayoutIndexes())
    {
        const ProjectOopMirStructLayout* indexed = indexes->mirStructLayoutForTypeKey(typeKey);
        if constexpr (internal::kIndexShadowCheckEnabled)
        {
            const ProjectOopMirStructLayout* scanned = nullptr;
            for (const ProjectOopMirStructLayout& layout : layouts)
            {
                if (layout.typeKey != typeKey) continue;
                scanned = &layout;
                break;
            }
            if (scanned != indexed) internal::reportIndexShadowMismatch("functionStructLayout", typeKey);
        }
        return indexed;
    }
    for (const ProjectOopMirStructLayout& layout : layouts)
        if (layout.typeKey == typeKey) return &layout;
    return nullptr;
}

ProjectOopMirAbiPassing abiPassingForType(
    const ProjectCompilationIndexes* indexes,
    std::span<const ProjectOopMirStructLayout> layouts,
    std::string_view typeKey) noexcept
{
    const ProjectOopMirStructLayout* layout = functionStructLayout(indexes, layouts, typeKey);
    return layout != nullptr ? layout->abiPassing : ProjectOopMirAbiPassing::Direct;
}

void configureFunctionAbi(
    ProjectOopMirFunction& function,
    const ProjectCompilationIndexes* indexes,
    std::span<const ProjectOopMirStructLayout> layouts)
{
    function.abi.returnPassing = function.referenceReturn
        ? ProjectOopMirAbiPassing::Direct
        : abiPassingForType(indexes, layouts, function.returnTypeKey);
    std::size_t physicalOrdinal = function.isStatic ? 0U : 1U;
    if (function.abi.returnPassing == ProjectOopMirAbiPassing::Indirect)
        function.abi.hiddenReturnParameterOrdinal = physicalOrdinal++;
    for (const std::string& parameterTypeKey : function.parameterTypeKeys)
    {
        function.abi.parameterPassing.push_back(
            abiPassingForType(indexes, layouts, parameterTypeKey));
        function.abi.parameterPhysicalOrdinals.push_back(physicalOrdinal++);
    }
    function.abi.physicalParameterCount = physicalOrdinal;
}

std::uint64_t alignFrameOffset(std::uint64_t value, std::uint64_t alignment) noexcept
{
    if (alignment <= 1) return value;
    return (value + alignment - 1) / alignment * alignment;
}

void sumAwaitedFrameSizeNodes(
    const ProjectOopMirNode* root,
    const ProjectCompilationIndexes* indexes,
    std::span<const ProjectOopMirStructLayout> layouts,
    std::uint64_t& frameSize)
{
    std::vector<const ProjectOopMirNode*> work{root};
    while (!work.empty())
    {
        const ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        if (node->kind == ProjectOopMirNodeKind::AwaitFuture)
        {
            if (const ProjectOopMirStructLayout* layout =
                    functionStructLayout(indexes, layouts, node->elementTypeKey))
                frameSize += layout->size;
        }
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
}

void collectPollLivenessNodes(
    const ProjectOopMirNode* root,
    std::uint64_t& order,
    PositionMap& localDefinition,
    PositionMap& localLastUse,
    PositionMap& parameterLastUse,
    std::vector<std::uint64_t>& suspendOrders)
{
    std::vector<const ProjectOopMirNode*> work{root};
    while (!work.empty())
    {
        const ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        const std::uint64_t here = order++;
        if (node->kind == ProjectOopMirNodeKind::LocalDeclaration &&
            node->localOrdinal.has_value())
        {
            if (!localDefinition.contains(*node->localOrdinal))
                localDefinition[*node->localOrdinal] = here;
        }
        else if ((node->kind == ProjectOopMirNodeKind::LocalValue ||
                  node->kind == ProjectOopMirNodeKind::LocalAddress) &&
            node->localOrdinal.has_value())
            localLastUse[*node->localOrdinal] = here;
        else if (node->kind == ProjectOopMirNodeKind::ParameterValue &&
            node->parameterOrdinal.has_value())
            parameterLastUse[*node->parameterOrdinal] = here;
        else if (node->kind == ProjectOopMirNodeKind::SuspendPoint ||
                 node->kind == ProjectOopMirNodeKind::AsyncYieldValue)
            suspendOrders.push_back(here);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
}

void collectMoveNextLivenessNodes(
    const ProjectOopMirNode* root,
    std::uint64_t& order,
    PositionMap& localDefinition,
    PositionMap& localLastUse,
    PositionMap& parameterLastUse,
    std::vector<std::uint64_t>& yieldOrders)
{
    std::vector<const ProjectOopMirNode*> work{root};
    while (!work.empty())
    {
        const ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        const std::uint64_t here = order++;
        if (node->kind == ProjectOopMirNodeKind::LocalDeclaration &&
            node->localOrdinal.has_value())
        {
            if (!localDefinition.contains(*node->localOrdinal))
                localDefinition[*node->localOrdinal] = here;
        }
        else if ((node->kind == ProjectOopMirNodeKind::LocalValue ||
                  node->kind == ProjectOopMirNodeKind::LocalAddress) &&
            node->localOrdinal.has_value())
            localLastUse[*node->localOrdinal] = here;
        else if (node->kind == ProjectOopMirNodeKind::ParameterValue &&
            node->parameterOrdinal.has_value())
            parameterLastUse[*node->parameterOrdinal] = here;
        else if (node->kind == ProjectOopMirNodeKind::YieldValue)
            yieldOrders.push_back(here);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
}

bool liveAcrossBoundary(
    std::size_t localOrdinal,
    const PositionMap& localDefinition,
    const PositionMap& localLastUse,
    std::span<const std::uint64_t> boundaries) noexcept
{
    const auto definition = localDefinition.find(localOrdinal);
    const auto lastUse = localLastUse.find(localOrdinal);
    if (definition == localDefinition.end() || lastUse == localLastUse.end()) return false;
    for (const std::uint64_t boundary : boundaries)
        if (definition->second < boundary && boundary < lastUse->second) return true;
    return false;
}

void collectBorrowSourcesNodes(
    const ProjectOopMirNode* root,
    std::unordered_map<std::size_t, std::size_t>& borrowSource,
    OrdinalSet& borrowNonLocalSource)
{
    std::vector<const ProjectOopMirNode*> work{root};
    while (!work.empty())
    {
        const ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        if (node->kind == ProjectOopMirNodeKind::LocalDeclaration &&
            node->localOrdinal.has_value() && !node->children.empty())
        {
            const ProjectOopMirNode* initializer = node->children[0].get();
            if (initializer != nullptr &&
                initializer->kind == ProjectOopMirNodeKind::BorrowValue)
            {
                if (initializer->localOrdinal.has_value())
                    borrowSource[*node->localOrdinal] = *initializer->localOrdinal;
                else
                    borrowNonLocalSource.insert(*node->localOrdinal);
            }
        }
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
}

void collectLivePollLocalNodes(
    const ProjectOopMirNode* root,
    const PositionMap& localDefinition,
    const PositionMap& localLastUse,
    std::span<const std::uint64_t> suspendOrders,
    OrdinalSet& liveSet)
{
    std::vector<const ProjectOopMirNode*> work{root};
    while (!work.empty())
    {
        const ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        if (node->kind == ProjectOopMirNodeKind::LocalDeclaration &&
            node->localOrdinal.has_value() &&
            liveAcrossBoundary(
                *node->localOrdinal, localDefinition, localLastUse, suspendOrders))
            liveSet.insert(*node->localOrdinal);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
}

bool validateNonLocalBorrowSources(
    const OrdinalSet& borrowNonLocalSource,
    const PositionMap& localDefinition,
    const PositionMap& localLastUse,
    std::span<const std::uint64_t> suspendOrders,
    std::string& error)
{
    for (const std::size_t borrowedLocal : borrowNonLocalSource)
    {
        if (!liveAcrossBoundary(
                borrowedLocal, localDefinition, localLastUse, suspendOrders))
            continue;
        error = "async function cannot hold a borrowed view of a parameter or "
                "field across a suspension (NS304-T05-F05)";
        return false;
    }
    return true;
}

void appendPollFrameLocalNodes(
    const ProjectOopMirNode* root,
    const OrdinalSet& liveSet,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup,
    ProjectOopMirFunction& function,
    std::uint64_t& frameSize)
{
    std::vector<const ProjectOopMirNode*> work{root};
    while (!work.empty())
    {
        const ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        if (node->kind == ProjectOopMirNodeKind::LocalDeclaration &&
            node->localOrdinal.has_value() && !node->typeKey.empty() &&
            liveSet.contains(*node->localOrdinal))
        {
            const auto layout = objectLayoutModel.valueLayoutForTypeKey(nameLookup, node->typeKey);
            if (layout.has_value())
            {
                frameSize = alignFrameOffset(frameSize, layout->second);
                function.asyncFrameLocals.emplace_back(
                    *node->localOrdinal, frameSize, node->typeKey);
                frameSize += layout->first;
            }
        }
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
}

void appendMoveNextFrameLocalNodes(
    const ProjectOopMirNode* root,
    const PositionMap& localDefinition,
    const PositionMap& localLastUse,
    std::span<const std::uint64_t> yieldOrders,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup,
    ProjectOopMirFunction& function,
    std::uint64_t& frameSize)
{
    std::vector<const ProjectOopMirNode*> work{root};
    while (!work.empty())
    {
        const ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        const bool isLocal = node->kind == ProjectOopMirNodeKind::LocalDeclaration &&
            node->localOrdinal.has_value() && !node->typeKey.empty();
        const bool survives = isLocal &&
            (liveAcrossBoundary(
                 *node->localOrdinal, localDefinition, localLastUse, yieldOrders) ||
             node->cleanupRegionOrdinal.has_value());
        if (survives)
        {
            const auto layout =
                objectLayoutModel.valueLayoutForTypeKey(nameLookup, node->typeKey);
            if (layout.has_value())
            {
                frameSize = alignFrameOffset(frameSize, layout->second);
                function.asyncFrameLocals.emplace_back(
                    *node->localOrdinal, frameSize, node->typeKey);
                frameSize += layout->first;
            }
        }
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
}

// EN: Freeze 3.0 C-02 (D2): each await keeps its child future in a slot of its own at the start of
//     the frame (secondaryImmediate holds the offset), so two awaits never share storage.
// ES: Congelamiento 3.0 C-02 (D2): cada await guarda su future hijo en un slot propio al inicio del
//     frame (secondaryImmediate lleva el offset), asi dos awaits nunca comparten almacenamiento.
void assignAwaitFrameSlotNodes(
    ProjectOopMirNode* root,
    const ProjectCompilationIndexes* indexes,
    std::span<const ProjectOopMirStructLayout> layouts,
    std::uint64_t& frameSize)
{
    std::vector<ProjectOopMirNode*> work{root};
    while (!work.empty())
    {
        ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        if (node->kind == ProjectOopMirNodeKind::AwaitFuture)
        {
            if (const ProjectOopMirStructLayout* layout =
                    functionStructLayout(indexes, layouts, node->elementTypeKey))
            {
                frameSize = alignFrameOffset(frameSize, 8);
                node->secondaryImmediate = frameSize;
                frameSize += layout->size;
            }
        }
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
}

// EN: Freeze 3.0 C-14: a frame slot's size and alignment. A future or iterator has no object
//     layout, only a MIR struct layout; without this fallback a future local or parameter was left
//     out of the frame (lost across a suspension, read as garbage by the poll).
// ES: Congelamiento 3.0 C-14: tamano y alineacion de un slot del frame. Un future o iterator no
//     tiene layout de objeto, solo layout MIR; sin este respaldo un future local o parametro quedaba
//     fuera del frame (perdido tras una suspension, leido como basura por el poll).
std::optional<std::pair<std::uint64_t, std::uint64_t>> frameValueLayout(
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup,
    const ProjectCompilationIndexes* indexes,
    std::span<const ProjectOopMirStructLayout> layouts,
    const std::string& typeKey)
{
    std::optional<std::pair<std::uint64_t, std::uint64_t>> layout =
        objectLayoutModel.valueLayoutForTypeKey(nameLookup, typeKey);
    if (layout.has_value()) return layout;
    if (const ProjectOopMirStructLayout* mir = functionStructLayout(indexes, layouts, typeKey))
        return std::pair<std::uint64_t, std::uint64_t>{mir->size, mir->alignment};
    return std::nullopt;
}

// EN: Freeze 3.0 C-02 (D1/D4): every local of a coroutine lives in its heap frame, so a resumed
//     poll sees the values it left and an address taken before a suspension stays valid. Linear
//     liveness missed locals reused by a later loop iteration; keeping them all is exact.
// ES: Congelamiento 3.0 C-02 (D1/D4): todo local de una corrutina vive en su frame del heap, asi el
//     poll reanudado ve los valores que dejo y una direccion tomada antes de suspender sigue
//     valida. La vivacidad lineal perdia locales que reusa la siguiente vuelta de un ciclo.
void appendCoroutineFrameLocalNodes(
    const ProjectOopMirNode* root,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup,
    const ProjectCompilationIndexes* indexes,
    std::span<const ProjectOopMirStructLayout> layouts,
    ProjectOopMirFunction& function,
    std::uint64_t& frameSize)
{
    OrdinalSet seen;
    std::vector<const ProjectOopMirNode*> work{root};
    while (!work.empty())
    {
        const ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        // EN: Freeze 3.0 D-01: the hidden owner of an owned temporary is a local too; a statement
        //     resumed after an await destroys the temporary its first pass made.
        // ES: Congelamiento 3.0 D-01: el dueno oculto de un temporal owned tambien es un local; una
        //     sentencia reanudada tras un await destruye el temporal que hizo su primera pasada.
        const bool declares = (node->kind == ProjectOopMirNodeKind::LocalDeclaration ||
                node->kind == ProjectOopMirNodeKind::UsingRegion ||
                node->kind == ProjectOopMirNodeKind::OwnedTemporaryBorrow) &&
            node->localOrdinal.has_value() && !node->staticLocal;
        const std::string& typeKey = node->kind == ProjectOopMirNodeKind::UsingRegion
            ? node->elementTypeKey : node->typeKey;
        if (declares && !typeKey.empty() && typeKey != "builtin:never" &&
            seen.insert(*node->localOrdinal).second)
        {
            // EN: A reference local stores an address; its slot is a pointer. A local a lambda
            //     captures lives in a heap cell, and its slot keeps that cell across suspensions.
            // ES: Un local referencia guarda una direccion; su slot es un puntero. Un local que una
            //     lambda captura vive en una celda del heap, y su slot conserva esa celda entre
            //     suspensiones.
            const bool captured = std::ranges::find(
                function.capturedLocalOrdinals, *node->localOrdinal) !=
                function.capturedLocalOrdinals.end();
            const bool pointerSlot = captured || node->referenceLocal || typeKey.ends_with("*");
            const std::string storageKey =
                captured || node->referenceLocal ? typeKey + "*" : typeKey;
            std::optional<std::pair<std::uint64_t, std::uint64_t>> layout;
            if (pointerSlot) layout = std::pair<std::uint64_t, std::uint64_t>{8, 8};
            else layout = frameValueLayout(objectLayoutModel, nameLookup, indexes, layouts, storageKey);
            if (layout.has_value())
            {
                frameSize = alignFrameOffset(frameSize, layout->second);
                function.asyncFrameLocals.emplace_back(*node->localOrdinal, frameSize, storageKey);
                frameSize += layout->first;
            }
        }
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
}

// EN: Freeze 3.0 C-02 (D3): every parameter the body reads or addresses is captured into the
//     frame by the constructor; a use inside a loop is needed again after any suspension.
// ES: Congelamiento 3.0 C-02 (D3): todo parametro que el cuerpo lee o direcciona lo captura el
//     constructor en el frame; un uso dentro de un ciclo se necesita tras cualquier suspension.
void appendUsedFrameParameterNodes(
    const ProjectOopMirNode* root,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup,
    const ProjectCompilationIndexes* indexes,
    std::span<const ProjectOopMirStructLayout> layouts,
    ProjectOopMirFunction& function,
    std::uint64_t& frameSize)
{
    OrdinalSet used;
    std::vector<const ProjectOopMirNode*> work{root};
    while (!work.empty())
    {
        const ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        // EN: Freeze 3.0 C-14: an owned parameter is captured even when unread, so its cleanup (or
        //     the future's drop) destroys the value its caller handed over.
        // ES: Congelamiento 3.0 C-14: un parametro owned se captura aunque no se lea, para que su
        //     cleanup (o el drop del future) destruya el valor que entrego quien llama.
        if ((node->kind == ProjectOopMirNodeKind::ParameterValue ||
             node->kind == ProjectOopMirNodeKind::ParameterAddress ||
             node->kind == ProjectOopMirNodeKind::OwnedParameterCleanup) &&
            node->parameterOrdinal.has_value())
            used.insert(*node->parameterOrdinal);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
    for (std::size_t parameter = 0;
         parameter < function.generatorParameterTypeKeys.size(); ++parameter)
    {
        if (!used.contains(parameter)) continue;
        const std::string& typeKey = function.generatorParameterTypeKeys[parameter];
        const auto layout = frameValueLayout(objectLayoutModel, nameLookup, indexes, layouts, typeKey);
        if (!layout.has_value()) continue;
        frameSize = alignFrameOffset(frameSize, layout->second);
        function.asyncFrameParameters.emplace_back(parameter, frameSize, typeKey);
        frameSize += layout->first;
    }
}

// EN: Freeze 3.0 D-01: where the climb from a suspending await toward its statement ends. A
//     control statement ends it before its other children are taken (its body is statements, and
//     a resume never re-enters it from the top); a simple statement ends it after.
// ES: Congelamiento 3.0 D-01: donde termina el ascenso desde un await que suspende hacia su
//     sentencia. Una sentencia de control lo termina antes de tomar sus otros hijos (su cuerpo son
//     sentencias, y una reanudacion nunca vuelve a entrar desde arriba); una simple, despues.
bool replayClimbStopsBefore(ProjectOopMirNodeKind kind) noexcept
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::Block:
        case ProjectOopMirNodeKind::BlockExpression:
        case ProjectOopMirNodeKind::If:
        case ProjectOopMirNodeKind::While:
        case ProjectOopMirNodeKind::DoWhile:
        case ProjectOopMirNodeKind::For:
        case ProjectOopMirNodeKind::Switch:
        case ProjectOopMirNodeKind::SwitchSection:
        case ProjectOopMirNodeKind::TryRegion:
        case ProjectOopMirNodeKind::CatchRegion:
        case ProjectOopMirNodeKind::FinallyRegion:
        case ProjectOopMirNodeKind::UsingRegion:
        case ProjectOopMirNodeKind::LoopExpression:
        case ProjectOopMirNodeKind::MatchExpression:
        case ProjectOopMirNodeKind::MatchArm:
            return true;
        default:
            return false;
    }
}

bool replayClimbStopsAfter(ProjectOopMirNodeKind kind) noexcept
{
    return kind == ProjectOopMirNodeKind::LocalDeclaration ||
        kind == ProjectOopMirNodeKind::ExpressionStatement ||
        kind == ProjectOopMirNodeKind::Return ||
        kind == ProjectOopMirNodeKind::YieldValue ||
        kind == ProjectOopMirNodeKind::AsyncYieldValue;
}

// EN: An operand worth a slot: a value with side effects or one that may change before the
//     resume. Constants, `this`, the fixed address of a variable and storage made in the poll's
//     own stack are not.
// ES: Un operando que merece slot: un valor con efectos o que puede cambiar antes de reanudar.
//     Las constantes, `this`, la direccion fija de una variable y el almacenamiento de la pila
//     del propio poll no.
bool replaySpillCandidateKind(ProjectOopMirNodeKind kind) noexcept
{
    if (replayClimbStopsBefore(kind) || replayClimbStopsAfter(kind)) return false;
    switch (kind)
    {
        case ProjectOopMirNodeKind::IntegerConstant:
        case ProjectOopMirNodeKind::FloatingConstant:
        case ProjectOopMirNodeKind::BooleanConstant:
        case ProjectOopMirNodeKind::NullConstant:
        case ProjectOopMirNodeKind::DefaultValue:
        case ProjectOopMirNodeKind::StringLiteral:
        case ProjectOopMirNodeKind::Utf8StringLiteral:
        case ProjectOopMirNodeKind::EnumName:
        case ProjectOopMirNodeKind::ThisValue:
        case ProjectOopMirNodeKind::LocalAddress:
        case ProjectOopMirNodeKind::ParameterAddress:
        case ProjectOopMirNodeKind::StaticFieldAddress:
        case ProjectOopMirNodeKind::NamespaceStateAddress:
        case ProjectOopMirNodeKind::TemporaryAddress:
        case ProjectOopMirNodeKind::StackAllocation:
        case ProjectOopMirNodeKind::ParameterPack:
        case ProjectOopMirNodeKind::AggregateElementStore:
        case ProjectOopMirNodeKind::AggregateSpreadStore:
        case ProjectOopMirNodeKind::TupleDeconstructElementStore:
        case ProjectOopMirNodeKind::BitFieldAddress:
        case ProjectOopMirNodeKind::FutureFieldAddress:
        case ProjectOopMirNodeKind::InlineAssembly:
        case ProjectOopMirNodeKind::InlineAssemblyLine:
        case ProjectOopMirNodeKind::InlineAssemblyInput:
        case ProjectOopMirNodeKind::InlineAssemblyOutput:
        case ProjectOopMirNodeKind::InlineAssemblyClobber:
        case ProjectOopMirNodeKind::Break:
        case ProjectOopMirNodeKind::Continue:
        case ProjectOopMirNodeKind::Throw:
        case ProjectOopMirNodeKind::Rethrow:
        case ProjectOopMirNodeKind::CleanupContinuation:
        case ProjectOopMirNodeKind::OwnedParameterCleanup:
        case ProjectOopMirNodeKind::SuspendPoint:
        case ProjectOopMirNodeKind::YieldBreak:
        case ProjectOopMirNodeKind::GeneratorFrameInit:
        case ProjectOopMirNodeKind::DropFrame:
            return false;
        default:
            return true;
    }
}

bool replaySpillAddressKind(ProjectOopMirNodeKind kind) noexcept
{
    return kind == ProjectOopMirNodeKind::FieldAddress ||
        kind == ProjectOopMirNodeKind::ArrayElementAddress ||
        kind == ProjectOopMirNodeKind::RangeArrayElementAddress ||
        kind == ProjectOopMirNodeKind::RawElementAddress;
}

// EN: Freeze 3.0 D-01: a resumed poll re-enters the innermost statement around the await that
//     suspended, so every operand that runs before the await in that statement used to run twice
//     (a call made twice, an index taken twice, a temporary made twice). Each such operand gets a
//     frame slot: the backend stores the value on the first pass and loads it on the replay. The
//     climb goes from the await to its statement and takes every child off the path; one that
//     runs after the await is stored and never loaded, which costs a store and keeps the rule
//     independent of each node's evaluation order. An address is a pointer slot; a value needs a
//     known layout.
// ES: Congelamiento 3.0 D-01: un poll reanudado vuelve a entrar a la sentencia mas interna que
//     rodea al await que suspendio, asi todo operando que corre antes del await en esa sentencia
//     corria dos veces (una llamada hecha dos veces, un indice tomado dos veces, un temporal hecho
//     dos veces). Cada operando asi recibe un slot del frame: el backend guarda el valor en la
//     primera pasada y lo carga en la repeticion. El ascenso va del await a su sentencia y toma
//     todo hijo fuera del camino; uno que corre despues del await se guarda y nunca se carga, lo
//     que cuesta un store y deja la regla independiente del orden de evaluacion de cada nodo. Una
//     direccion es un slot de puntero; un valor necesita layout conocido.
void assignReplaySpillSlotNodes(
    ProjectOopMirNode* root,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup,
    const ProjectCompilationIndexes* indexes,
    std::span<const ProjectOopMirStructLayout> layouts,
    std::uint64_t& frameSize)
{
    std::unordered_map<const ProjectOopMirNode*, ProjectOopMirNode*> parents;
    std::vector<ProjectOopMirNode*> awaits;
    std::vector<ProjectOopMirNode*> work{root};
    while (!work.empty())
    {
        ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        if (node->kind == ProjectOopMirNodeKind::AwaitFuture && node->integerValue == 1)
            awaits.push_back(node);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
        {
            if (*child == nullptr) continue;
            parents.emplace(child->get(), node);
            work.push_back(child->get());
        }
    }
    for (ProjectOopMirNode* await : awaits)
    {
        const ProjectOopMirNode* onPath = await;
        for (auto parent = parents.find(onPath); parent != parents.end(); parent = parents.find(onPath))
        {
            ProjectOopMirNode* ancestor = parent->second;
            if (replayClimbStopsBefore(ancestor->kind)) break;
            for (const auto& child : ancestor->children)
            {
                ProjectOopMirNode* candidate = child.get();
                if (candidate == nullptr || candidate == onPath ||
                    candidate->replaySpillOffset.has_value() ||
                    !replaySpillCandidateKind(candidate->kind) || candidate->typeKey.empty() ||
                    candidate->typeKey == "builtin:void" || candidate->typeKey == "builtin:never")
                    continue;
                std::optional<std::pair<std::uint64_t, std::uint64_t>> layout;
                if (replaySpillAddressKind(candidate->kind))
                    layout = std::pair<std::uint64_t, std::uint64_t>{8, 8};
                else
                    layout = frameValueLayout(
                        objectLayoutModel, nameLookup, indexes, layouts, candidate->typeKey);
                if (!layout.has_value()) continue;
                frameSize = alignFrameOffset(frameSize, std::max<std::uint64_t>(8, layout->second));
                candidate->replaySpillOffset = frameSize;
                frameSize += std::max<std::uint64_t>(8, layout->first);
            }
            if (replayClimbStopsAfter(ancestor->kind)) break;
            onPath = ancestor;
        }
    }
}

// EN: Freeze 3.0 C-02: one drop-flag byte per frame local closes the frame, 8-aligned, so a
//     destructor guard survives a suspension too. The backend finds the area at
//     asyncFrameSize - align8(locals); the constructor zeroes it.
// ES: Congelamiento 3.0 C-02: un byte de bandera de destruccion por local del frame cierra el
//     frame, alineado a 8, para que la guarda del destructor tambien sobreviva una suspension. El
//     backend halla el area en asyncFrameSize - align8(locales); el constructor la pone en cero.
void appendCoroutineDropFlags(const ProjectOopMirFunction& function, std::uint64_t& frameSize) noexcept
{
    if (function.asyncFrameLocals.empty()) return;
    frameSize = alignFrameOffset(frameSize, 8) + function.asyncFrameLocals.size();
}

bool buildPollFrame(
    ProjectOopMirFunction& function,
    const ProjectCompilationIndexes* indexes,
    std::span<const ProjectOopMirStructLayout> layouts,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup,
    std::string& error)
{
    std::uint64_t frameSize = 0;
    assignAwaitFrameSlotNodes(function.body.get(), indexes, layouts, frameSize);
    assignReplaySpillSlotNodes(
        function.body.get(), objectLayoutModel, nameLookup, indexes, layouts, frameSize);
    std::uint64_t order = 0;
    PositionMap localDefinition;
    PositionMap localLastUse;
    PositionMap parameterLastUse;
    std::vector<std::uint64_t> suspendOrders;
    collectPollLivenessNodes(
        function.body.get(), order, localDefinition, localLastUse,
        parameterLastUse, suspendOrders);
    std::unordered_map<std::size_t, std::size_t> borrowSource;
    OrdinalSet borrowNonLocalSource;
    collectBorrowSourcesNodes(function.body.get(), borrowSource, borrowNonLocalSource);
    // EN: Liveness still decides the borrow rule; the frame itself keeps every local (C-02).
    // ES: La vivacidad aun decide la regla de prestamos; el frame guarda todo local (C-02).
    if (!validateNonLocalBorrowSources(
            borrowNonLocalSource, localDefinition, localLastUse, suspendOrders, error))
        return false;
    appendCoroutineFrameLocalNodes(
        function.body.get(), objectLayoutModel, nameLookup, indexes, layouts, function, frameSize);
    appendUsedFrameParameterNodes(
        function.body.get(), objectLayoutModel, nameLookup, indexes, layouts, function, frameSize);
    appendCoroutineDropFlags(function, frameSize);
    function.asyncFrameSize = alignFrameOffset(frameSize, 8);
    return true;
}

void buildMoveNextFrame(
    ProjectOopMirFunction& function,
    const ProjectCompilationIndexes* indexes,
    std::span<const ProjectOopMirStructLayout> layouts,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup)
{
    // EN: Freeze 3.0 C-02: like a poll, a MoveNext keeps every local and every used parameter in
    //     the frame; the linear liveness this replaced lost values across loop iterations.
    // ES: Congelamiento 3.0 C-02: como un poll, un MoveNext guarda todo local y todo parametro
    //     usado en el frame; la vivacidad lineal que esto sustituye perdia valores entre vueltas.
    std::uint64_t frameSize = 0;
    appendCoroutineFrameLocalNodes(
        function.body.get(), objectLayoutModel, nameLookup, indexes, layouts, function, frameSize);
    appendUsedFrameParameterNodes(
        function.body.get(), objectLayoutModel, nameLookup, indexes, layouts, function, frameSize);
    appendCoroutineDropFlags(function, frameSize);
    function.asyncFrameSize = alignFrameOffset(frameSize, 8);
}

void inheritMoveNextDropFrames(std::vector<ProjectOopMirFunction>& functions)
{
    // mangledName -> first function with that name (the candidate the former scan found). Built
    // once; the loop below changes frames only, never names.
    std::unordered_map<std::string_view, std::size_t> firstByName;
    firstByName.reserve(functions.size());
    for (std::size_t index = 0; index < functions.size(); ++index)
        firstByName.try_emplace(functions[index].mangledName, index);
    for (ProjectOopMirFunction& function : functions)
    {
        if (!function.mangledName.ends_with("$moveNext")) continue;
        const std::string dropName =
            function.mangledName.substr(0, function.mangledName.size() - 9) + "$drop";
        const auto drop = firstByName.find(dropName);
        if constexpr (internal::kIndexShadowCheckEnabled)
        {
            const auto scanned = std::ranges::find(functions, dropName, &ProjectOopMirFunction::mangledName);
            const bool same = drop == firstByName.end()
                ? scanned == functions.end()
                : scanned != functions.end() &&
                    static_cast<std::size_t>(scanned - functions.begin()) == drop->second;
            if (!same) internal::reportIndexShadowMismatch("inheritMoveNextDropFrames", dropName);
        }
        if (drop == firstByName.end()) continue;
        ProjectOopMirFunction& candidate = functions[drop->second];
        candidate.asyncFrameLocals = function.asyncFrameLocals;
        candidate.asyncFrameSize = function.asyncFrameSize;
    }
}

void insertGeneratorFrameInit(ProjectOopMirFunction& function)
{
    if (function.body == nullptr || function.body->children.empty() ||
        function.body->children[0]->kind != ProjectOopMirNodeKind::LocalDeclaration)
        return;
    // EN: Freeze 3.0 C-17: an async generator inherits from both `$moveNext` and `$moveNext$poll`;
    //     a second init allocated a second frame and lost the first.
    // ES: Congelamiento 3.0 C-17: un generador async hereda de `$moveNext` y de `$moveNext$poll`;
    //     una segunda inicializacion reservaba un segundo frame y perdia el primero.
    if (function.body->children.size() > 1U &&
        function.body->children[1]->kind == ProjectOopMirNodeKind::GeneratorFrameInit)
        return;
    auto frameInit = std::make_unique<ProjectOopMirNode>();
    frameInit->kind = ProjectOopMirNodeKind::GeneratorFrameInit;
    frameInit->typeKey = "builtin:void";
    frameInit->localOrdinal = function.body->children[0]->localOrdinal;
    function.body->children.insert(
        function.body->children.begin() + 1, std::move(frameInit));
}

void inheritConstructorFrames(
    std::vector<ProjectOopMirFunction>& functions,
    std::string_view suffix,
    std::size_t suffixLength,
    bool excludeMoveNextPoll)
{
    // mangledName -> first generator constructor with that name (the candidate the former scan
    // found, which required isGeneratorConstructor). Built once; the loop below changes frames
    // and bodies only, never names or the generator flag.
    std::unordered_map<std::string_view, std::size_t> constructorByName;
    for (std::size_t index = 0; index < functions.size(); ++index)
    {
        if (functions[index].isGeneratorConstructor)
            constructorByName.try_emplace(functions[index].mangledName, index);
    }
    for (ProjectOopMirFunction& function : functions)
    {
        if (!function.mangledName.ends_with(suffix)) continue;
        if (excludeMoveNextPoll && function.mangledName.ends_with("$moveNext$poll")) continue;
        const std::string constructorName =
            function.mangledName.substr(0, function.mangledName.size() - suffixLength);
        const auto constructor = constructorByName.find(constructorName);
        if constexpr (internal::kIndexShadowCheckEnabled)
        {
            const auto scanned = std::ranges::find_if(functions, [&](const ProjectOopMirFunction& candidate)
            {
                return candidate.mangledName == constructorName && candidate.isGeneratorConstructor;
            });
            const bool same = constructor == constructorByName.end()
                ? scanned == functions.end()
                : scanned != functions.end() &&
                    static_cast<std::size_t>(scanned - functions.begin()) == constructor->second;
            if (!same) internal::reportIndexShadowMismatch("inheritConstructorFrames", constructorName);
        }
        if (constructor == constructorByName.end()) continue;
        ProjectOopMirFunction& candidate = functions[constructor->second];
        candidate.asyncFrameLocals = function.asyncFrameLocals;
        candidate.asyncFrameParameters = function.asyncFrameParameters;
        candidate.asyncFrameSize = function.asyncFrameSize;
        insertGeneratorFrameInit(candidate);
    }
}
}

bool ProjectOopMirModel::collectCleanupContract(
    ProjectOopMirFunction& function,
    const ProjectOopMirNode* root,
    language::PanicProfile panicProfile,
    std::string& error)
{
    return collectCleanupContractNodes(function, root, panicProfile, error);
}

bool ProjectOopMirModel::validateCleanupReferences(
    const ProjectOopMirFunction& function,
    const ProjectOopMirNode* root,
    std::string& error)
{
    return validateCleanupReferenceNodes(function, root, error);
}

void ProjectOopMirModel::sumAwaitedFrameSize(
    const ProjectOopMirNode* root,
    std::span<const ProjectOopMirStructLayout> layouts,
    std::uint64_t& frameSize)
{
    // The layouts given here may not be the ones the shared index was built from: scan them.
    sumAwaitedFrameSizeNodes(root, nullptr, layouts, frameSize);
}

void ProjectOopMirModel::collectPollLiveness(
    const ProjectOopMirNode* root,
    std::uint64_t& order,
    std::unordered_map<std::size_t, std::uint64_t>& localDefinition,
    std::unordered_map<std::size_t, std::uint64_t>& localLastUse,
    std::unordered_map<std::size_t, std::uint64_t>& parameterLastUse,
    std::vector<std::uint64_t>& suspendOrders)
{
    collectPollLivenessNodes(
        root, order, localDefinition, localLastUse, parameterLastUse, suspendOrders);
}

void ProjectOopMirModel::collectMoveNextLiveness(
    const ProjectOopMirNode* root,
    std::uint64_t& order,
    std::unordered_map<std::size_t, std::uint64_t>& localDefinition,
    std::unordered_map<std::size_t, std::uint64_t>& localLastUse,
    std::unordered_map<std::size_t, std::uint64_t>& parameterLastUse,
    std::vector<std::uint64_t>& yieldOrders)
{
    collectMoveNextLivenessNodes(
        root, order, localDefinition, localLastUse, parameterLastUse, yieldOrders);
}

void ProjectOopMirModel::collectBorrowSources(
    const ProjectOopMirNode* root,
    std::unordered_map<std::size_t, std::size_t>& borrowSource,
    std::unordered_set<std::size_t>& borrowNonLocalSource)
{
    collectBorrowSourcesNodes(root, borrowSource, borrowNonLocalSource);
}

void ProjectOopMirModel::collectLivePollLocals(
    const ProjectOopMirNode* root,
    const std::unordered_map<std::size_t, std::uint64_t>& localDefinition,
    const std::unordered_map<std::size_t, std::uint64_t>& localLastUse,
    std::span<const std::uint64_t> suspendOrders,
    std::unordered_set<std::size_t>& liveSet)
{
    collectLivePollLocalNodes(
        root, localDefinition, localLastUse, suspendOrders, liveSet);
}

void ProjectOopMirModel::appendPollFrameLocals(
    const ProjectOopMirNode* root,
    const std::unordered_set<std::size_t>& liveSet,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup,
    ProjectOopMirFunction& function,
    std::uint64_t& frameSize)
{
    appendPollFrameLocalNodes(
        root, liveSet, objectLayoutModel, nameLookup, function, frameSize);
}

void ProjectOopMirModel::appendMoveNextFrameLocals(
    const ProjectOopMirNode* root,
    const std::unordered_map<std::size_t, std::uint64_t>& localDefinition,
    const std::unordered_map<std::size_t, std::uint64_t>& localLastUse,
    std::span<const std::uint64_t> yieldOrders,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup,
    ProjectOopMirFunction& function,
    std::uint64_t& frameSize)
{
    appendMoveNextFrameLocalNodes(
        root, localDefinition, localLastUse, yieldOrders,
        objectLayoutModel, nameLookup, function, frameSize);
}

ProjectOopMirFunction ProjectOopMirModel::lowerOneFunction(
    const ProjectOopHirFunction& source,
    const ProjectNameLookup& nameLookup,
    const ProjectObjectLayoutModel& objectLayoutModel,
    std::span<const ProjectOopMirStructLayout> functionLayouts,
    language::PanicProfile panicProfile,
    std::string& error)
{
    ProjectOopMirFunction function;
    copyFunctionMetadata(function, source);
    configureFunctionAbi(function, indexes_, functionLayouts);
    if (source.isImportedC || source.isImportedNative) return function;
    function.body = lowerNode(source.body.get(), error, functionLayouts);
    if (function.mangledName.ends_with("$poll") &&
        !buildPollFrame(function, indexes_, functionLayouts, objectLayoutModel, nameLookup, error))
        return function;
    // EN: Freeze 3.0 C-14: only a synchronous generator's MoveNext is a coroutine; an async
    //     generator's $moveNext just builds the demand future, whose local must stay off the frame.
    // ES: Congelamiento 3.0 C-14: solo el MoveNext de un generador sincrono es una corrutina; el
    //     $moveNext de un generador async solo arma el future de demanda, cuyo local no va al frame.
    if (function.mangledName.ends_with("$moveNext") && function.returnTypeKey == "builtin:bool")
        buildMoveNextFrame(function, indexes_, functionLayouts, objectLayoutModel, nameLookup);
    if (function.body == nullptr)
    {
        error += "\n  while lowering '" + source.mangledName + "'";
    }
    else if (!finalizeCleanupContract(function, panicProfile, error))
    {
        error += "\n  while finalizing ownership/cleanup MIR for '" +
            source.mangledName + "'";
    }
    return function;
}

void ProjectOopMirModel::configureFunctionLowerer(
    ProjectOopMirModel& lowerer,
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectVirtualDispatchModel& virtualDispatchModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectDestructionModel& destructionModel,
    const ProjectOopHirModel& hirModel) const
{
    lowerer.nameLookup_ = &nameLookup;
    lowerer.memberModel_ = &memberModel;
    lowerer.objectLayoutModel_ = &objectLayoutModel;
    lowerer.virtualDispatchModel_ = &virtualDispatchModel;
    lowerer.interfaceModel_ = &interfaceModel;
    lowerer.destructionModel_ = &destructionModel;
    lowerer.hirModel_ = &hirModel;
    lowerer.indexes_ = indexes_;
}

bool ProjectOopMirModel::lowerFunctions(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectVirtualDispatchModel& virtualDispatchModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectDestructionModel& destructionModel,
    const ProjectOopHirModel& hirModel,
    std::string& error,
    std::size_t workerCount)
{
    driver::CompilationWorkerPool workerPool(workerCount);
    return lowerFunctions(
        nameLookup, memberModel, objectLayoutModel, virtualDispatchModel,
        interfaceModel, destructionModel, hirModel, error, workerCount, &workerPool);
}

bool ProjectOopMirModel::lowerFunctions(
    const ProjectNameLookup& nameLookup,
    const ProjectMemberModel& memberModel,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectVirtualDispatchModel& virtualDispatchModel,
    const ProjectInterfaceModel& interfaceModel,
    const ProjectDestructionModel& destructionModel,
    const ProjectOopHirModel& hirModel,
    std::string& error,
    std::size_t workerCount,
    driver::CompilationWorkerPool* workerPool)
{
    const std::span<const ProjectOopHirFunction> hirFunctions = hirModel.functions();
    std::vector<std::optional<ProjectOopMirFunction>> loweredFunctions(hirFunctions.size());
    std::vector<std::string> loweringErrors(hirFunctions.size());
    installRestoredMirFunctions(loweredFunctions);
    if (workerCount > 1 && hirFunctions.size() > 1)
    {
        driver::CompilationWorkerPool& workers = *workerPool;
        for (std::size_t index = 0; index < hirFunctions.size(); ++index)
        {
            if (loweredFunctions[index].has_value()) continue;
            workers.submit([&, index](std::size_t) {
                ProjectOopMirModel lowerer;
                configureFunctionLowerer(
                    lowerer, nameLookup, memberModel, objectLayoutModel,
                    virtualDispatchModel, interfaceModel, destructionModel, hirModel);
                loweredFunctions[index] = lowerer.lowerOneFunction(
                    hirFunctions[index], nameLookup, objectLayoutModel,
                    structLayouts_, panicProfile_, loweringErrors[index]);
            });
        }
        workers.wait();
    }
    else
    {
        for (std::size_t index = 0; index < hirFunctions.size(); ++index)
        {
            if (loweredFunctions[index].has_value()) continue;
            loweredFunctions[index] = lowerOneFunction(
                hirFunctions[index], nameLookup, objectLayoutModel,
                structLayouts_, panicProfile_, loweringErrors[index]);
        }
    }

    functions_.reserve(hirFunctions.size());
    for (std::size_t index = 0; index < hirFunctions.size(); ++index)
    {
        if (!loweringErrors[index].empty() || !loweredFunctions[index].has_value() ||
            (loweredFunctions[index]->body == nullptr &&
             !loweredFunctions[index]->isImportedC && !loweredFunctions[index]->isImportedNative))
        {
            error = loweringErrors[index].empty()
                ? "Stage 34 failed to lower an OOP HIR function to MIR"
                : std::move(loweringErrors[index]);
            return false;
        }
        ProjectOopMirFunction function = std::move(*loweredFunctions[index]);
        if (function.namespaceFunctionOrdinal.has_value())
        {
            functionByNamespaceFunction_[*function.namespaceFunctionOrdinal] = functions_.size();
        }
        else if (!function.isLambda && !function.isSyntheticImplicitConstructor)
        {
            functionByMember_[function.memberOrdinal] = functions_.size();
        }
        functions_.push_back(std::move(function));
    }

    inheritMoveNextDropFrames(functions_);
    inheritConstructorFrames(functions_, "$moveNext", 9, false);
    inheritConstructorFrames(functions_, "$moveNext$poll", 14, false);
    inheritConstructorFrames(functions_, "$poll", 5, true);
    return true;
}
}
