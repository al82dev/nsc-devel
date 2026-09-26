#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"

#include "nativesharp/LLVM/FloatingPointMode.h"

#include "nativesharp/Basic/Sha256.h"

#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationObservation.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopCodePartition.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
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
    if (!partition.build(
            mirModel, workerCount, error,
            (cache != nullptr && cache->enabled()) ||
                    linkTimeOptimization == LinkTimeOptimizationMode::Thin
                ? ProjectOopCodePartitionPolicy::StableIncremental
                : ProjectOopCodePartitionPolicy::Balanced))
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
    // Textual IR used to be serialized for every code unit even though the Windows import
    // planner no longer consumes it.  Keep it only for the two features whose keys/summaries
    // are intentionally based on IR.  Ordinary EXE/DLL/LIB production already fingerprints
    // the emitted objects when incremental linking is enabled.
    const bool preserveModuleText =
        (cache != nullptr && cache->enabled()) ||
        linkTimeOptimization == LinkTimeOptimizationMode::Thin;

    // Process one code unit end to end. The LLVM module and target machine are destroyed as soon
    // as its object is emitted instead of retaining every partition through three global waves.
    const auto processModule = [&](std::size_t ordinal) {
        ProjectOopLlvmLowerer lowerer;
        std::unique_ptr<LlvmModule> module;
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
        if (module == nullptr || !lowerer.error().empty())
        {
            moduleErrors[ordinal] = "lowering failed";
            if (!lowerer.error().empty()) moduleErrors[ordinal] += ": " + lowerer.error();
            return;
        }

        std::unique_ptr<LlvmTargetMachine> targetMachine =
            LlvmTargetMachine::createWindowsX64(optimizationLevel, moduleErrors[ordinal]);
        if (targetMachine == nullptr ||
            !targetMachine->configureModuleForTarget(*module, moduleErrors[ordinal]))
        {
            if (moduleErrors[ordinal].empty())
                moduleErrors[ordinal] = "target configuration failed";
            else
                moduleErrors[ordinal] = "target configuration failed: " + moduleErrors[ordinal];
            return;
        }

        LlvmOptimizer optimizer;
        LlvmOptimizationObservation observation;
        if (!optimizer.optimizePrepared(
                *module, *targetMachine, optimizationLevel,
                &observation, moduleErrors[ordinal]))
        {
            if (moduleErrors[ordinal].empty())
                moduleErrors[ordinal] = "optimization failed";
            else
                moduleErrors[ordinal] = "optimization failed: " + moduleErrors[ordinal];
            return;
        }

        if (preserveModuleText) moduleTexts[ordinal] = module->text();
        std::string cacheKey;
        if (cache != nullptr && cache->enabled())
        {
            cacheKey = cache->objectKey(
                moduleTexts[ordinal], targetMachine->triple(),
                targetMachine->dataLayout(), optimizationLevel,
                debugInformation);
            bool hit = false;
            if (!cache->restoreObject(
                    cacheKey, result->objectFiles_[ordinal], hit,
                    moduleErrors[ordinal]))
                return;
            if (hit)
            {
                CoffObjectVerifier verifier;
                if (verifier.verify(result->objectFiles_[ordinal]))
                {
                    cacheHits[ordinal] = 1;
                    return;
                }
            }
            cacheMisses[ordinal] = 1;
        }
        if (!targetMachine->emitPrepared(
                *module, result->objectFiles_[ordinal],
                LlvmCodeGenFileType::Object, moduleErrors[ordinal]))
        {
            return;
        }
        CoffObjectVerifier verifier;
        if (!verifier.verify(result->objectFiles_[ordinal]))
            moduleErrors[ordinal] = verifier.error();
        if (moduleErrors[ordinal].empty() && cache != nullptr && cache->enabled() &&
            !cache->storeObject(cacheKey, result->objectFiles_[ordinal], moduleErrors[ordinal]))
            return;
    };

    const FloatingPointState workerFloatingPointState = captureFloatingPointState();
    const auto initializeWorkerFloatingPoint = [workerFloatingPointState](std::size_t) {
        initializeFloatingPointStateForWorker(workerFloatingPointState);
    };
    if (usePartitions)
    {
        driver::CompilationWorkerPool workers(workerCount, initializeWorkerFloatingPoint);
        for (std::size_t ordinal = 0; ordinal < moduleCount; ++ordinal)
            workers.submit([&, ordinal](std::size_t) { processModule(ordinal); });
        workers.wait();
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
