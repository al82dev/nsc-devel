#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"
#include "nativesharp/LLVM/ProjectOopFunctionReachability.h"

#include "nativesharp/LLVM/FloatingPointMode.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/LLVM/CodegenIdentity.h"

#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationObservation.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopCodePartition.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/ProjectOopMirCacheIdentity.h"

#include "Internal/ColdLlvmPreambleTelemetry.h"
#include "Internal/ColdLlvmTrace.h"
#include "Internal/ColdLlvmTraceScope.h"
#include "Internal/CodeUnitProgress.h"
#include "Internal/PhaseClock.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <optional>
#include <system_error>
#include <utility>

namespace nativesharp::llvm_backend
{
namespace
{
std::optional<std::filesystem::path> createTemporaryDirectory(std::string& error)
{
    std::error_code filesystemError;
    const std::filesystem::path root = std::filesystem::temp_directory_path(filesystemError);
    if (filesystemError)
    {
        error = "could not resolve the system temporary directory for partitioned code generation: " +
            filesystemError.message();
        return std::nullopt;
    }
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (std::uint32_t attempt = 0; attempt < 100; ++attempt)
    {
        const std::filesystem::path candidate = root /
            ("nativesharp-codegen-" + std::to_string(stamp) + "-" + std::to_string(attempt));
        filesystemError.clear();
        if (std::filesystem::create_directory(candidate, filesystemError)) return candidate;
        if (filesystemError && filesystemError != std::errc::file_exists)
        {
            error = "could not create partitioned code-generation directory: " +
                filesystemError.message();
            return std::nullopt;
        }
    }
    error = "could not allocate a unique partitioned code-generation directory";
    return std::nullopt;
}





void appendNodeSourceRanges(
    const frontend::ProjectOopMirNode* root,
    std::vector<SourceRange>& ranges)
{
    std::vector<const frontend::ProjectOopMirNode*> work;
    work.push_back(root);
    while (!work.empty())
    {
        const frontend::ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        if (node->syntax != nullptr) ranges.push_back(node->syntax->range());
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            work.push_back(child->get());
    }
}

std::vector<SourceRange> collectLineTableSourceRanges(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel)
{
    std::vector<SourceRange> ranges;
    for (const frontend::NamedTypeDeclaration& type : nameLookup.namedTypes())
        ranges.push_back(type.range);
    for (const frontend::ClassMemberSymbol& member : memberModel.members())
        ranges.push_back(member.range);
    for (const frontend::ProjectOopMirStaticField& field : mirModel.staticFields())
        ranges.push_back(field.sourceRange);
    for (const frontend::ProjectOopMirFieldInitializer& initializer : mirModel.fieldInitializers())
        appendNodeSourceRanges(initializer.value.get(), ranges);
    for (const frontend::ProjectOopMirFunction& function : mirModel.functions())
    {
        appendNodeSourceRanges(function.body.get(), ranges);
        for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
            appendNodeSourceRanges(cleanup.action, ranges);
        for (const frontend::ProjectOopMirNonlocalExit& exit : function.exceptionFlow.nonlocalExits)
            appendNodeSourceRanges(exit.node, ranges);
    }
    return ranges;
}

std::string combineLineTableCacheIdentity(
    std::string mirIdentity,
    std::string_view sourceIdentity)
{
    std::string combined = "NativeSharp.ProjectOopLineTableObjectIdentity.v1\n";
    combined += "mir-size " + std::to_string(mirIdentity.size()) + "\n";
    combined += mirIdentity;
    combined += "source-size " + std::to_string(sourceIdentity.size()) + "\n";
    combined += sourceIdentity;
    return combined;
}

std::vector<std::uint8_t> prepareCodeUnitTelemetry(
    std::size_t ordinal,
    bool usePartitions,
    const frontend::ProjectOopMirModel& mirModel,
    const ProjectOopCodePartition& partition,
    std::span<const std::size_t> coordinatorFunctionDeclarations,
    const ProjectOopFunctionReachabilityAnalysis& functionAnalysis,
    CodeUnitTelemetry& telemetry,
    std::string& moduleName)
{
    moduleName = "NativeSharp.OOP";
    if (usePartitions)
        moduleName = ordinal == 0
            ? "NativeSharp.OOP.globals"
            : "NativeSharp.OOP." + partition.units()[ordinal - 1].stableKey;
    telemetry.name = moduleName;
    if (!usePartitions)
    {
        telemetry.definitionCount = mirModel.functions().size();
        telemetry.declarationCount = mirModel.functions().size();
    }
    else if (ordinal == 0)
    {
        telemetry.declarationCount = coordinatorFunctionDeclarations.size();
    }
    else
    {
        const ProjectOopCodeUnit& unit = partition.units()[ordinal - 1];
        telemetry.definitionCount = unit.functionOrdinals.size();
        telemetry.declarationCount = unit.declarationOrdinals.size();
    }
    const std::span<const std::size_t> definitions = !usePartitions || ordinal == 0
        ? std::span<const std::size_t>{}
        : std::span<const std::size_t>(partition.units()[ordinal - 1].functionOrdinals);
    const PhaseClock staticFieldMaskClock;
    std::vector<std::uint8_t> selectedStaticFields =
        functionAnalysis.staticFieldDeclarationMask(
            definitions, usePartitions, usePartitions && ordinal == 0);
    telemetry.staticFieldMaskMicros = staticFieldMaskClock.elapsedMicros();
    telemetry.staticFieldCount = static_cast<std::size_t>(std::count(
        selectedStaticFields.begin(), selectedStaticFields.end(), std::uint8_t{1U}));
    return selectedStaticFields;
}

bool restorePreLlvmObject(
    const CompilationArtifactCache* cache,
    bool usePreLlvmCache,
    const std::string& cacheKey,
    const std::filesystem::path& objectFile,
    CodeUnitTelemetry& telemetry,
    std::uint8_t& cacheHit,
    std::uint8_t& cacheMiss,
    std::string& moduleError,
    const std::function<void(const basic::CompilationEvent&)>& progressSink,
    const std::string& moduleName,
    bool& complete)
{
    complete = false;
    if (!usePreLlvmCache) return true;
    bool hit = false;
    CacheMissReason reason = CacheMissReason::Cold;
    const PhaseClock lookup;
    if (!cache->restoreObject(cacheKey, objectFile, hit, moduleError, &reason)) return false;
    telemetry.lookupMicros += lookup.elapsedMicros();
    telemetry.missReason = reason;
    if (hit)
    {
        const PhaseClock verification;
        CoffObjectVerifier verifier;
        if (verifier.verify(objectFile))
        {
            telemetry.verificationMicros += verification.elapsedMicros();
            cacheHit = 1;
            if (progressSink)
                progressSink({basic::CompilationEventKind::CacheHit,
                    "LLVM", moduleName, 0, 0, {}});
            complete = true;
            return true;
        }
        telemetry.verificationMicros += verification.elapsedMicros();
    }
    cacheMiss = 1;
    if (progressSink)
        progressSink({basic::CompilationEventKind::CacheMiss,
            "LLVM", moduleName, 0, 0, {}});
    return true;
}

std::unique_ptr<LlvmModule> lowerCodeUnit(
    std::size_t ordinal,
    bool usePartitions,
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const std::uint8_t> reachableDefinitions,
    std::span<const std::uint8_t> selectedStaticFields,
    std::span<const std::size_t> coordinatorFunctionDeclarations,
    const ProjectOopCodePartition& partition,
    std::string& moduleError,
    CodeUnitTelemetry& telemetry)
{
    ProjectOopLlvmLowerer lowerer;
    const PhaseClock lowering;
    std::unique_ptr<LlvmModule> module;
    if (!usePartitions)
        module = lowerer.lower(ProjectOopLlvmLoweringRequest::withDebug(
            nameLookup, memberModel, mirModel, sourceManager, debugInformation,
            optimizationLevel, debugPrefixMaps, reachableDefinitions, selectedStaticFields));
    else if (ordinal == 0)
        module = lowerer.lower(ProjectOopLlvmLoweringRequest::partition(
            nameLookup, memberModel, mirModel, sourceManager, debugInformation,
            optimizationLevel, {}, true, "NativeSharp.OOP.globals", debugPrefixMaps,
            coordinatorFunctionDeclarations, reachableDefinitions, selectedStaticFields));
    else
    {
        const ProjectOopCodeUnit& unit = partition.units()[ordinal - 1];
        module = lowerer.lower(ProjectOopLlvmLoweringRequest::partition(
            nameLookup, memberModel, mirModel, sourceManager, debugInformation,
            optimizationLevel, unit.functionOrdinals, false,
            "NativeSharp.OOP." + unit.stableKey, debugPrefixMaps,
            unit.declarationOrdinals, reachableDefinitions, selectedStaticFields));
    }
    telemetry.loweringMicros += lowering.elapsedMicros();
    if (module == nullptr || !lowerer.error().empty())
    {
        moduleError = "lowering failed";
        if (!lowerer.error().empty()) moduleError += ": " + lowerer.error();
        return nullptr;
    }
    return module;
}

std::unique_ptr<LlvmTargetMachine> prepareCodeUnitTarget(
    LlvmModule& module,
    LlvmOptimizationLevel optimizationLevel,
    bool requiresWholeModuleRequiredInlining,
    std::string& moduleError,
    CodeUnitTelemetry& telemetry)
{
    const PhaseClock targetSetup;
    std::unique_ptr<LlvmTargetMachine> targetMachine =
        LlvmTargetMachine::createWindowsX64(optimizationLevel, moduleError);
    const bool configured = targetMachine != nullptr &&
        targetMachine->configureModuleForTarget(module, moduleError);
    telemetry.targetMicros += targetSetup.elapsedMicros();
    if (!configured)
    {
        moduleError = moduleError.empty()
            ? "target configuration failed"
            : "target configuration failed: " + moduleError;
        return nullptr;
    }
    LlvmOptimizer optimizer;
    const PhaseClock optimization;
    if (!optimizer.optimizePrepared(
            module, *targetMachine, optimizationLevel,
            requiresWholeModuleRequiredInlining, nullptr, moduleError))
    {
        telemetry.optimizationMicros += optimization.elapsedMicros();
        moduleError = moduleError.empty()
            ? "optimization failed"
            : "optimization failed: " + moduleError;
        return nullptr;
    }
    telemetry.optimizationMicros += optimization.elapsedMicros();
    return targetMachine;
}

bool restorePostLlvmObject(
    const CompilationArtifactCache* cache,
    bool usePreLlvmCache,
    const LlvmTargetMachine& targetMachine,
    const std::string& codeGenerationPolicy,
    LlvmOptimizationLevel optimizationLevel,
    DebugInformationKind debugInformation,
    const std::filesystem::path& objectFile,
    const std::string& moduleText,
    std::string& cacheKey,
    CodeUnitTelemetry& telemetry,
    std::uint8_t& cacheHit,
    std::uint8_t& cacheMiss,
    std::string& moduleError,
    bool& complete)
{
    complete = false;
    if (cache == nullptr || !cache->enabled() || usePreLlvmCache) return true;
    const PhaseClock fingerprint;
    cacheKey = cache->objectKey(
        moduleText, targetMachine.triple(), targetMachine.dataLayout(),
        codeGenerationPolicy, optimizationLevel, debugInformation);
    telemetry.fingerprintMicros += fingerprint.elapsedMicros();
    bool hit = false;
    CacheMissReason reason = CacheMissReason::Cold;
    const PhaseClock lookup;
    if (!cache->restoreObject(cacheKey, objectFile, hit, moduleError, &reason)) return false;
    telemetry.lookupMicros += lookup.elapsedMicros();
    telemetry.missReason = reason;
    if (hit)
    {
        const PhaseClock verification;
        CoffObjectVerifier verifier;
        if (verifier.verify(objectFile))
        {
            telemetry.verificationMicros += verification.elapsedMicros();
            cacheHit = 1;
            complete = true;
            return true;
        }
        telemetry.verificationMicros += verification.elapsedMicros();
    }
    cacheMiss = 1;
    return true;
}

void emitAndStoreCodeUnit(
    LlvmModule& module,
    LlvmTargetMachine& targetMachine,
    const CompilationArtifactCache* cache,
    const std::string& cacheKey,
    const std::filesystem::path& objectFile,
    CodeUnitTelemetry& telemetry,
    std::string& moduleError)
{
    const PhaseClock emission;
    if (!targetMachine.emitPrepared(
            module, objectFile, LlvmCodeGenFileType::Object, moduleError))
    {
        telemetry.emissionMicros += emission.elapsedMicros();
        return;
    }
    telemetry.emissionMicros += emission.elapsedMicros();
    CoffObjectVerifier verifier;
    const PhaseClock verification;
    if (!verifier.verify(objectFile)) moduleError = verifier.error();
    telemetry.verificationMicros += verification.elapsedMicros();
    if (!moduleError.empty() || cache == nullptr || !cache->enabled()) return;
    const PhaseClock store;
    cache->storeObject(cacheKey, objectFile, moduleError);
    telemetry.storeMicros += store.elapsedMicros();
}

void processObjectCodeUnit(
    std::size_t ordinal,
    bool usePartitions,
    bool usePreLlvmCache,
    bool preserveModuleText,
    bool requiresWholeModuleRequiredInlining,
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const std::uint8_t> reachableDefinitions,
    std::span<const std::size_t> coordinatorFunctionDeclarations,
    const ProjectOopCodePartition& partition,
    const ProjectOopFunctionReachabilityAnalysis& functionAnalysis,
    const CompilationArtifactCache* cache,
    const std::string& codeGenerationPolicy,
    const std::filesystem::path& objectFile,
    std::string& moduleText,
    std::string& moduleError,
    std::string cacheKey,
    std::uint8_t& cacheHit,
    std::uint8_t& cacheMiss,
    CodeUnitTelemetry& telemetry,
    ColdLlvmTrace& coldLlvmTrace,
    const std::function<void(const basic::CompilationEvent&)>& progressSink,
    std::atomic_size_t& completedModules,
    std::mutex& moduleProgressMutex,
    std::size_t moduleCount)
{
    std::string moduleName;
    const std::vector<std::uint8_t> selectedStaticFields = prepareCodeUnitTelemetry(
        ordinal, usePartitions, mirModel, partition, coordinatorFunctionDeclarations,
        functionAnalysis, telemetry, moduleName);
    const ColdLlvmTraceScope coldTraceScope(
        coldLlvmTrace, telemetry, cacheHit, cacheMiss, moduleError);
    if (cache == nullptr || !cache->enabled()) telemetry.missReason = CacheMissReason::Disabled;
    const CodeUnitProgress progress(
        progressSink, completedModules, moduleProgressMutex, moduleCount, moduleName);
    bool complete = false;
    if (!restorePreLlvmObject(
            cache, usePreLlvmCache, cacheKey, objectFile, telemetry,
            cacheHit, cacheMiss, moduleError, progressSink, moduleName, complete) || complete) return;
    std::unique_ptr<LlvmModule> module = lowerCodeUnit(
        ordinal, usePartitions, nameLookup, memberModel, mirModel, sourceManager,
        debugInformation, optimizationLevel, debugPrefixMaps, reachableDefinitions,
        selectedStaticFields, coordinatorFunctionDeclarations, partition, moduleError, telemetry);
    if (module == nullptr) return;
    std::unique_ptr<LlvmTargetMachine> targetMachine = prepareCodeUnitTarget(
        *module, optimizationLevel, requiresWholeModuleRequiredInlining, moduleError, telemetry);
    if (targetMachine == nullptr) return;
    if (preserveModuleText) moduleText = module->text();
    if (!restorePostLlvmObject(
            cache, usePreLlvmCache, *targetMachine, codeGenerationPolicy,
            optimizationLevel, debugInformation, objectFile, moduleText, cacheKey,
            telemetry, cacheHit, cacheMiss, moduleError, complete) || complete) return;
    emitAndStoreCodeUnit(
        *module, *targetMachine, cache, cacheKey, objectFile, telemetry, moduleError);
}

bool buildObjectPartition(
    const frontend::ProjectOopMirModel& mirModel,
    std::size_t partitionCount,
    LinkTimeOptimizationMode linkTimeOptimization,
    std::span<const std::uint8_t> reachableDefinitions,
    const CompilationArtifactCache* cache,
    ProjectOopCodePartition& partition,
    std::string& partitionPlanKey,
    std::string& partitionPlanPayload,
    ColdLlvmPreambleTelemetry& telemetry,
    std::string& error)
{
    bool partitionPlanHit = false;
    if (cache != nullptr && cache->enabled())
    {
        const PhaseClock partitionKeyClock;
        partitionPlanKey = ProjectOopCodePartition::persistentPlanKey(
            mirModel, partitionCount, reachableDefinitions);
        telemetry.partitionKeyMicros = partitionKeyClock.elapsedMicros();
        const PhaseClock partitionRestoreClock;
        const bool restored = cache->restoreCodePartitionPlan(
            partitionPlanKey, partitionPlanPayload, partitionPlanHit, error);
        telemetry.partitionRestoreMicros = partitionRestoreClock.elapsedMicros();
        if (!restored) return false;
    }
    const ProjectOopCodePartitionPolicy policy = cache != nullptr && cache->enabled()
        ? ProjectOopCodePartitionPolicy::CacheAwareBalanced
        : linkTimeOptimization == LinkTimeOptimizationMode::Thin
            ? ProjectOopCodePartitionPolicy::StableIncremental
            : ProjectOopCodePartitionPolicy::Balanced;
    const PhaseClock partitionBuildClock;
    const bool built = partition.build(
        mirModel, partitionCount, error, policy,
        partitionPlanHit ? partitionPlanPayload : std::string_view{}, reachableDefinitions);
    telemetry.partitionBuildMicros = partitionBuildClock.elapsedMicros();
    return built;
}

bool requiresWholeModuleInlining(
    const frontend::ProjectOopMirModel& mirModel,
    std::span<const std::uint8_t> reachableDefinitions)
{
    for (std::size_t ordinal = 0; ordinal < mirModel.functions().size(); ++ordinal)
    {
        const frontend::ProjectOopMirFunction& function = mirModel.functions()[ordinal];
        if (function.inliningPolicy == frontend::InliningPolicy::Required &&
            (!function.isTransportedInlineBody ||
             (ordinal < reachableDefinitions.size() && reachableDefinitions[ordinal] != 0U)))
            return true;
    }
    return false;
}

std::vector<std::size_t> collectCacheFunctionOrdinals(
    const frontend::ProjectOopMirModel& mirModel,
    std::span<const std::uint8_t> reachableDefinitions)
{
    std::vector<std::size_t> ordinals;
    ordinals.reserve(mirModel.functions().size());
    for (std::size_t ordinal = 0; ordinal < mirModel.functions().size(); ++ordinal)
    {
        const frontend::ProjectOopMirFunction& function = mirModel.functions()[ordinal];
        if (function.isTransportedInlineBody &&
            (ordinal >= reachableDefinitions.size() || reachableDefinitions[ordinal] == 0U))
            continue;
        ordinals.push_back(ordinal);
    }
    return ordinals;
}

void composeModuleCacheKeys(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::span<const std::uint8_t> reachableDefinitions,
    std::span<const std::size_t> coordinatorFunctionDeclarations,
    const ProjectOopCodePartition& partition,
    bool usePartitions,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    const CompilationArtifactCache& cache,
    const std::string& codeGenerationPolicy,
    std::vector<std::string>& cacheKeys,
    std::vector<CodeUnitTelemetry>& codeUnitTelemetry,
    ColdLlvmPreambleTelemetry& telemetry)
{
    const PhaseClock identityIndexClock;
    const ProjectOopMirCacheIdentityIndex identityIndex(mirModel);
    telemetry.identityIndexBuildMicros = identityIndexClock.elapsedMicros();
    std::string lineTableSourceIdentity;
    if (debugInformation == DebugInformationKind::LineTablesOnly)
    {
        const PhaseClock lineTableIdentityClock;
        const std::vector<SourceRange> sourceRanges =
            collectLineTableSourceRanges(nameLookup, memberModel, mirModel);
        lineTableSourceIdentity = projectOopLineTableSourceIdentity(
            sourceManager, debugPrefixMaps, sourceRanges);
        telemetry.lineTableIdentityMicros = lineTableIdentityClock.elapsedMicros();
    }
    const std::vector<std::size_t> allFunctions =
        collectCacheFunctionOrdinals(mirModel, reachableDefinitions);
    const PhaseClock moduleKeyComposeClock;
    for (std::size_t ordinal = 0; ordinal < cacheKeys.size(); ++ordinal)
    {
        std::span<const std::size_t> definitions;
        std::span<const std::size_t> declarations;
        if (!usePartitions)
        {
            definitions = allFunctions;
            declarations = allFunctions;
        }
        else if (ordinal == 0)
        {
            declarations = coordinatorFunctionDeclarations;
        }
        else
        {
            const ProjectOopCodeUnit& unit = partition.units()[ordinal - 1];
            definitions = unit.functionOrdinals;
            declarations = unit.declarationOrdinals;
        }
        const PhaseClock fingerprint;
        std::string identity = identityIndex.identity(definitions, declarations);
        if (debugInformation == DebugInformationKind::LineTablesOnly)
            identity = combineLineTableCacheIdentity(
                std::move(identity), lineTableSourceIdentity);
        cacheKeys[ordinal] = cache.inputObjectKey(
            identity, "windows-x64-msvc", codeGenerationPolicy,
            optimizationLevel, debugInformation);
        codeUnitTelemetry[ordinal].fingerprintMicros += fingerprint.elapsedMicros();
    }
    telemetry.moduleKeyComposeMicros = moduleKeyComposeClock.elapsedMicros();
}

bool executeCodeUnits(
    std::size_t workerCount,
    std::size_t moduleCount,
    bool usePartitions,
    const FloatingPointState& floatingPointState,
    const std::function<void(std::size_t)>& processModule,
    const std::vector<std::string>& moduleErrors,
    std::string& error)
{
    const auto initializeWorkerFloatingPoint = [floatingPointState](std::size_t) {
        initializeFloatingPointStateForWorker(floatingPointState);
    };
    if (!usePartitions)
    {
        processModule(0);
        return true;
    }
    driver::CompilationWorkerPool workers(workerCount, initializeWorkerFloatingPoint);
    std::atomic_bool moduleFailure = false;
    for (std::size_t ordinal = 0; ordinal < moduleCount; ++ordinal)
    {
        workers.submit([&, ordinal](std::size_t) {
            if (moduleFailure.load(std::memory_order_acquire)) return;
            processModule(ordinal);
            if (!moduleErrors[ordinal].empty())
            {
                moduleFailure.store(true, std::memory_order_release);
                workers.requestCancellation();
            }
        });
    }
    try
    {
        workers.wait();
    }
    catch (const std::exception& exception)
    {
        error = "LLVM worker failed: " + std::string(exception.what());
        return false;
    }
    catch (...)
    {
        error = "LLVM worker failed with a non-standard exception";
        return false;
    }
    return true;
}

bool summarizeCodeUnits(
    const std::vector<std::string>& moduleErrors,
    const std::vector<std::uint8_t>& cacheHits,
    const std::vector<std::uint8_t>& cacheMisses,
    std::size_t& cacheHitCount,
    std::size_t& cacheMissCount,
    std::string& error)
{
    for (std::size_t ordinal = 0; ordinal < moduleErrors.size(); ++ordinal)
    {
        if (!moduleErrors[ordinal].empty())
        {
            error = "LLVM code unit #" + std::to_string(ordinal) +
                " failed: " + moduleErrors[ordinal];
            return false;
        }
        cacheHitCount += cacheHits[ordinal];
        cacheMissCount += cacheMisses[ordinal];
    }
    return true;
}

void recordPartitionCosts(
    ProjectOopCodePartition& partition,
    const std::vector<std::uint8_t>& cacheHits,
    const std::vector<CodeUnitTelemetry>& telemetry)
{
    // NSC305-L01. Record the observed per-unit cost into the partition's bounded historical weight.
    for (std::size_t ordinal = 0; ordinal < partition.units().size(); ++ordinal)
    {
        if (cacheHits[ordinal + 1] != 0U) continue;
        const CodeUnitTelemetry& unit = telemetry[ordinal + 1];
        const std::uint64_t observed = unit.fingerprintMicros + unit.lookupMicros +
            unit.loweringMicros + unit.targetMicros + unit.optimizationMicros +
            unit.emissionMicros + unit.verificationMicros + unit.storeMicros;
        partition.recordCodeUnitCost(partition.units()[ordinal].ordinal, observed);
    }
}

bool storePartitionPlan(
    ProjectOopCodePartition& partition,
    const CompilationArtifactCache& cache,
    const std::string& partitionPlanKey,
    std::string& partitionPlanPayload,
    ColdLlvmPreambleTelemetry& telemetry,
    std::string& error)
{
    partitionPlanPayload = partition.persistentPlanPayload();
    if (partitionPlanPayload.empty())
    {
        error = "cache-aware code partition did not produce a persistent plan";
        return false;
    }
    const PhaseClock planStoreClock;
    const bool stored = cache.storeCodePartitionPlan(
        partitionPlanKey, partitionPlanPayload, error);
    telemetry.planStoreMicros = planStoreClock.elapsedMicros();
    return stored;
}

std::string combineModuleTexts(const std::vector<std::string>& moduleTexts)
{
    std::string combined;
    for (const std::string& moduleText : moduleTexts)
    {
        combined += moduleText;
        combined += '\n';
    }
    return combined;
}

std::string thinLtoIndexDigest(const std::vector<std::string>& moduleTexts)
{
    // The index is independent of temporary paths, scheduling and cache state. Each optimized
    // partition contributes in stable ordinal order, matching ThinLTO's summary-before-backend
    // contract while native object generation remains embarrassingly parallel.
    std::string index = "NativeSharp.ThinLTO.Index.v1\n";
    for (std::size_t ordinal = 0; ordinal < moduleTexts.size(); ++ordinal)
    {
        const auto bytes = std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(moduleTexts[ordinal].data()),
            moduleTexts[ordinal].size());
        index += std::to_string(ordinal) + ":" +
            basic::sha256Hex(basic::sha256(bytes)) + "\n";
    }
    const auto bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(index.data()), index.size());
    return basic::sha256Hex(basic::sha256(bytes));
}

}

