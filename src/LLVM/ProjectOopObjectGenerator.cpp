#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"

#include "nativesharp/LLVM/FloatingPointMode.h"

#include "nativesharp/Basic/Sha256.h"

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
class CodeUnitProgress final
{
public:
    CodeUnitProgress(
        const std::function<void(const basic::CompilationEvent&)>& sink,
        std::atomic_size_t& completed,
        std::mutex& mutex,
        std::size_t total,
        std::string name)
        : sink_(sink), completed_(completed), mutex_(mutex), total_(total),
          name_(std::move(name))
    {
        if (sink_)
            sink_({basic::CompilationEventKind::WorkStarted,
                "LLVM", name_, 0, 0, {}});
    }

    ~CodeUnitProgress()
    {
        if (!sink_) return;
        const std::lock_guard<std::mutex> lock(mutex_);
        const std::size_t current = completed_.fetch_add(1, std::memory_order_relaxed) + 1U;
        sink_({basic::CompilationEventKind::WorkFinished,
            "LLVM", name_, current, total_});
    }

private:
    const std::function<void(const basic::CompilationEvent&)>& sink_;
    std::atomic_size_t& completed_;
    std::mutex& mutex_;
    std::size_t total_;
    std::string name_;
};

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

// NSC305-P02. Monotonic wall timer for per-code-unit phase attribution. Zero allocation; the
// elapsed value is read explicitly so each phase can be summed without touching control flow.
class PhaseClock final
{
public:
    PhaseClock() : started_(std::chrono::steady_clock::now()) {}
    std::uint64_t elapsedMicros() const
    {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started_).count());
    }

private:
    std::chrono::steady_clock::time_point started_;
};

// INSTRUMENTATION CODE: NSC305-L06. Temporary, file-backed attribution for GUI builds that
// cannot add --time. Keep this marker until the real-project cold LLVM bottleneck is corrected.
class ColdLlvmTrace final
{
public:
    ColdLlvmTrace(
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimization,
        std::size_t moduleCount,
        bool partitioned,
        bool preLlvmCache,
        CachePruneTelemetry pruneTelemetry)
        : output_(R"(A:\temp\nsc-llvm-code-unit-timing.log)",
              std::ios::binary | std::ios::trunc)
    {
        if (!output_) return;
        output_ << "NSC305-L06 LLVM CODE UNIT INSTRUMENTATION version=2\n"
                << "debug=" << debugInformationKindName(debugInformation)
                << " optimization=" << llvmOptimizationLevelName(optimization)
                << " modules=" << moduleCount
                << " partitioned=" << (partitioned ? 1 : 0)
                << " pre-llvm-cache=" << (preLlvmCache ? 1 : 0)
                << " prune-performed=" << (pruneTelemetry.performed ? 1 : 0)
                << " prune-us=" << pruneTelemetry.elapsedMicros
                << " prune-entries=" << pruneTelemetry.inspectedEntries
                << " prune-bytes=" << pruneTelemetry.inspectedBytes
                << " prune-evicted-entries=" << pruneTelemetry.evictedEntries
                << " prune-evicted-bytes=" << pruneTelemetry.evictedBytes << '\n';
        output_.flush();
    }

    void record(
        const CodeUnitTelemetry& telemetry,
        bool hit,
        bool miss,
        std::string_view error)
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (!output_) return;
        std::string oneLineError(error);
        std::ranges::replace(oneLineError, '\r', ' ');
        std::ranges::replace(oneLineError, '\n', ' ');
        output_ << "code-unit " << telemetry.name
                << " hit=" << (hit ? 1 : 0)
                << " miss=" << (miss ? 1 : 0)
                << " miss-reason=" << cacheMissReasonName(telemetry.missReason)
                << " fingerprint-us=" << telemetry.fingerprintMicros
                << " lookup-us=" << telemetry.lookupMicros
                << " lowering-us=" << telemetry.loweringMicros
                << " target-us=" << telemetry.targetMicros
                << " optimization-us=" << telemetry.optimizationMicros
                << " emission-us=" << telemetry.emissionMicros
                << " verification-us=" << telemetry.verificationMicros
                << " store-us=" << telemetry.storeMicros;
        if (!oneLineError.empty()) output_ << " error=" << oneLineError;
        output_ << '\n';
        output_.flush();
    }

