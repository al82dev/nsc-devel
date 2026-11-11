#include "Internal/CompilationInputManifestFactory.h"
#include "Internal/DriverInternal.h"

#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarPath.h"
#include "nativesharp/LLVM/ThinLtoLibrarySidecarCodec.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/WindowsRuntimeImports.h"
#include "nativesharp/Linker/WindowsRuntimeLibrary.h"
#include "nativesharp/Interop/CppBinaryDependencies.h"
#include "nativesharp/Basic/Sha256.h"
#if defined(NSC_ENABLE_CPP_INTEROP)
#include "nativesharp/Frontend/CppDiagnosticAdapter.h"
#include "nativesharp/Interop/CppNamespaceInventory.h"
#include "nativesharp/Interop/CppSemanticFunctionGroups.h"
#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Interop/CppVariadicAbiReconciliation.h"
#endif

#include <algorithm>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace nativesharp::driver
{
std::optional<compilation::CompilationInputManifest> loadCompilationInputManifest(
    const std::filesystem::path& projectFile,
    std::string& error)
{
    if (activeExecutionScope != nullptr &&
        activeExecutionScope->preparedInputManifest != nullptr &&
        foldedPathKey(
            activeExecutionScope->preparedInputManifest->projectGraph().rootProjectFile()) ==
            foldedPathKey(projectFile))
    {
        compilation::CompilationInputManifest manifest =
            std::move(*activeExecutionScope->preparedInputManifest);
        activeExecutionScope->preparedInputManifest = nullptr;
        return manifest;
    }
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
        },
        {},
        activeExecutionScope != nullptr
            ? &activeExecutionScope->compilationLifetime.workerPool()
            : nullptr);
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
    return createCompilationInputManifest(
        std::move(*graph), std::move(loadOptions));
}

void addCompilationProductIdentity(
    compilation::CompilationInputManifest& manifest,
    std::string_view productKind,
    const std::filesystem::path& outputPath)
{
    manifest.addIdentityField("product", std::string(productKind));
    manifest.setBundleProductKind(std::string(productKind));
    if (!outputPath.empty())
    {
        // EN: T08-002: UTF-8, like every other path in the manifest. The narrow conversion used before
        //     threw for names the active code page cannot represent (a combining accent).
        // ES: T08-002: UTF-8, como las demás rutas del manifiesto. La conversión estrecha anterior
        //     lanzaba una excepción con nombres que la página de códigos activa no representa.
        manifest.addIdentityField("output-name", pathToUtf8(outputPath.filename()));
        manifest.setBundleOutputPath(outputPath);
        if (productKind == "static-library" || productKind == "dynamic-library")
        {
            std::filesystem::path header = outputPath;
            header.replace_extension(".h");
            manifest.addRequiredBundleOutput(std::move(header));
            std::filesystem::path metadata = outputPath;
            metadata.replace_extension(".nsmeta");
            manifest.addRequiredBundleOutput(metadata);
            manifest.addRequiredBundleOutput(
                compilation::precompiledProductSidecarPath(metadata));
        }
        if (productKind == "library-metadata")
        {
            manifest.addRequiredBundleOutput(
                compilation::precompiledProductSidecarPath(outputPath));
        }
        if (productKind == "dynamic-library")
        {
            std::filesystem::path importLibrary = outputPath;
            importLibrary.replace_extension(".lib");
            if (importLibrary == outputPath) importLibrary += ".import.lib";
            manifest.addRequiredBundleOutput(std::move(importLibrary));
        }
    }
}