void ProjectOopObjectGenerator::setProgressSink(
    std::function<void(const basic::CompilationEvent&)> sink)
{
    progressSink_ = std::move(sink);
}

ProjectOopObjectBundle::ProjectOopObjectBundle(
    std::filesystem::path temporaryDirectory) noexcept
    : temporaryDirectory_(std::move(temporaryDirectory))
{
}

ProjectOopObjectBundle::~ProjectOopObjectBundle()
{
    if (temporaryDirectory_.empty()) return;
    std::error_code error;
    std::filesystem::remove_all(temporaryDirectory_, error);
}

std::span<const std::filesystem::path> ProjectOopObjectBundle::objectFiles() const noexcept
{
    return objectFiles_;
}

const std::string& ProjectOopObjectBundle::combinedLlvmText() const noexcept
{
    return combinedLlvmText_;
}

std::size_t ProjectOopObjectBundle::codeUnitCount() const noexcept
{
    return codeUnitCount_;
}

bool ProjectOopObjectBundle::usedParallelOptimization() const noexcept
{
    return usedParallelOptimization_;
}

std::size_t ProjectOopObjectBundle::cacheHitCount() const noexcept
{
    return cacheHitCount_;
}

std::size_t ProjectOopObjectBundle::cacheMissCount() const noexcept
{
    return cacheMissCount_;
}

