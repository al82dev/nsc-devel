#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
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

ProjectOopMirAbiPassing abiPassingForType(
    std::span<const ProjectOopMirStructLayout> layouts,
    std::string_view typeKey) noexcept
{
    for (const ProjectOopMirStructLayout& layout : layouts)
        if (layout.typeKey == typeKey) return layout.abiPassing;
    return ProjectOopMirAbiPassing::Direct;
}

void configureFunctionAbi(
    ProjectOopMirFunction& function,
    std::span<const ProjectOopMirStructLayout> layouts)
{
    function.abi.returnPassing = function.referenceReturn
        ? ProjectOopMirAbiPassing::Direct
        : abiPassingForType(layouts, function.returnTypeKey);
    std::size_t physicalOrdinal = function.isStatic ? 0U : 1U;
    if (function.abi.returnPassing == ProjectOopMirAbiPassing::Indirect)
        function.abi.hiddenReturnParameterOrdinal = physicalOrdinal++;
    for (const std::string& parameterTypeKey : function.parameterTypeKeys)
    {
        function.abi.parameterPassing.push_back(
            abiPassingForType(layouts, parameterTypeKey));
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
            for (const ProjectOopMirStructLayout& layout : layouts)
            {
                if (layout.typeKey != node->elementTypeKey) continue;
                frameSize += layout.size;
                break;
            }
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

void closeLiveBorrowSources(
    const std::unordered_map<std::size_t, std::size_t>& borrowSource,
    OrdinalSet& liveSet)
{
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const auto& [borrowedLocal, sourceLocal] : borrowSource)
        {
            if (!liveSet.contains(borrowedLocal) || liveSet.contains(sourceLocal)) continue;
            liveSet.insert(sourceLocal);
            changed = true;
        }
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

void appendMoveNextFrameLocals(
    const ProjectOopMirNode* node,
    const PositionMap& localDefinition,
    const PositionMap& localLastUse,
    std::span<const std::uint64_t> yieldOrders,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup,
    ProjectOopMirFunction& function,
    std::uint64_t& frameSize)
{
    if (node == nullptr) return;
    const bool isLocal = node->kind == ProjectOopMirNodeKind::LocalDeclaration &&
        node->localOrdinal.has_value() && !node->typeKey.empty();
    const bool survives = isLocal &&
        (liveAcrossBoundary(
             *node->localOrdinal, localDefinition, localLastUse, yieldOrders) ||
         node->cleanupRegionOrdinal.has_value());
    if (survives)
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
    for (const std::unique_ptr<ProjectOopMirNode>& child : node->children)
        appendMoveNextFrameLocals(
            child.get(), localDefinition, localLastUse, yieldOrders,
            objectLayoutModel, nameLookup, function, frameSize);
}

void appendLiveFrameParameters(
    const PositionMap& parameterLastUse,
    std::span<const std::uint64_t> boundaries,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup,
    ProjectOopMirFunction& function,
    std::uint64_t& frameSize)
{
    for (std::size_t parameter = 0;
         parameter < function.generatorParameterTypeKeys.size(); ++parameter)
    {
        const auto lastUse = parameterLastUse.find(parameter);
        if (lastUse == parameterLastUse.end()) continue;
        bool live = false;
        for (const std::uint64_t boundary : boundaries)
        {
            if (boundary >= lastUse->second) continue;
            live = true;
            break;
        }
        if (!live) continue;
        const std::string& typeKey = function.generatorParameterTypeKeys[parameter];
        const auto layout = objectLayoutModel.valueLayoutForTypeKey(nameLookup, typeKey);
        if (!layout.has_value()) continue;
        frameSize = alignFrameOffset(frameSize, layout->second);
        function.asyncFrameParameters.emplace_back(parameter, frameSize, typeKey);
        frameSize += layout->first;
    }
}

bool buildPollFrame(
    ProjectOopMirFunction& function,
    std::span<const ProjectOopMirStructLayout> layouts,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup,
    std::string& error)
{
    std::uint64_t frameSize = 0;
    sumAwaitedFrameSizeNodes(function.body.get(), layouts, frameSize);
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
    OrdinalSet liveSet;
    collectLivePollLocalNodes(
        function.body.get(), localDefinition, localLastUse, suspendOrders, liveSet);
    closeLiveBorrowSources(borrowSource, liveSet);
    if (!validateNonLocalBorrowSources(
            borrowNonLocalSource, localDefinition, localLastUse, suspendOrders, error))
        return false;
    appendPollFrameLocalNodes(
        function.body.get(), liveSet, objectLayoutModel, nameLookup, function, frameSize);
    appendLiveFrameParameters(
        parameterLastUse, suspendOrders, objectLayoutModel, nameLookup, function, frameSize);
    function.asyncFrameSize = alignFrameOffset(frameSize, 8);
    return true;
}

void buildMoveNextFrame(
    ProjectOopMirFunction& function,
    const ProjectObjectLayoutModel& objectLayoutModel,
    const ProjectNameLookup& nameLookup)
{
    std::uint64_t order = 0;
    PositionMap localDefinition;
    PositionMap localLastUse;
    PositionMap parameterLastUse;
    std::vector<std::uint64_t> yieldOrders;
    collectMoveNextLivenessNodes(
        function.body.get(), order, localDefinition, localLastUse,
        parameterLastUse, yieldOrders);
    std::uint64_t frameSize = 0;
    appendMoveNextFrameLocals(
        function.body.get(), localDefinition, localLastUse, yieldOrders,
        objectLayoutModel, nameLookup, function, frameSize);
    appendLiveFrameParameters(
        parameterLastUse, yieldOrders, objectLayoutModel, nameLookup, function, frameSize);
    function.asyncFrameSize = alignFrameOffset(frameSize, 8);
}

void inheritMoveNextDropFrames(std::vector<ProjectOopMirFunction>& functions)
{
    for (ProjectOopMirFunction& function : functions)
    {
        if (!function.mangledName.ends_with("$moveNext")) continue;
        const std::string dropName =
            function.mangledName.substr(0, function.mangledName.size() - 9) + "$drop";
        for (ProjectOopMirFunction& candidate : functions)
        {
            if (candidate.mangledName != dropName) continue;
            candidate.asyncFrameLocals = function.asyncFrameLocals;
            candidate.asyncFrameSize = function.asyncFrameSize;
            break;
        }
    }
}

void insertGeneratorFrameInit(ProjectOopMirFunction& function)
{
    if (function.body == nullptr || function.body->children.empty() ||
        function.body->children[0]->kind != ProjectOopMirNodeKind::LocalDeclaration)
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
    for (ProjectOopMirFunction& function : functions)
    {
        if (!function.mangledName.ends_with(suffix)) continue;
        if (excludeMoveNextPoll && function.mangledName.ends_with("$moveNext$poll")) continue;
        const std::string constructorName =
            function.mangledName.substr(0, function.mangledName.size() - suffixLength);
        for (ProjectOopMirFunction& candidate : functions)
        {
            if (candidate.mangledName != constructorName || !candidate.isGeneratorConstructor)
                continue;
            candidate.asyncFrameLocals = function.asyncFrameLocals;
            candidate.asyncFrameParameters = function.asyncFrameParameters;
            candidate.asyncFrameSize = function.asyncFrameSize;
            insertGeneratorFrameInit(candidate);
            break;
        }
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
    sumAwaitedFrameSizeNodes(root, layouts, frameSize);
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
    configureFunctionAbi(function, functionLayouts);
    if (source.isImportedC || source.isImportedNative) return function;
    function.body = lowerNode(source.body.get(), error);
    if (function.mangledName.ends_with("$poll") &&
        !buildPollFrame(function, functionLayouts, objectLayoutModel, nameLookup, error))
        return function;
    if (function.mangledName.ends_with("$moveNext"))
        buildMoveNextFrame(function, objectLayoutModel, nameLookup);
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
    lowerer.structLayouts_ = structLayouts_;
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
    const std::span<const ProjectOopHirFunction> hirFunctions = hirModel.functions();
    std::vector<std::optional<ProjectOopMirFunction>> loweredFunctions(hirFunctions.size());
    std::vector<std::string> loweringErrors(hirFunctions.size());
    if (workerCount > 1 && hirFunctions.size() > 1)
    {
        driver::CompilationWorkerPool workers(workerCount);
        for (std::size_t index = 0; index < hirFunctions.size(); ++index)
        {
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
            loweredFunctions[index] = lowerOneFunction(
                hirFunctions[index], nameLookup, objectLayoutModel,
                structLayouts_, panicProfile_, loweringErrors[index]);
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
