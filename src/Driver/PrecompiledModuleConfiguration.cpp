#include "Internal/DriverInternal.h"

#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"

namespace nativesharp::driver
{
std::optional<compilation::CompilationInputManifest> loadCompilationInputManifest(
    const std::filesystem::path& projectFile,
    std::string& error)
{
    const CompilationJobPolicy* const jobs = activeCompilationJobPolicy();
    compilation::ProjectLoader& projectLoader = activeProjectLoader();
    const compilation::ProjectLoadOptions loadOptions = activeProjectLoadOptions();
    compilation::ProjectGraphLoader loader(
        jobs != nullptr ? jobs->effectiveJobs() : 1,
        [&projectLoader, loadOptions](
            const std::filesystem::path& path,
            std::string& loadError)
        {
            return projectLoader.load(path, loadOptions, loadError);
        });
    std::optional<compilation::ProjectGraph> graph = measureDriverPhase(
        "project-graph",
        [&]
        {
            return loader.load(projectFile, error);
        });
    if (!graph.has_value())
    {
        return std::nullopt;
    }
    return compilation::CompilationInputManifest(
        std::move(*graph), std::move(loadOptions));
}

bool configurePrecompiledModules(
    compilation::CompilationInputManifest& manifest,
    frontend::ProjectCompilation& compilation,
    std::string& error)
{
    return compilation::preparePrecompiledProject(
        manifest.releaseProjectGraph(), manifest.loadOptions(), compilation, error);
}

void configureObjectGeneratorProgress(
    llvm_backend::ProjectOopObjectGenerator& generator)
{
    if (activeExecutionScope == nullptr || !activeExecutionScope->reportProgress)
    {
        return;
    }
    DriverExecutionScope* const scope = activeExecutionScope;
    generator.setProgressSink(
        [scope](const basic::CompilationEvent& event)
        {
            scope->writeProgress(event);
        });
}
} // namespace nativesharp::driver