private:
    std::mutex mutex_;
    std::ofstream output_;
};

class ColdLlvmTraceScope final
{
public:
    ColdLlvmTraceScope(
        ColdLlvmTrace& trace,
        const CodeUnitTelemetry& telemetry,
        const std::uint8_t& hit,
        const std::uint8_t& miss,
        const std::string& error)
        : trace_(trace), telemetry_(telemetry), hit_(hit), miss_(miss), error_(error)
    {
    }

    ~ColdLlvmTraceScope()
    {
        trace_.record(telemetry_, hit_ != 0U, miss_ != 0U, error_);
    }

    ColdLlvmTraceScope(const ColdLlvmTraceScope&) = delete;
    ColdLlvmTraceScope& operator=(const ColdLlvmTraceScope&) = delete;

private:
    ColdLlvmTrace& trace_;
    const CodeUnitTelemetry& telemetry_;
    const std::uint8_t& hit_;
    const std::uint8_t& miss_;
    const std::string& error_;
};

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
    const CompilationArtifactCache* const cache = activeCompilationArtifactCache();

    ProjectOopCodePartition partition;
    std::string partitionPlanKey;
    std::string partitionPlanPayload;
    bool partitionPlanHit = false;
    if (cache != nullptr && cache->enabled())
    {
        partitionPlanKey = ProjectOopCodePartition::persistentPlanKey(mirModel, workerCount);
        if (!cache->restoreCodePartitionPlan(
                partitionPlanKey, partitionPlanPayload, partitionPlanHit, error))
            return nullptr;
    }
    const ProjectOopCodePartitionPolicy partitionPolicy =
        cache != nullptr && cache->enabled()
        ? ProjectOopCodePartitionPolicy::CacheAwareBalanced
        : linkTimeOptimization == LinkTimeOptimizationMode::Thin
            ? ProjectOopCodePartitionPolicy::StableIncremental
            : ProjectOopCodePartitionPolicy::Balanced;
    if (!partition.build(
            mirModel, workerCount, error, partitionPolicy,
            partitionPlanHit ? partitionPlanPayload : std::string_view{}))
        return nullptr;
    // LLVM can only inline a call while caller and callee coexist in the same module. Until
    // code-unit summary import is available, splitting a full-debug optimized build silently
    // removes its CodeView/PDB inline scopes. More importantly, splitting any build containing a
    // Required callable can leave a forceinline call unexpanded -- including O0, where the
    // mandatory inliner otherwise works correctly. Keep both profiles monolithic. Projects with
    // no forceinline obligation retain their existing partitioned O0 and lightweight-debug paths.
    const bool requiresWholeModuleDebugInlining =
        debugInformation == DebugInformationKind::Full &&
        optimizationLevel != LlvmOptimizationLevel::O0;
    const bool requiresWholeModuleRequiredInlining = std::ranges::any_of(
        mirModel.functions(), [](const frontend::ProjectOopMirFunction& function) {
            return function.inliningPolicy == frontend::InliningPolicy::Required;
        });
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

    std::vector<std::string> moduleTexts(moduleCount);
    std::vector<std::string> moduleErrors(moduleCount);
    std::vector<std::uint8_t> cacheHits(moduleCount, 0);
    std::vector<std::uint8_t> cacheMisses(moduleCount, 0);
    std::vector<CodeUnitTelemetry> codeUnitTelemetry(moduleCount);
    const FloatingPointState workerFloatingPointState = captureFloatingPointState();
    const std::string codeGenerationPolicy =
        codeGenerationPolicyIdentity(linkTimeOptimization);
    const bool usePreLlvmCache = cache != nullptr && cache->enabled() &&
        debugInformation != DebugInformationKind::Full &&
        linkTimeOptimization != LinkTimeOptimizationMode::Thin;
    std::vector<std::string> cacheKeys(moduleCount);
    if (usePreLlvmCache)
    {
        const ProjectOopMirCacheIdentityIndex identityIndex(mirModel);
        std::string lineTableSourceIdentity;
        if (debugInformation == DebugInformationKind::LineTablesOnly)
        {
            const std::vector<SourceRange> sourceRanges =
                collectLineTableSourceRanges(nameLookup, memberModel, mirModel);
            lineTableSourceIdentity = projectOopLineTableSourceIdentity(
                sourceManager, debugPrefixMaps, sourceRanges);
        }
        std::vector<std::size_t> allFunctionOrdinals(mirModel.functions().size());
        for (std::size_t ordinal = 0; ordinal < allFunctionOrdinals.size(); ++ordinal)
            allFunctionOrdinals[ordinal] = ordinal;
        for (std::size_t moduleOrdinal = 0; moduleOrdinal < moduleCount; ++moduleOrdinal)
        {
            std::span<const std::size_t> definitions;
            std::span<const std::size_t> declarations;
            if (!usePartitions)
            {
                definitions = allFunctionOrdinals;
                declarations = allFunctionOrdinals;
            }
            else if (moduleOrdinal == 0)
            {
                declarations = allFunctionOrdinals;
            }
            else
            {
                const ProjectOopCodeUnit& unit = partition.units()[moduleOrdinal - 1];
                definitions = unit.functionOrdinals;
                declarations = unit.declarationOrdinals;
            }
            const PhaseClock fingerprint;
            std::string moduleIdentity = identityIndex.identity(definitions, declarations);
            if (debugInformation == DebugInformationKind::LineTablesOnly)
            {
                moduleIdentity = combineLineTableCacheIdentity(
                    std::move(moduleIdentity), lineTableSourceIdentity);
            }
            cacheKeys[moduleOrdinal] = cache->inputObjectKey(
                moduleIdentity,
                "windows-x64-msvc", codeGenerationPolicy,
                optimizationLevel, debugInformation);
            codeUnitTelemetry[moduleOrdinal].fingerprintMicros += fingerprint.elapsedMicros();
        }
    }
    // Textual IR used to be serialized for every code unit even though the Windows import
    // planner no longer consumes it.  Keep it only for the two features whose keys/summaries
    // are intentionally based on IR.  Ordinary EXE/DLL/LIB production already fingerprints
    // the emitted objects when incremental linking is enabled.
    const bool preserveModuleText =
        (cache != nullptr && cache->enabled() && !usePreLlvmCache) ||
        linkTimeOptimization == LinkTimeOptimizationMode::Thin;

    ColdLlvmTrace coldLlvmTrace(
        debugInformation, optimizationLevel, moduleCount, usePartitions, usePreLlvmCache,
        cache != nullptr ? cache->lastPruneTelemetry() : CachePruneTelemetry{});

    // Process one code unit end to end. The LLVM module and target machine are destroyed as soon
    // as its object is emitted instead of retaining every partition through three global waves.
    std::atomic_size_t completedModules = 0;
    std::mutex moduleProgressMutex;
    const auto processModule = [&](std::size_t ordinal) {
        std::string moduleName = "NativeSharp.OOP";
        if (usePartitions)
            moduleName = ordinal == 0
                ? "NativeSharp.OOP.globals"
                : "NativeSharp.OOP." + partition.units()[ordinal - 1].stableKey;
        CodeUnitTelemetry& telemetry = codeUnitTelemetry[ordinal];
        telemetry.name = moduleName;
        const ColdLlvmTraceScope coldTraceScope(
            coldLlvmTrace, telemetry, cacheHits[ordinal], cacheMisses[ordinal],
            moduleErrors[ordinal]);
        if (cache == nullptr || !cache->enabled())
            telemetry.missReason = CacheMissReason::Disabled;
        const CodeUnitProgress progress(
            progressSink_, completedModules, moduleProgressMutex,
            moduleCount, moduleName);
        if (usePreLlvmCache)
        {
            bool hit = false;
            CacheMissReason reason = CacheMissReason::Cold;
            const PhaseClock lookup;
            if (!cache->restoreObject(
                    cacheKeys[ordinal], result->objectFiles_[ordinal], hit,
                    moduleErrors[ordinal], &reason)) return;
            telemetry.lookupMicros += lookup.elapsedMicros();
            telemetry.missReason = reason;
            if (hit)
            {
                const PhaseClock verification;
                CoffObjectVerifier verifier;
                if (verifier.verify(result->objectFiles_[ordinal]))
                {
                    telemetry.verificationMicros += verification.elapsedMicros();
                    cacheHits[ordinal] = 1;
                    if (progressSink_)
                        progressSink_({basic::CompilationEventKind::CacheHit,
                            "LLVM", moduleName, 0, 0, {}});
                    return;
                }
                telemetry.verificationMicros += verification.elapsedMicros();
            }
            cacheMisses[ordinal] = 1;
            if (progressSink_)
                progressSink_({basic::CompilationEventKind::CacheMiss,
                    "LLVM", moduleName, 0, 0, {}});
        }

        ProjectOopLlvmLowerer lowerer;
        std::unique_ptr<LlvmModule> module;
        const PhaseClock lowering;
        if (!usePartitions)
        {
            module = lowerer.lower(ProjectOopLlvmLoweringRequest::withDebug(
                nameLookup, memberModel, mirModel, sourceManager, debugInformation,
                optimizationLevel, debugPrefixMaps));
        }
        else if (ordinal == 0)
        {
            module = lowerer.lower(ProjectOopLlvmLoweringRequest::partition(
                nameLookup, memberModel, mirModel, sourceManager, debugInformation,
                optimizationLevel, {}, true, "NativeSharp.OOP.globals", debugPrefixMaps));
        }
        else
        {
            const ProjectOopCodeUnit& unit = partition.units()[ordinal - 1];
            module = lowerer.lower(ProjectOopLlvmLoweringRequest::partition(
                nameLookup, memberModel, mirModel, sourceManager, debugInformation,
                optimizationLevel, unit.functionOrdinals, false,
                "NativeSharp.OOP." + unit.stableKey, debugPrefixMaps,
                unit.declarationOrdinals));
        }
        telemetry.loweringMicros += lowering.elapsedMicros();
        if (module == nullptr || !lowerer.error().empty())
        {
            moduleErrors[ordinal] = "lowering failed";
            if (!lowerer.error().empty()) moduleErrors[ordinal] += ": " + lowerer.error();
            return;
        }

        const PhaseClock targetSetup;
        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(optimizationLevel, moduleErrors[ordinal]);
        const bool targetConfigured = targetMachine != nullptr &&
            targetMachine->configureModuleForTarget(*module, moduleErrors[ordinal]);
        telemetry.targetMicros += targetSetup.elapsedMicros();
        if (!targetConfigured)
        {
            if (moduleErrors[ordinal].empty())
                moduleErrors[ordinal] = "target configuration failed";
            else
                moduleErrors[ordinal] = "target configuration failed: " + moduleErrors[ordinal];
            return;
        }

        LlvmOptimizer optimizer;
        const PhaseClock optimization;
        if (!optimizer.optimizePrepared(
                *module, *targetMachine, optimizationLevel,
                requiresWholeModuleRequiredInlining, nullptr, moduleErrors[ordinal]))
        {
            telemetry.optimizationMicros += optimization.elapsedMicros();
            if (moduleErrors[ordinal].empty())
                moduleErrors[ordinal] = "optimization failed";
            else
                moduleErrors[ordinal] = "optimization failed: " + moduleErrors[ordinal];
            return;
        }
        telemetry.optimizationMicros += optimization.elapsedMicros();

        if (preserveModuleText) moduleTexts[ordinal] = module->text();
        std::string cacheKey = cacheKeys[ordinal];
        if (cache != nullptr && cache->enabled() && !usePreLlvmCache)
        {
            const PhaseClock fingerprint;
            cacheKey = cache->objectKey(
                moduleTexts[ordinal], targetMachine->triple(),
                targetMachine->dataLayout(), codeGenerationPolicy, optimizationLevel,
                debugInformation);
            telemetry.fingerprintMicros += fingerprint.elapsedMicros();
            bool hit = false;
            CacheMissReason reason = CacheMissReason::Cold;
            const PhaseClock lookup;
            if (!cache->restoreObject(
                    cacheKey, result->objectFiles_[ordinal], hit,
                    moduleErrors[ordinal], &reason))
                return;
            telemetry.lookupMicros += lookup.elapsedMicros();
            telemetry.missReason = reason;
            if (hit)
            {
                const PhaseClock verification;
                CoffObjectVerifier verifier;
                if (verifier.verify(result->objectFiles_[ordinal]))
                {
                    telemetry.verificationMicros += verification.elapsedMicros();
                    cacheHits[ordinal] = 1;
                    return;
                }
                telemetry.verificationMicros += verification.elapsedMicros();
            }
            cacheMisses[ordinal] = 1;
        }
        const PhaseClock emission;
        if (!targetMachine->emitPrepared(
                *module, result->objectFiles_[ordinal],
                LlvmCodeGenFileType::Object, moduleErrors[ordinal]))
        {
            telemetry.emissionMicros += emission.elapsedMicros();
            return;
        }
        telemetry.emissionMicros += emission.elapsedMicros();
        CoffObjectVerifier verifier;
        const PhaseClock verification;
        if (!verifier.verify(result->objectFiles_[ordinal]))
            moduleErrors[ordinal] = verifier.error();
        telemetry.verificationMicros += verification.elapsedMicros();
        if (moduleErrors[ordinal].empty() && cache != nullptr && cache->enabled())
        {
            const PhaseClock store;
            if (!cache->storeObject(cacheKey, result->objectFiles_[ordinal], moduleErrors[ordinal]))
            {
                telemetry.storeMicros += store.elapsedMicros();
                return;
            }
            telemetry.storeMicros += store.elapsedMicros();
        }
    };

    const auto initializeWorkerFloatingPoint = [workerFloatingPointState](std::size_t) {
        initializeFloatingPointStateForWorker(workerFloatingPointState);
    };
    if (usePartitions)
    {
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
            return nullptr;
        }
        catch (...)
        {
            error = "LLVM worker failed with a non-standard exception";
            return nullptr;
        }
    }
    else
    {
        processModule(0);
    }
    for (std::size_t ordinal = 0; ordinal < moduleCount; ++ordinal)
    {
        if (!moduleErrors[ordinal].empty())
        {
            error = "LLVM code unit #" + std::to_string(ordinal) +
                " failed: " + moduleErrors[ordinal];
            return nullptr;
        }
        result->cacheHitCount_ += cacheHits[ordinal];
        result->cacheMissCount_ += cacheMisses[ordinal];
    }
    // NSC305-L01. Record the observed per-unit cost into the partition's bounded historical weight.
    if (usePartitions)
    {
        for (std::size_t ordinal = 0; ordinal < partition.units().size(); ++ordinal)
        {
            if (cacheHits[ordinal + 1] != 0U) continue;
            const CodeUnitTelemetry& unitTelemetry = codeUnitTelemetry[ordinal + 1];
            const std::uint64_t observed = unitTelemetry.fingerprintMicros +
                unitTelemetry.lookupMicros + unitTelemetry.loweringMicros +
                unitTelemetry.targetMicros +
                unitTelemetry.optimizationMicros + unitTelemetry.emissionMicros +
                unitTelemetry.verificationMicros + unitTelemetry.storeMicros;
            partition.recordCodeUnitCost(partition.units()[ordinal].ordinal, observed);
        }
    }
    if (usePartitions && cache != nullptr && cache->enabled())
    {
        partitionPlanPayload = partition.persistentPlanPayload();
        if (partitionPlanPayload.empty())
        {
            error = "cache-aware code partition did not produce a persistent plan";
            return nullptr;
        }
        if (!cache->storeCodePartitionPlan(
                partitionPlanKey, partitionPlanPayload, error))
            return nullptr;
    }
    result->codeUnitTelemetry_ = std::move(codeUnitTelemetry);

    if (preserveModuleText)
    {
        for (const std::string& moduleText : moduleTexts)
        {
            result->combinedLlvmText_ += moduleText;
            result->combinedLlvmText_ += '\n';
        }
    }
    if (linkTimeOptimization == LinkTimeOptimizationMode::Thin)
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
        const auto indexBytes = std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(index.data()), index.size());
        result->thinLtoSummaryCount_ = moduleTexts.size();
        result->thinLtoIndexDigest_ = basic::sha256Hex(basic::sha256(indexBytes));
    }
    return result;
}
}