std::size_t ProjectOopObjectBundle::thinLtoSummaryCount() const noexcept
{
    return thinLtoSummaryCount_;
}

const std::string& ProjectOopObjectBundle::thinLtoIndexDigest() const noexcept
{
    return thinLtoIndexDigest_;
}

std::span<const CodeUnitTelemetry> ProjectOopObjectBundle::codeUnitTelemetry() const noexcept
{
    return codeUnitTelemetry_;
}

bool ProjectOopObjectGenerator::finishCodeUnitBundle(
    ProjectOopObjectBundle& result,
    bool usePartitions,
    bool preserveModuleText,
    LinkTimeOptimizationMode linkTimeOptimization,
    ProjectOopCodePartition& partition,
    const CompilationArtifactCache* cache,
    const std::string& partitionPlanKey,
    std::string& partitionPlanPayload,
    std::vector<std::string>& moduleTexts,
    const std::vector<std::string>& moduleErrors,
    const std::vector<std::uint8_t>& cacheHits,
    const std::vector<std::uint8_t>& cacheMisses,
    std::vector<CodeUnitTelemetry>& codeUnitTelemetry,
    ColdLlvmPreambleTelemetry& preambleTelemetry,
    std::string& error) const
{
    if (!summarizeCodeUnits(
            moduleErrors, cacheHits, cacheMisses,
            result.cacheHitCount_, result.cacheMissCount_, error)) return false;
    if (usePartitions) recordPartitionCosts(partition, cacheHits, codeUnitTelemetry);
    if (usePartitions && cache != nullptr && cache->enabled() &&
        !storePartitionPlan(
            partition, *cache, partitionPlanKey, partitionPlanPayload,
            preambleTelemetry, error)) return false;
    result.codeUnitTelemetry_ = std::move(codeUnitTelemetry);
    if (preserveModuleText) result.combinedLlvmText_ = combineModuleTexts(moduleTexts);
    if (linkTimeOptimization == LinkTimeOptimizationMode::Thin)
    {
        result.thinLtoSummaryCount_ = moduleTexts.size();
        result.thinLtoIndexDigest_ = thinLtoIndexDigest(moduleTexts);
    }
    return true;
}