void addCompilationCodegenIdentity(
    compilation::CompilationInputManifest& manifest,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    llvm_backend::DebugInformationKind debugInformation,
    std::span<const llvm_backend::DebugPrefixMap> debugPrefixMaps,
    llvm_backend::LinkTimeOptimizationMode linkTimeOptimization)
{
    manifest.addIdentityField(
        "optimization",
        std::string(llvm_backend::llvmOptimizationLevelName(optimizationLevel)));
    manifest.addIdentityField(
        "debug-information",
        std::string(llvm_backend::debugInformationKindName(debugInformation)));
    manifest.addIdentityField(
        "link-time-optimization",
        std::string(llvm_backend::linkTimeOptimizationModeName(
            linkTimeOptimization)));
    for (std::size_t index = 0; index < debugPrefixMaps.size(); ++index)
    {
        const std::string prefix = "debug-prefix-" + std::to_string(index);
        manifest.addIdentityField(
            prefix + "-source",
            pathToUtf8(debugPrefixMaps[index].sourcePrefix));
        manifest.addIdentityField(
            prefix + "-replacement",
            pathToUtf8(debugPrefixMaps[index].replacementPrefix));
    }
    // A static library with real ThinLTO publishes its sidecar, so restoring the bundle requires it
    // (optimization part 2, item 48). Una biblioteca estatica con ThinLTO real publica su sidecar, asi
    // que restaurar el bundle lo exige (parte 2, punto 48).
    if (manifest.bundleProductKind() == "static-library" &&
        llvm_backend::ProjectOopObjectGenerator::thinLtoRuns(linkTimeOptimization, optimizationLevel) &&
        !linker::configuredWindowsRuntimeLibrary().empty() &&
        !manifest.requiredBundleOutputNames().empty())
    {
        manifest.addRequiredBundleOutput(llvm_backend::ThinLtoLibrarySidecarCodec::pathFor(
            manifest.requiredBundleOutputNames().front()));
    }
    if (debugInformation != llvm_backend::DebugInformationKind::None)
    {
        manifest.addIdentityField(
            "debug-source-root", pathToUtf8(manifest.rootDirectory()));
        if ((manifest.bundleProductKind() == "executable" ||
             manifest.bundleProductKind() == "dynamic-library") &&
            !manifest.requiredBundleOutputNames().empty())
        {
            std::filesystem::path pdb =
                manifest.requiredBundleOutputNames().front();
            pdb.replace_extension(".pdb");
            manifest.addRequiredBundleOutput(std::move(pdb));
        }
    }
}

namespace
{
/// EN: Adds one file read by the link step to the bundle identity: `role` names it as an
///     identity field and the file itself becomes a manifest input, so the identity covers its
///     size and SHA-256 (served by the input digest cache on repeated builds).
///     Parameters: `manifest`, the identity being built; `role`, a stable field suffix such as
///     "linker"; `path`, the file the link step would use, empty when it was not found.
///     Effects: a missing file is recorded as "unavailable" instead of failing here; the link
///     step itself still reports the real error when the bundle misses.
///     Why: a restored bundle replaces the link step, so its key must change whenever the
///     linker or a toolchain file it reads changes (2026-10-24: a new NSLink was never run
///     for unchanged sources).
/// ES: Agrega a la identidad del bundle un archivo que lee el paso de enlace: `role` lo nombra
///     como campo de identidad y el archivo se vuelve entrada del manifiesto, de modo que la
///     identidad cubre su tamano y su SHA-256 (con el cache de digests en builds repetidos).
///     Parametros: `manifest`, la identidad en construccion; `role`, un sufijo estable como
///     "linker"; `path`, el archivo que usaria el enlace, vacio si no se encontro.
///     Efectos: un archivo ausente se registra como "unavailable" en vez de fallar aqui; el
///     paso de enlace sigue reportando el error real cuando el bundle falla.
///     Motivo: un bundle restaurado sustituye al enlace, asi que su llave debe cambiar cuando
///     cambia el linker o un archivo del toolchain que lee (2026-10-24: un NSLink nuevo no se
///     ejecutaba con fuentes sin cambios).
void addLinkToolInput(
    compilation::CompilationInputManifest& manifest,
    const std::string& role,
    const std::filesystem::path& path)
{
    std::error_code filesystemError;
    if (path.empty() || !std::filesystem::is_regular_file(path, filesystemError) || filesystemError)
    {
        manifest.addIdentityField("link-tool-" + role, "unavailable");
        return;
    }
    // EN: UTF-8 for the same reason as "output-name" (T08-002). ES: UTF-8 por la misma razon que "output-name".
    manifest.addIdentityField("link-tool-" + role, pathToUtf8(path.filename()));
    manifest.addInputFile(path);
}
}

