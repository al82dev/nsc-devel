#include "Internal/DriverInternal.h"

#include "../Frontend/Snapshot/MemberSnapshotCapture.h"
#include "../Frontend/Snapshot/NameSnapshotCapture.h"
#include "../Frontend/Snapshot/NamespaceMemberSnapshotCapture.h"
#include "../Frontend/Snapshot/TypeModelSnapshotCapture.h"

#include "nativesharp/Compilation/PrecompiledProductSidecarCodec.h"
#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"
#include "nativesharp/Compilation/PrecompiledSemanticSnapshotCodec.h"

#include <set>
#include <string>
#include <string_view>
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

// EN: The snapshot header of the captured API unit: every other module of the reference
//     compilation with its NSMETA digest, and the shape of the API tree indexed before any phase
//     mutated it.
// ES: La cabecera del snapshot de la unidad de API capturada: cada otro módulo de la compilación de
//     referencia con el digest de su NSMETA, y la forma del árbol de API indexado antes de que lo
//     mutara cualquier fase.
bool captureSnapshotHeader(const frontend::ProjectCompilation &reference,
                           const frontend::PrecompiledSnapshotUnit &capture,
                           compilation::PrecompiledSemanticSnapshot &snapshot, std::string &error)
{
    for (const frontend::PrecompiledSyntaxModule &module : reference.precompiledModules())
    {
        if (module.moduleName == capture.moduleName)
            continue;
        if (module.metadataDigest.size() != 64U)
        {
            error = "dependency '" + module.moduleName + "' has no NSMETA digest";
            return false;
        }
        snapshot.dependencies.push_back({module.moduleName, module.metadataDigest});
    }
    snapshot.nodeCount = capture.index.nodes().size();
    snapshot.typeCount = capture.index.types().size();
    snapshot.nodeKindDigest = capture.index.kindDigest();
    return true;
}

using SectionCapture = bool (*)(const frontend::ProjectCompilation &, const frontend::PrecompiledSnapshotUnit &,
                                std::string &, std::string &);

// EN: Captures one model section; on failure the model is recorded as skipped with its reason.
// ES: Captura la sección de un modelo; si falla, el modelo queda como omitido con su razón.
void captureSection(std::string_view model, SectionCapture captureModel, const frontend::ProjectCompilation &reference,
                    const frontend::PrecompiledSnapshotUnit &capture,
                    compilation::PrecompiledSemanticSnapshot &snapshot, std::string &skipped)
{
    std::string payload;
    std::string error;
    if (captureModel(reference, capture, payload, error))
        snapshot.sections.push_back({std::string(model), std::move(payload)});
    else
        skipped += " " + std::string(model) + "=" + error;
}

// EN: Plan item 2.2: captures the section of every model; a model whose capture fails gets no
//     section, so consumers analyze that model as today. Returns the skipped models and reasons.
// ES: Punto 2.2 del plan: captura la sección de cada modelo; un modelo cuya captura falla no lleva
//     sección, así que los consumidores analizan ese modelo como hoy. Regresa los modelos omitidos
//     y sus razones.
std::string captureSnapshotSections(const frontend::ProjectCompilation &reference,
                                    const frontend::PrecompiledSnapshotUnit &capture,
                                    compilation::PrecompiledSemanticSnapshot &snapshot)
{
    std::string skipped;
    captureSection("names", frontend::captureNameSnapshot, reference, capture, snapshot, skipped);
    captureSection("name-declarations", frontend::captureNameDeclarationSnapshot, reference, capture, snapshot,
                   skipped);
    captureSection("members", frontend::captureMemberSnapshot, reference, capture, snapshot, skipped);
    captureSection("namespace-members", frontend::captureNamespaceMemberSnapshot, reference, capture, snapshot,
                   skipped);
    captureSection("inheritance", frontend::captureInheritanceSnapshot, reference, capture, snapshot, skipped);
    captureSection("interfaces", frontend::captureInterfaceSnapshot, reference, capture, snapshot, skipped);
    captureSection("dispatch", frontend::captureDispatchSnapshot, reference, capture, snapshot, skipped);
    return skipped;
}

// EN: Plan item 2.3: rewrites the `.nsproducts` of the library transactionally as v2 with the
//     snapshot; the four products stay exactly as written.
// ES: Punto 2.3 del plan: reescribe de forma transaccional el `.nsproducts` de la biblioteca como
//     v2 con el snapshot; los cuatro productos quedan exactamente como se escribieron.
bool writeSnapshotSidecar(const frontend::ProjectCompilation &reference, const std::filesystem::path &metadataPath,
                          const compilation::PrecompiledSemanticSnapshot &snapshot, std::string &error)
{
    const compilation::LibraryMetadataRepositoryEntry *const metadata =
        reference.libraryMetadataRepository().load(metadataPath, error);
    if (metadata == nullptr)
        return false;
    compilation::PrecompiledProductSidecar sidecar;
    if (!compilation::PrecompiledProductSidecarCodec::read(
            compilation::PrecompiledProductSidecarCodec::pathForMetadata(metadataPath), metadata->bytes(), sidecar,
            error))
    {
        return false;
    }
    sidecar.publicApiSnapshot = compilation::PrecompiledSemanticSnapshotCodec::encode(snapshot);
    return compilation::PrecompiledProductSidecarCodec::write(metadataPath, sidecar, metadata->bytes(), error);
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
    reference.setPrecompiledSnapshotCaptureModule(graph->rootProject()->moduleName());
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
        // EN: Plan item 2.4: any failure leaves the v1 `.nsproducts` as written; the library is
        //     published anyway and consumers take today's path.
        // ES: Punto 2.4 del plan: cualquier falla deja el `.nsproducts` v1 tal como se escribió; la
        //     biblioteca se publica igual y los consumidores van por la ruta de hoy.
        compilation::PrecompiledSemanticSnapshot snapshot;
        const frontend::PrecompiledSnapshotUnit *const capture =
            built ? reference.precompiledSnapshotCapture() : nullptr;
        if (built && capture == nullptr)
            error = "the library has no restored API unit";
        std::string skipped;
        const bool written = capture != nullptr && captureSnapshotHeader(reference, *capture, snapshot, error) &&
                             (skipped = captureSnapshotSections(reference, *capture, snapshot), true) &&
                             writeSnapshotSidecar(reference, metadataPath, snapshot, error);
        if (activeExecutionScope != nullptr && activeExecutionScope->reportTiming)
        {
            std::string sections;
            for (const compilation::PrecompiledSnapshotSection &section : snapshot.sections)
                sections += (sections.empty() ? "" : ",") + section.model;
            activeExecutionScope->output << "precompiled-snapshot-reference: "
                                         << (written ? "written sections=" + sections + skipped
                                                     : "not-written " + error)
                                         << '\n';
        }
    }
    catch (const std::exception &exception)
    {
        if (activeExecutionScope != nullptr && activeExecutionScope->reportTiming)
            activeExecutionScope->output << "precompiled-snapshot-reference: failed " << exception.what() << '\n';
    }
}
} // namespace nativesharp::driver
