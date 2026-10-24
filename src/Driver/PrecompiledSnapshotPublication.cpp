#include "Internal/DriverInternal.h"

#include "nativesharp/Compilation/PrecompiledProductSidecarCodec.h"
#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::driver
{
namespace
{
// EN: Module name of the empty consumer; it cannot clash with a NativeSharp module name.
// ES: Nombre de módulo del consumidor vacío; no puede chocar con un nombre de módulo NativeSharp.
constexpr std::string_view kReferenceModule = "__nsc_snapshot_reference";

// EN: The library just written first, then every library reference of the producer's graph, so
//     the binary closure of the new library resolves to the same artifacts the producer used.
// ES: Primero la biblioteca recién escrita y luego cada referencia de biblioteca del grafo del
//     productor, para que la clausura binaria de la nueva biblioteca resuelva a los mismos
//     artefactos que usó el productor.
std::vector<compilation::LibraryReference> referenceLibraries(
    const compilation::ProjectGraph &graph,
    const std::filesystem::path &metadataPath)
{
    std::filesystem::path libraryPath = metadataPath;
    libraryPath.replace_extension(".lib");
    std::vector<compilation::LibraryReference> references = {
        {libraryPath, metadataPath, {}, compilation::PrecompiledProductSidecarCodec::pathForMetadata(metadataPath)}};
    std::set<std::string> seen = {foldedPathKey(metadataPath)};
    for (const compilation::Project &project : graph.projects())
    {
        for (const compilation::LibraryReference &reference : project.libraryReferences())
        {
            if (seen.insert(foldedPathKey(reference.metadataPath)).second)
                references.push_back(reference);
        }
    }
    return references;
}
}

bool buildPrecompiledSnapshotReference(const frontend::ProjectCompilation &producer,
                                       const std::filesystem::path &metadataPath,
                                       frontend::ProjectCompilation &reference, std::string &error)
{
    const compilation::ProjectGraph *const graph = producer.projectGraph();
    if (graph == nullptr || graph->rootProject() == nullptr)
    {
        error = "producer has no project graph";
        return false;
    }
    // EN: A dependency that is not available as a library (a source project of the producer) makes
    //     the closure fail below, and the library is then published without a snapshot.
    // ES: Una dependencia que no está disponible como biblioteca (un proyecto fuente del productor)
    //     hace fallar la clausura de abajo, y entonces la biblioteca se publica sin snapshot.
    // EN: The consumer lives beside the metadata only by name; nothing is written for it.
    // ES: El consumidor vive junto a la metadata solo de nombre; no se escribe nada para él.
    const std::filesystem::path directory = metadataPath.parent_path();
    const std::filesystem::path projectFile = directory / (std::string(kReferenceModule) + ".nsproj");
    compilation::ProjectSourceSet sources(directory);
    if (!sources.finalize(error, true))
        return false;
    std::vector<compilation::Project> projects;
    projects.emplace_back(projectFile, std::string(kReferenceModule), compilation::ProjectOutputKind::Library,
                          std::move(sources), std::vector<std::filesystem::path>{},
                          referenceLibraries(*graph, metadataPath), std::vector<compilation::SdkManifest>{});
    reference.setProjectLoadOptions(producer.projectLoadOptions());
    reference.setCompilationJobs(producer.compilationJobs());
    if (!compilation::preparePrecompiledProject(compilation::ProjectGraph(projectFile, std::move(projects)),
                                                producer.projectLoadOptions(), reference, error))
        return false;
    return reference.compileProjectObjectLayouts(projectFile, error);
}

void publishPrecompiledSnapshot(const frontend::ProjectCompilation &compilation,
                                const std::filesystem::path &metadataPath)
{
    try
    {
        frontend::ProjectCompilation reference;
        std::string error;
        const bool built = measureDriverPhase("precompiled-snapshot-reference", [&]
                                              { return buildPrecompiledSnapshotReference(
                                                    compilation, metadataPath, reference, error); });
        if (activeExecutionScope != nullptr && activeExecutionScope->reportTiming)
        {
            activeExecutionScope->output << "precompiled-snapshot-reference: "
                                         << (built ? std::string("ok") : "failed " + error) << '\n';
        }
    }
    catch (const std::exception &exception)
    {
        if (activeExecutionScope != nullptr && activeExecutionScope->reportTiming)
            activeExecutionScope->output << "precompiled-snapshot-reference: failed " << exception.what() << '\n';
    }
}
} // namespace nativesharp::driver