bool ProjectOopObjectGenerator::generateCodeUnits(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::size_t workerCount,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    LinkTimeOptimizationMode linkTimeOptimization,
    const CompilationArtifactCache* cache,
    const ProjectOopFunctionReachabilityAnalysis& functionAnalysis,
    std::span<const std::uint8_t> reachableDefinitions,
    std::span<const std::size_t> coordinatorFunctionDeclarations,
    ProjectOopCodePartition& partition,
    const std::string& partitionPlanKey,
    std::string& partitionPlanPayload,
    bool requiresWholeModuleRequiredInlining,
    bool usePartitions,
    ProjectOopObjectBundle& result,
    ColdLlvmPreambleTelemetry& preambleTelemetry,
    std::string& error) const
{
    const std::size_t moduleCount = result.objectFiles_.size();
    std::vector<std::string> moduleTexts(moduleCount);
    std::vector<std::string> moduleErrors(moduleCount);
    std::vector<std::uint8_t> cacheHits(moduleCount, 0);
    std::vector<std::uint8_t> cacheMisses(moduleCount, 0);
    std::vector<CodeUnitTelemetry> codeUnitTelemetry(moduleCount);
    const FloatingPointState workerFloatingPointState = captureFloatingPointState();
    const std::string codeGenerationPolicy = codeGenerationPolicyIdentity(linkTimeOptimization);
    const bool usePreLlvmCache = cache != nullptr && cache->enabled() &&
        debugInformation != DebugInformationKind::Full &&
        linkTimeOptimization != LinkTimeOptimizationMode::Thin;
    ColdLlvmTrace coldLlvmTrace(
        kCodegenCacheIdentity, functionAnalysis.projectFingerprint(), debugInformation,
        optimizationLevel, moduleCount, usePartitions, usePreLlvmCache,
        cache != nullptr ? cache->lastPruneTelemetry() : CachePruneTelemetry{},
        preambleTelemetry);
    std::vector<std::string> cacheKeys(moduleCount);
    if (usePreLlvmCache)
        composeModuleCacheKeys(
            nameLookup, memberModel, mirModel, sourceManager, debugPrefixMaps,
            reachableDefinitions, coordinatorFunctionDeclarations, partition, usePartitions,
            debugInformation, optimizationLevel, *cache, codeGenerationPolicy,
            cacheKeys, codeUnitTelemetry, preambleTelemetry);
    // Textual IR used to be serialized for every code unit even though the Windows import
    // planner no longer consumes it.  Keep it only for the two features whose keys/summaries
    // are intentionally based on IR.  Ordinary EXE/DLL/LIB production already fingerprints
    // the emitted objects when incremental linking is enabled.
    const bool preserveModuleText =
        (cache != nullptr && cache->enabled() && !usePreLlvmCache) ||
        linkTimeOptimization == LinkTimeOptimizationMode::Thin;
    // Process one code unit end to end. The LLVM module and target machine are destroyed as soon
    // as its object is emitted instead of retaining every partition through three global waves.
    std::atomic_size_t completedModules = 0;
    std::mutex moduleProgressMutex;
    const auto processModule = [&](std::size_t ordinal) {
        processObjectCodeUnit(
            ordinal, usePartitions, usePreLlvmCache, preserveModuleText,
            requiresWholeModuleRequiredInlining, nameLookup, memberModel, mirModel,
            sourceManager, debugInformation, optimizationLevel, debugPrefixMaps,
            reachableDefinitions, coordinatorFunctionDeclarations, partition,
            functionAnalysis, cache, codeGenerationPolicy, result.objectFiles_[ordinal],
            moduleTexts[ordinal], moduleErrors[ordinal], cacheKeys[ordinal],
            cacheHits[ordinal], cacheMisses[ordinal], codeUnitTelemetry[ordinal],
            coldLlvmTrace, progressSink_, completedModules, moduleProgressMutex, moduleCount);
    };
    const PhaseClock workerWallClock;
    if (!executeCodeUnits(
            workerCount, moduleCount, usePartitions, workerFloatingPointState,
            processModule, moduleErrors, error)) return false;
    preambleTelemetry.workerWallMicros = workerWallClock.elapsedMicros();
    for (const CodeUnitTelemetry& telemetry : codeUnitTelemetry)
        preambleTelemetry.staticFieldMaskMicros += telemetry.staticFieldMaskMicros;
    return finishCodeUnitBundle(
        result, usePartitions, preserveModuleText, linkTimeOptimization, partition,
        cache, partitionPlanKey, partitionPlanPayload, moduleTexts, moduleErrors,
        cacheHits, cacheMisses, codeUnitTelemetry, preambleTelemetry, error);
}