/// EN: Adds the link tools of the manifest's product to its bundle identity, resolved exactly
///     as the link step resolves them: NSLink (NSC_LINKER, beside nsc.exe, PATH), the
///     librarian for static libraries and DLLs, the configured Windows runtime library, and
///     the canonical Windows import libraries for executables and DLLs.
///     Precondition: addCompilationProductIdentity already set the bundle product kind.
/// ES: Agrega a la identidad del bundle las herramientas de enlace del producto, resueltas igual
///     que las resuelve el enlace: NSLink (NSC_LINKER, junto a nsc.exe, PATH), el
///     bibliotecario para bibliotecas estaticas y DLLs, la biblioteca de runtime de Windows
///     configurada y las import libraries canonicas de Windows para ejecutables y DLLs.
///     Precondicion: addCompilationProductIdentity ya fijo el tipo de producto del bundle.
void addCompilationLinkToolIdentity(compilation::CompilationInputManifest& manifest, std::string_view linkerSelection)
{
    const std::string product(manifest.bundleProductKind());
    std::string linkerError;
    const std::unique_ptr<linker::LldLinker> linkTool = linker::LldLinker::discoverSelected(linkerSelection, linkerError);
    addLinkToolInput(
        manifest, "linker",
        linkTool != nullptr ? linkTool->executablePath() : std::filesystem::path());
    if (product == "static-library" || product == "dynamic-library")
    {
        addLinkToolInput(
            manifest, "librarian",
            linkTool != nullptr ? linkTool->librarianExecutablePath() : std::filesystem::path());
    }
    addLinkToolInput(manifest, "runtime-library", linker::configuredWindowsRuntimeLibrary());
    if (product == "static-library") return;
    std::vector<std::filesystem::path> importLibraries;
    std::string importError;
    if (!linker::findCanonicalWindowsImportLibraries(importLibraries, importError))
    {
        manifest.addIdentityField("link-tool-import-libraries", "unavailable");
        return;
    }
    for (std::size_t index = 0; index < importLibraries.size(); ++index)
    {
        addLinkToolInput(
            manifest, "import-library-" + std::to_string(index), importLibraries[index]);
    }
}

void addCompilationLinkIdentity(
    compilation::CompilationInputManifest& manifest,
    bool incrementalLink,
    linker::WindowsExecutableSubsystem subsystem,
    std::span<const std::filesystem::path> extraLinkLibraries,
    std::string_view linkerSelection)
{
    manifest.addIdentityField(
        "incremental-link", incrementalLink ? "true" : "false");
    manifest.addIdentityField(
        "subsystem", std::string(subsystemDisplayName(subsystem)));
    if (incrementalLink && !manifest.requiredBundleOutputNames().empty())
    {
        manifest.addRequiredBundleOutput(
            manifest.requiredBundleOutputNames().front() + ".nslinc");
    }
    for (std::size_t index = 0; index < extraLinkLibraries.size(); ++index)
    {
        manifest.addInputFile(extraLinkLibraries[index]);
        manifest.addIdentityField(
            "extra-link-library-" + std::to_string(index),
            pathToUtf8(extraLinkLibraries[index].filename()));
    }
    // EN: Freeze 3.0 DEC-13: a bundle built without the host-DLL check must not satisfy a build that
    //     asks for it. The field exists only when requested, so default identities do not change.
    // ES: Congelamiento 3.0 DEC-13: un bundle construido sin la comprobacion contra DLL del host no
    //     debe satisfacer una compilacion que la pide. El campo solo existe cuando se pide, asi que
    //     las identidades por defecto no cambian.
    if (activeExecutionScope != nullptr && activeExecutionScope->validateSystemImports)
        manifest.addIdentityField("validate-system-imports", "true");
    addCompilationLinkToolIdentity(manifest, linkerSelection);
}

