#include "Internal/CompilationInputManifestFactory.h"
#include "Internal/DriverInternal.h"

#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarPath.h"
#include "nativesharp/LLVM/ThinLtoLibrarySidecarCodec.h"
#include "nativesharp/Linker/LldLinker.h"
#include "nativesharp/Linker/WindowsRuntimeImports.h"
#include "nativesharp/Linker/WindowsRuntimeLibrary.h"

#include <memory>
#include <string>
#include <system_error>
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
        manifest.addIdentityField(
            "output-name", outputPath.filename().generic_string());
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
            debugPrefixMaps[index].sourcePrefix.generic_string());
        manifest.addIdentityField(
            prefix + "-replacement",
            debugPrefixMaps[index].replacementPrefix.generic_string());
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
            "debug-source-root", manifest.rootDirectory().generic_string());
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
    manifest.addIdentityField("link-tool-" + role, path.filename().generic_string());
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
void addCompilationLinkToolIdentity(compilation::CompilationInputManifest& manifest)
{
    const std::string product(manifest.bundleProductKind());
    std::string linkerError;
    const std::unique_ptr<linker::LldLinker> linkTool = linker::LldLinker::discover(linkerError);
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
    std::span<const std::filesystem::path> extraLinkLibraries)
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
            extraLinkLibraries[index].filename().generic_string());
    }
    // EN: Freeze 3.0 DEC-13: a bundle built without the host-DLL check must not satisfy a build that
    //     asks for it. The field exists only when requested, so default identities do not change.
    // ES: Congelamiento 3.0 DEC-13: un bundle construido sin la comprobacion contra DLL del host no
    //     debe satisfacer una compilacion que la pide. El campo solo existe cuando se pide, asi que
    //     las identidades por defecto no cambian.
    if (activeExecutionScope != nullptr && activeExecutionScope->validateSystemImports)
        manifest.addIdentityField("validate-system-imports", "true");
    addCompilationLinkToolIdentity(manifest);
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
bool lookupCompilationBundle(
    compilation::CompilationInputManifest& manifest,
    frontend::ProjectCompilation& compilation,
    const compilation::ProjectGraph& closedGraph,
    bool deferrable,
    bool& skipMaterialization,
    std::string& error)
{
    manifest.addProjectGraphInputs(closedGraph);
    compilation::CompilationBundleCache bundleCache;
    const llvm_backend::CompilationArtifactCache* const artifactCache =
        llvm_backend::activeCompilationArtifactCache();
    // Only the bundle cache uses the identity. Without it the inputs are not hashed: the lookup
    // below reports the cache as disabled and the sources are read again by the syntax phase.
    if (artifactCache != nullptr && artifactCache->enabled())
    {
        bundleCache.setRoot(
            artifactCache->root() / "compilation-bundles");
        manifest.setInputDigestCacheRoot(artifactCache->root() / "input-digests");
        if (!manifest.calculateIdentity(
                compilation.libraryMetadataRepository(), error))
        {
            return false;
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
    const bool deferrable = !manifest.requiredBundleOutputNames().empty();
    const compilation::PrecompiledMaterializationGate gate =
        [&manifest, &compilation, deferrable](
            const compilation::ProjectGraph& closedGraph,
            bool& skipMaterialization,
            std::string& gateError)
        {
            return lookupCompilationBundle(
                manifest, compilation, closedGraph, deferrable, skipMaterialization, gateError);
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
        if (artifactCache == nullptr || !artifactCache->enabled()) return;
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