std::unique_ptr<ProjectOopObjectBundle> ProjectOopObjectGenerator::generate(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::size_t workerCount,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    std::string& error) const
{
    return generate(nameLookup, memberModel, mirModel, sourceManager, debugInformation,
        optimizationLevel, workerCount, debugPrefixMaps,
        LinkTimeOptimizationMode::None, error);
}

std::unique_ptr<ProjectOopObjectBundle> ProjectOopObjectGenerator::generate(
    const frontend::ProjectNameLookup& nameLookup,
    const frontend::ProjectMemberModel& memberModel,
    const frontend::ProjectOopMirModel& mirModel,
    const source::SourceManager& sourceManager,
    DebugInformationKind debugInformation,
    LlvmOptimizationLevel optimizationLevel,
    std::size_t workerCount,
    std::span<const DebugPrefixMap> debugPrefixMaps,
    LinkTimeOptimizationMode linkTimeOptimization,
    std::string& error) const
{
    error.clear();
    workerCount = std::max<std::size_t>(1, workerCount);
    // NSC305-L06D. At O0, using the requested worker count as the code-unit count
    // over-partitions cold builds: beyond eight units the duplicated LLVM setup,
    // emission, verification and cache publication exceed the scheduling gain.
    // Keep the requested worker pool, but cap only the number of O0 partitions.
    constexpr std::size_t maxColdO0CodeUnits = 8;
    const std::size_t partitionCount = optimizationLevel == LlvmOptimizationLevel::O0
        ? std::min(workerCount, maxColdO0CodeUnits)
        : workerCount;
    const CompilationArtifactCache* const cache = activeCompilationArtifactCache();
    ColdLlvmPreambleTelemetry preambleTelemetry;
    const PhaseClock reachabilityClock;
    const ProjectOopFunctionReachabilityAnalysis functionAnalysis(mirModel);
    preambleTelemetry.reachabilityMicros = reachabilityClock.elapsedMicros();
    const std::span<const std::uint8_t> reachableDefinitions =
        functionAnalysis.reachableDefinitions();

    ProjectOopCodePartition partition;
    std::string partitionPlanKey;
    std::string partitionPlanPayload;
    if (!buildObjectPartition(
            mirModel, partitionCount, linkTimeOptimization, reachableDefinitions, cache,
            partition, partitionPlanKey, partitionPlanPayload, preambleTelemetry, error))
        return nullptr;
    const PhaseClock coordinatorClosureClock;
    const std::vector<std::size_t> coordinatorFunctionDeclarations =
        functionAnalysis.coordinatorFunctionDeclarations();
    preambleTelemetry.coordinatorClosureMicros = coordinatorClosureClock.elapsedMicros();
    // LLVM can only inline a call while caller and callee coexist in the same module. Until
    // code-unit summary import is available, splitting a full-debug optimized build silently
    // removes its CodeView/PDB inline scopes. More importantly, splitting any build containing a
    // Required callable can leave a forceinline call unexpanded -- including O0, where the
    // mandatory inliner otherwise works correctly. Keep both profiles monolithic. Projects with
    // no forceinline obligation retain their existing partitioned O0 and lightweight-debug paths.
    const bool requiresWholeModuleDebugInlining =
        debugInformation == DebugInformationKind::Full &&
        optimizationLevel != LlvmOptimizationLevel::O0;
    const bool requiresWholeModuleRequiredInlining =
        requiresWholeModuleInlining(mirModel, reachableDefinitions);
    // Full LTO deliberately keeps the complete program in one LLVM module. This makes
    // inter-unit inlining, global DCE and constant propagation happen before native object
    // emission instead of asking the PE linker to recover information already discarded.
    const bool usePartitions = linkTimeOptimization != LinkTimeOptimizationMode::Full &&
        workerCount > 1 && partition.units().size() > 1 &&
        !requiresWholeModuleDebugInlining && !requiresWholeModuleRequiredInlining;
    const std::size_t moduleCount = usePartitions ? partition.units().size() + 1 : 1;
    const bool parallelOptimization = usePartitions && optimizationLevel != LlvmOptimizationLevel::O0;
    const std::optional<std::filesystem::path> temporaryDirectory =
        createTemporaryDirectory(error);
    if (!temporaryDirectory.has_value()) return nullptr;
    auto result = std::unique_ptr<ProjectOopObjectBundle>(
        new ProjectOopObjectBundle(*temporaryDirectory));
    result->codeUnitCount_ = usePartitions ? partition.units().size() : 1;
    result->usedParallelOptimization_ = parallelOptimization;
    result->objectFiles_.reserve(moduleCount);
    for (std::size_t ordinal = 0; ordinal < moduleCount; ++ordinal)
        result->objectFiles_.push_back(
            *temporaryDirectory / ("program-" + std::to_string(ordinal) + ".obj"));

    if (!generateCodeUnits(
            nameLookup, memberModel, mirModel, sourceManager, debugInformation,
            optimizationLevel, workerCount, debugPrefixMaps, linkTimeOptimization, cache,
            functionAnalysis, reachableDefinitions, coordinatorFunctionDeclarations, partition,
            partitionPlanKey, partitionPlanPayload, requiresWholeModuleRequiredInlining,
            usePartitions, *result, preambleTelemetry, error)) return nullptr;
    return result;
}
}