namespace
{
/// EN: Calculates the manifest identity from the closed project graph and looks up a verified
///     compilation bundle for it.
///     Parameters: `manifest`, whose graph inputs, identity and bundle lookup are filled;
///     `compilation`, whose metadata repository supplies reference digests; `closedGraph`, the
///     graph after the precompiled dependency closure; `deferrable`, true when the caller
///     restores a verified bundle instead of compiling; `skipMaterialization` becomes true
///     exactly when `deferrable` and the bundle is verified; `error`, the failure text.
///     Returns false on an identity or cache I/O failure. Effects: may quarantine a corrupt bundle
///     entry (inside CompilationBundleCache) and writes the `compilation-bundle-cache:` report
///     line under --time.
///     Why: this used to run after the precompiled modules were materialized; running it first
///     lets a verified hit skip that work (audit 2026-10-22).
/// ES: Calcula la identidad del manifiesto desde el grafo cerrado y busca un bundle de compilacion
///     verificado para ella.
///     Parametros: `manifest`, al que se agregan entradas del grafo, identidad y busqueda;
///     `compilation`, cuyo repositorio de metadata da los digests de referencias; `closedGraph`, el grafo despues de la
///     clausura precompilada; `deferrable`, true cuando el llamador restaura un bundle verificado
///     en vez de compilar; `skipMaterialization` queda en true exactamente cuando `deferrable` y
///     el bundle esta verificado; `error`, el texto de falla.
///     Retorno: false ante una falla de identidad o de E/S del cache. Efectos: puede poner en
///     cuarentena una entrada corrupta (dentro de CompilationBundleCache) y escribe la linea
///     `compilation-bundle-cache:` con --time.
///     Motivo: antes esto corria despues de materializar los modulos precompilados; correrlo antes
///     permite que un acierto verificado omita ese trabajo (auditoria 2026-10-22).
// Capture sibling runtime dependencies before lookup: packaging discovers the same
// closure later, so a bundle must change when a sibling appears, disappears or changes.
bool addCppRuntimeClosureInputs(compilation::CompilationInputManifest& manifest,
                                std::vector<std::filesystem::path> pending)
{
    std::unordered_set<std::string> seen;
    for (std::size_t index = 0; index < pending.size(); ++index)
    {
        const auto runtime = pending[index].lexically_normal();
        if (!seen.insert(foldedPathKey(runtime)).second) continue;
        linker::PeExecutableVerifier verifier;
        if (!verifier.verifyDynamicLibrary(runtime)) return false;
        manifest.addInputFile(runtime);
        for (const auto& imported : verifier.info().importedLibraries)
        {
            const auto leaf = pathFromUtf8(imported);
            if (leaf.empty() || leaf != leaf.filename() || leaf.has_root_name()) return false;
            const auto sibling = (runtime.parent_path() / leaf).lexically_normal();
            std::error_code ec;
            const bool present = std::filesystem::is_regular_file(sibling, ec);
            if (ec && ec != std::errc::no_such_file_or_directory &&
                ec != std::errc::not_a_directory) return false;
            manifest.addIdentityField("cpp-runtime-sibling:" + foldedPathKey(sibling),
                                      present ? "present" : "absent");
            if (present) pending.push_back(sibling);
        }
    }
    return true;
}

bool lookupCompilationBundle(
    compilation::CompilationInputManifest& manifest,
    frontend::ProjectCompilation& compilation,
    const compilation::ProjectGraph& closedGraph,
    bool deferrable,
    bool cppInputsComplete,
    const std::vector<std::pair<std::string, std::size_t>>& cppSessionOwners,
    bool& skipMaterialization,
    std::string& error)
{
    manifest.addProjectGraphInputs(closedGraph);
    compilation::CompilationBundleCache bundleCache;
    const llvm_backend::CompilationArtifactCache* const artifactCache =
        llvm_backend::activeCompilationArtifactCache();
    // Only the bundle cache uses the identity. Without it the inputs are not hashed: the lookup
    // below reports the cache as disabled and the sources are read again by the syntax phase.
    for (const auto& project : closedGraph.projects())
    {
        if (project.cppLibraryReferences().empty()) continue;
        const auto owner = std::make_pair(foldedPathKey(project.projectFile()),
                                          project.cppLibraryReferences().size());
        if (std::find(cppSessionOwners.begin(), cppSessionOwners.end(), owner) == cppSessionOwners.end())
            cppInputsComplete = false;
    }
    if (artifactCache != nullptr && artifactCache->enabled() && cppInputsComplete)
    {
        interop::LibraryMetadataCodec codec;
        std::vector<std::filesystem::path> cppRuntimeRoots;
        for (const auto& project : closedGraph.projects())
        {
            for (const auto& reference : project.cppLibraryReferences())
                if (!reference.runtimePath.empty()) cppRuntimeRoots.push_back(reference.runtimePath);
            for (const auto& reference : project.libraryReferences())
            {
                const auto metadata = compilation.libraryMetadataRepository().decode(
                    reference.metadataPath,
                    [&codec](std::span<const std::uint8_t> bytes, interop::LibraryMetadata& model,
                             std::string& decodeError) { return codec.read(bytes, model, decodeError); },
                    error);
                if (!metadata) return false;
                if (!metadata->cppBinaryDependencies.empty())
                {
                    std::vector<interop::ResolvedCppBinaryDependency> dependencies;
                    if (!interop::resolveCppBinaryMetadataDependencies(metadata->cppBinaryDependencies,
                            reference.metadataPath.parent_path(), dependencies, error))
                        return false;
                    for (const auto& dependency : dependencies)
                    {
                        manifest.addInputFile(dependency.libraryPath);
                        if (!dependency.runtimePath.empty())
                        {
                            manifest.addInputFile(dependency.runtimePath);
                            cppRuntimeRoots.push_back(dependency.runtimePath);
                        }
                    }
                }
            }
        }
        if (addCppRuntimeClosureInputs(manifest, std::move(cppRuntimeRoots)))
        {
            bundleCache.setRoot(artifactCache->root() / "compilation-bundles");
            manifest.setInputDigestCacheRoot(artifactCache->root() / "input-digests");
            if (!manifest.calculateIdentity(compilation.libraryMetadataRepository(), error)) return false;
        }
    }
    compilation::CompilationBundleLookup lookup;
    if (!bundleCache.lookup(manifest.identity(), lookup, error))
    {
        return false;
    }
    bool hit = false;
    if (!bundleCache.verify(
            manifest.identity(),
            manifest.bundleOutputDirectory(),
            manifest.requiredBundleOutputNames(),
            lookup,
            hit,
            error))
    {
        return false;
    }
    if (activeExecutionScope != nullptr && activeExecutionScope->reportTiming)
    {
        activeExecutionScope->output
            << "compilation-bundle-cache: "
            << compilation::compilationBundleMissReasonName(lookup.reason);
        if (!lookup.detail.empty())
        {
            activeExecutionScope->output << " detail=" << lookup.detail;
        }
        activeExecutionScope->output << '\n';
    }
    skipMaterialization = deferrable && hit;
    manifest.setBundleLookup(std::move(lookup));
    return true;
}
}

