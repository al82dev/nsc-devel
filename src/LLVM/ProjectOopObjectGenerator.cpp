#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"

#include "nativesharp/Driver/CompilationWorkerPool.h"
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
    error.clear();
    workerCount = std::max<std::size_t>(1, workerCount);

    ProjectOopCodePartition partition;
    if (!partition.build(mirModel, workerCount, error)) return nullptr;
    const bool usePartitions = workerCount > 1 && partition.units().size() > 1;
    const std::size_t moduleCount = usePartitions ? partition.units().size() + 1 : 1;
    std::vector<std::unique_ptr<LlvmModule>> modules(moduleCount);
    std::vector<std::string> moduleErrors(moduleCount);

    const auto lowerModule = [&](std::size_t moduleOrdinal) {
        ProjectOopLlvmLowerer lowerer;
        if (!usePartitions)
        {
            modules[moduleOrdinal] = lowerer.lower(
                nameLookup, memberModel, mirModel, sourceManager, debugInformation,
                optimizationLevel, debugPrefixMaps);
        }
        else if (moduleOrdinal == 0)
        {
            modules[moduleOrdinal] = lowerer.lowerPartition(
                nameLookup, memberModel, mirModel, sourceManager, debugInformation,
                optimizationLevel, {}, true, "NativeSharp.OOP.globals", debugPrefixMaps);
        }
        else
        {
            const ProjectOopCodeUnit& unit = partition.units()[moduleOrdinal - 1];
            modules[moduleOrdinal] = lowerer.lowerPartition(
                nameLookup, memberModel, mirModel, sourceManager, debugInformation,
                optimizationLevel, unit.functionOrdinals, false,
                "NativeSharp.OOP." + unit.stableKey, debugPrefixMaps);
        }
        moduleErrors[moduleOrdinal] = lowerer.error();
    };

    if (usePartitions)
    {
        driver::CompilationWorkerPool workers(workerCount);
        for (std::size_t ordinal = 0; ordinal < moduleCount; ++ordinal)
            workers.submit([&, ordinal](std::size_t) { lowerModule(ordinal); });
        workers.wait();
    }
    else
    {
        lowerModule(0);
    }
    for (std::size_t ordinal = 0; ordinal < moduleCount; ++ordinal)
    {
        if (modules[ordinal] == nullptr || !moduleErrors[ordinal].empty())
        {
            error = "LLVM code unit #" + std::to_string(ordinal) + " failed";
            if (!moduleErrors[ordinal].empty()) error += ": " + moduleErrors[ordinal];
            return nullptr;
        }
    }

    // Item 258 parallelizes lowering and emission. Optimization intentionally remains an ordered
    // coordinator pass; item 259 moves this exact loop to the worker pool without changing units.
    std::vector<std::unique_ptr<LlvmTargetMachine>> targetMachines(moduleCount);
    LlvmOptimizer optimizer;
    for (std::size_t ordinal = 0; ordinal < moduleCount; ++ordinal)
    {
        std::string moduleError;
        targetMachines[ordinal] = LlvmTargetMachine::createWindowsX64(
            optimizationLevel, moduleError);
        if (targetMachines[ordinal] == nullptr ||
            !targetMachines[ordinal]->configureModuleForTarget(*modules[ordinal], moduleError))
        {
            error = "LLVM code unit #" + std::to_string(ordinal) +
                " target configuration failed: " + moduleError;
            return nullptr;
        }
        LlvmOptimizationObservation observation;
        if (!optimizer.optimize(*modules[ordinal], *targetMachines[ordinal], optimizationLevel,
                &observation, moduleError))
        {
            error = "LLVM code unit #" + std::to_string(ordinal) +
                " optimization failed: " + moduleError;
            return nullptr;
        }
    }

    const std::optional<std::filesystem::path> temporaryDirectory =
        createTemporaryDirectory(error);
    if (!temporaryDirectory.has_value()) return nullptr;
    auto result = std::unique_ptr<ProjectOopObjectBundle>(
        new ProjectOopObjectBundle(*temporaryDirectory));
    result->codeUnitCount_ = usePartitions ? partition.units().size() : 1;
    result->objectFiles_.reserve(moduleCount);
    for (std::size_t ordinal = 0; ordinal < moduleCount; ++ordinal)
    {
        result->combinedLlvmText_ += modules[ordinal]->text();
        result->combinedLlvmText_ += '\n';
        result->objectFiles_.push_back(
            *temporaryDirectory / ("program-" + std::to_string(ordinal) + ".obj"));
    }

    std::vector<std::string> emissionErrors(moduleCount);
    const auto emitModule = [&](std::size_t ordinal) {
        if (!targetMachines[ordinal]->emit(
                *modules[ordinal], result->objectFiles_[ordinal],
                LlvmCodeGenFileType::Object, emissionErrors[ordinal]))
        {
            return;
        }
        CoffObjectVerifier verifier;
        if (!verifier.verify(result->objectFiles_[ordinal]))
            emissionErrors[ordinal] = verifier.error();
    };
    if (usePartitions)
    {
        driver::CompilationWorkerPool workers(workerCount);
        for (std::size_t ordinal = 0; ordinal < moduleCount; ++ordinal)
            workers.submit([&, ordinal](std::size_t) { emitModule(ordinal); });
        workers.wait();
    }
    else
    {
        emitModule(0);
    }
    for (std::size_t ordinal = 0; ordinal < moduleCount; ++ordinal)
    {
        if (!emissionErrors[ordinal].empty())
        {
            error = "LLVM code unit #" + std::to_string(ordinal) +
                " object emission failed: " + emissionErrors[ordinal];
            return nullptr;
        }
    }
    return result;
}
}