/// EN: Prepares the precompiled project of `manifest` in `compilation` and looks up its bundle.
///     A verified bundle hit for a caller that restores outputs (one with required bundle
///     outputs) skips materializing the precompiled modules, which only a compilation would use.
///     Every caller with required outputs restores on a verified hit and never compiles; a
///     caller without required outputs keeps the previous full preparation. Returns false with
///     `error` on failure.
/// ES: Prepara el proyecto precompilado de `manifest` en `compilation` y busca su bundle. Un
///     acierto verificado para un llamador que restaura salidas (con salidas requeridas) omite
///     materializar los modulos precompilados, que solo usaria una compilacion. Todo llamador con
///     salidas requeridas restaura ante un acierto verificado y no compila; un llamador sin
///     salidas requeridas conserva la preparacion completa anterior. Devuelve false con `error`.
bool configurePrecompiledModules(
    compilation::CompilationInputManifest& manifest,
    frontend::ProjectCompilation& compilation,
    std::string& error,
    bool materializeDemandedSurface)
{
    bool hasCppReferences = false;
    for (const auto& project : manifest.projectGraph().projects())
        hasCppReferences = hasCppReferences || !project.cppLibraryReferences().empty();
#if defined(NSC_ENABLE_CPP_INTEROP)
    if (hasCppReferences && (activeExecutionScope == nullptr ||
        !activeExecutionScope->cppSessionEnvironment.has_value()))
    {
        error = "C/C++ project references require --cpp-clang-root";
        return false;
    }
#else
    if (hasCppReferences)
    {
        error = "C/C++ project references require a compiler built with C/C++ interoperability";
        return false;
    }
#endif
    const auto* artifactCache = llvm_backend::activeCompilationArtifactCache();
    const bool captureCppInputs = artifactCache != nullptr && artifactCache->enabled();
    bool cppInputsComplete = true;
    std::vector<std::pair<std::string, std::size_t>> cppSessionOwners;
#if defined(NSC_ENABLE_CPP_INTEROP)
    if (activeExecutionScope != nullptr && activeExecutionScope->cppSessionEnvironment.has_value())
    {
        if (activeExecutionScope->cppSessionFactory == nullptr)
        {
            error = "C/C++ session environment requires a session factory";
            return false;
        }
        interop::CppCancellationToken cancellation;
        if (activeExecutionScope->expired()) cancellation.cancel();
        auto environment = *activeExecutionScope->cppSessionEnvironment;
        environment.capturePreprocessingFingerprint = captureCppInputs;
        auto opened = interop::openCppGraphSessions(
            manifest.projectGraph(), environment,
            *activeExecutionScope->cppSessionFactory, cancellation);
        if (auto* failure = std::get_if<interop::CppGraphSessionFailure>(&opened))
        {
            error = "C/C++ session failed for project '" + pathToUtf8(failure->projectFile) +
                    "', reference " + std::to_string(failure->projectFailure.referenceIndex) +
                    ": " + failure->projectFailure.failure.message;
            frontend::appendCppDiagnostics(
                failure->projectFailure.failure.diagnostics, compilation.diagnostics());
            return false;
        }
        const bool hasSessions = !std::get<interop::CppGraphSessions>(opened).projects.empty();
        if (captureCppInputs)
        {
            for (const auto& group : std::get<interop::CppGraphSessions>(opened).projects)
            {
                const auto owner = foldedPathKey(group.projectFile);
                cppSessionOwners.emplace_back(owner, group.sessions.size());
                for (std::size_t index = 0; index < group.sessions.size(); ++index)
                {
                    const auto& session = group.sessions[index];
                    if (!session) { cppInputsComplete = false; continue; }
                    const auto options = session->optionsSnapshot();
                    const auto fingerprint = session->preprocessingFingerprint();
                    const auto sources = session->sourceFileContentsSnapshot();
                    if (!fingerprint || !sources || sources->empty() || options.language != interop::CppLanguageMode::C)
                    { cppInputsComplete = false; continue; }
                    const std::string key = "clang-session:" + owner + ":" + std::to_string(index);
                    manifest.addIdentityField(key + ":options", interop::cppSessionConfigurationIdentity(options));
                    manifest.addIdentityField(key + ":preprocessing", basic::sha256Hex(*fingerprint));
                    for (const auto& source : *sources)
                    {
                        const auto encoded = source.path.generic_u8string();
                        const std::string path(reinterpret_cast<const char*>(encoded.data()), encoded.size());
                        manifest.addIdentityField(key + ":source:" + path,
                            std::to_string(source.size) + ":" + basic::sha256Hex(source.digest));
                    }
                    manifest.addInputFile(options.driverReferencePath);
                }
            }
        }
        bool hasCppSessions = false;
        for (const auto& group : std::get<interop::CppGraphSessions>(opened).projects)
            for (const auto& session : group.sessions)
                if (session && session->optionsSnapshot().language == interop::CppLanguageMode::Cpp)
                    hasCppSessions = true;
        if (!compilation.setCppProjectSessions(
                std::move(std::get<interop::CppGraphSessions>(opened).projects), error))
            return false;
        if (hasSessions && !compilation.setCppVariadicCallPlanProvider(
                [&compilation](const frontend::NamespaceFunctionSymbol& function,
                               std::span<const std::string> optionalTypes) {
                    const auto* graph = compilation.projectGraph();
                    const auto* lookup = compilation.nameLookup();
                    return graph && lookup ? interop::planCppVariadicCall(
                        *graph, compilation.cppProjectSessions(), *lookup, function, optionalTypes) : nullptr;
                }, error)) return false;
        if (hasSessions && !compilation.setCppMetadataCatalogProvider(
                interop::provideCppCRecordMetadataCatalog, error)) return false;
        if (hasSessions && !compilation.setCppNamespaceProvider(
                [](const auto& graph, auto projects, auto& output, auto& providerError) {
                    auto inventory = interop::collectCppNamespaces(graph, projects);
                    if (const auto* failure = std::get_if<interop::CppNamespaceInventoryFailure>(&inventory))
                    {
                        providerError = "C/C++ namespace collection failed for project '" +
                            pathToUtf8(failure->projectFile) + "', reference " +
                            std::to_string(failure->referenceIndex) + ": " + failure->message;
                        return false;
                    }
                    auto names = interop::visibleCppNamespaces(
                        std::get<std::vector<interop::CppNamespaceInventoryEntry>>(inventory));
                    if (const auto* failure = std::get_if<interop::CppNamespaceInventoryFailure>(&names))
                    {
                        providerError = "C/C++ namespace conversion failed for project '" +
                            pathToUtf8(failure->projectFile) + "', reference " +
                            std::to_string(failure->referenceIndex) + ": " + failure->message;
                        return false;
                    }
                    auto& visible = std::get<std::vector<std::pair<std::size_t, std::string>>>(names);
                    output.insert(output.end(), visible.begin(), visible.end());
                    return true;
                }, error))
            return false;
        if (hasSessions && !compilation.setCppTypeAliasProvider(interop::provideCppTypeAliases, error))
            return false;
        if (hasSessions && !hasCppSessions &&
            (!compilation.setExperimentalCppTypeProvider(interop::provideCppInteropTypes, error) ||
             !compilation.setExperimentalCppTypeSelector(interop::selectExperimentalCppRecordTypesForCompilation, error) ||
             !compilation.setExperimentalCppInstanceFieldProvider(interop::provideExperimentalCppInstanceFields, error) ||
             !compilation.enableCppImportedFieldLowering(error))) return false;
        if (hasCppSessions &&
            (!compilation.setExperimentalCppTypeProvider(interop::provideCppInteropTypes, error) ||
             !compilation.setExperimentalCppTypeSelector(interop::selectExperimentalCppRecordTypesForCompilation, error) ||
             !compilation.setExperimentalCppInstanceMethodProvider(interop::provideExperimentalCppInstanceMethods, error) ||
             !compilation.setExperimentalCppStaticMethodProvider(interop::provideExperimentalCppStaticMethods, error) ||
             !compilation.setExperimentalCppInstanceFieldProvider(interop::provideExperimentalCppInstanceFields, error) ||
             !compilation.setExperimentalCppStaticFieldProvider(interop::provideExperimentalCppStaticFields, error) ||
             !compilation.setExperimentalCppLifetimeMethodProvider(interop::provideExperimentalCppLifetimeMethods, error) ||
             !compilation.enableCppImportedMethodLowering(error) ||
             !compilation.enableCppImportedFieldLowering(error) ||
             !compilation.enableCppImportedNoexceptLocalLifetime(error) ||
             !compilation.enableExperimentalCppDiscardedLifetimeLoweringForTests(error))) return false;
        if (hasSessions && !compilation.setCppFunctionProvider(
                [&compilation](const auto& graph, auto projects, auto, auto demands, auto& projected,
                   auto& deferred, auto& providerError) {
                    const auto* lookup = compilation.nameLookup();
                    if (!lookup) { providerError = "C/C++ name lookup is unavailable"; return false; }
                    return interop::provideExperimentalCppFunctionsWithRecords(
                        graph, projects, *lookup, demands, projected, deferred, providerError);
                }, error))
            return false;
        if (hasSessions && !compilation.setCppGlobalProvider(
                [&compilation](const auto& graph, auto projects, auto demands, auto& states, auto& providerError) {
                    const auto* lookup = compilation.nameLookup();
                    if (!lookup) { providerError = "C/C++ name lookup is unavailable"; return false; }
                    return interop::provideCppSemanticValuesWithLookup(graph, projects, *lookup, demands, states, providerError);
                }, error))
            return false;
    }
#endif
    const bool deferrable = !manifest.requiredBundleOutputNames().empty();
    const compilation::PrecompiledMaterializationGate gate =
        [&manifest, &compilation, deferrable, cppInputsComplete, cppSessionOwners](
            const compilation::ProjectGraph& closedGraph,
            bool& skipMaterialization,
            std::string& gateError)
        {
            return lookupCompilationBundle(
                manifest, compilation, closedGraph, deferrable, cppInputsComplete, cppSessionOwners,
                skipMaterialization, gateError);
        };
    // Sources the manifest hashed for the bundle identity are served from memory; the others are
    // read from disk.
    const compilation::PrecompiledSurfaceSourceProvider surfaceSources =
        [&manifest](const std::filesystem::path& path)
        {
            return manifest.sourceBuffer(path);
        };
    if (!compilation::preparePrecompiledProject(
            manifest.releaseProjectGraph(), manifest.loadOptions(), compilation, error, gate,
            materializeDemandedSurface ? &surfaceSources : nullptr))
    {
        return false;
    }
    if (compilation.projectGraph() == nullptr)
    {
        error = "compilation input manifest requires a prepared project graph";
        return false;
    }
    bool hasPrecompiledCCatalog = false;
    for (const auto& module : compilation.precompiledModules())
        hasPrecompiledCCatalog |= !module.cppCTypeCatalog.empty();
    if (hasPrecompiledCCatalog)
    {
#if defined(NSC_ENABLE_CPP_INTEROP)
        if (!compilation.setCppTypeAliasProvider(
                [&compilation](const auto& graph, auto projects, auto& aliases, auto& why) {
                    return interop::provideCppTypeAliasesWithPrecompiledCatalogs(
                        graph, projects, compilation.precompiledModules(), aliases, why);
                }, error) ||
            !compilation.setCppPrecompiledParserTypeProvider(interop::providePrecompiledCParserTypes, error) ||
            !compilation.setExperimentalCppTypeSelector(interop::selectCppTypesWithPrecompiledCatalogs, error) ||
            !compilation.setExperimentalCppInstanceFieldProvider(
                [&compilation](const auto& graph, auto projects, const auto& lookup, auto& fields, auto& why) {
                    return interop::provideCppFieldsWithPrecompiledCatalogs(
                        graph, projects, compilation.precompiledModules(), lookup, fields, why);
                }, error) ||
            !compilation.setCppFunctionProvider(
                [&compilation](const auto& graph, auto projects, auto, auto demands,
                               auto& functions, auto& deferred, auto& why) {
                    const auto* lookup = compilation.nameLookup();
                    if (!lookup) { why = "C/C++ name lookup is unavailable"; return false; }
                    return interop::provideCppFunctionsWithPrecompiledCatalogs(
                        graph, projects, compilation.precompiledModules(), *lookup, demands, functions, deferred, why);
                }, error) ||
            !compilation.setCppGlobalProvider(
                [&compilation](const auto& graph, auto projects, auto demands, auto& values, auto& why) {
                    const auto* lookup = compilation.nameLookup();
                    if (!lookup) { why = "C/C++ name lookup is unavailable"; return false; }
                    return interop::provideCppValuesWithPrecompiledCatalogs(
                        graph, projects, compilation.precompiledModules(), *lookup, demands, values, why);
                }, error) || !compilation.enableCppImportedFieldLowering(error)) return false;
#else
        error = "precompiled C type catalogues require a compiler built with C/C++ interoperability";
        return false;
#endif
    }
    compilation.setSourceBufferProvider(
        [&manifest](const std::filesystem::path& path)
        {
            return manifest.sourceBuffer(path);
        });
    return true;
}

bool restoreCompilationBundle(
    const compilation::CompilationInputManifest& manifest,
    bool& restored,
    std::string& error)
{
    compilation::CompilationBundleCache bundleCache;
    return bundleCache.restore(manifest.bundleLookup(), restored, error);
}

void publishCompilationBundle(
    const compilation::CompilationInputManifest& manifest) noexcept
{
    try
    {
        const llvm_backend::CompilationArtifactCache* const artifactCache =
            llvm_backend::activeCompilationArtifactCache();
        if (artifactCache == nullptr || !artifactCache->enabled() || manifest.identity().empty()) return;
        compilation::CompilationBundleCache bundleCache;
        bundleCache.setRoot(artifactCache->root() / "compilation-bundles");
        std::string ignoredError;
        (void)bundleCache.store(
            manifest.identity(),
            manifest.bundleOutputDirectory(),
            manifest.requiredBundleOutputNames(),
            manifest.bundleResultOutput(),
            ignoredError);
    }
    catch (...)
    {
    }
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
