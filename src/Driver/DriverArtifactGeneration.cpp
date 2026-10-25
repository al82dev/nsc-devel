#include "Internal/DriverInternal.h"
#include "ThinLtoLibraryLinking.h"

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

// Interop Foundation: gather the project's `extern C from "<dll>"` declarations
// as (dll -> its symbols), in first-appearance order, for the linker's
// import-library step. The compiler learned no API by name at any point -- this
// is purely the shape the frontend recorded.
std::vector<linker::ExternalImportModule> collectExternalImports(const frontend::ProjectOopMirModel &mirModel)
{
    std::vector<linker::ExternalImportModule> modules;
    ExternalImportListBuilder moduleList(modules);
    bool anyRuntimeBoundImport = false;
    const auto addSymbol = [&moduleList](const std::string &dllName, const std::string &symbol)
    { moduleList.append(dllName, symbol); };
    for (const frontend::ProjectOopMirFunction &function : mirModel.functions())
    {
        if (!function.isImportedC || function.importDllName.empty())
            continue;
        // C1.2 "import identity": an import whose C symbol name is shared by two
        // libraries has NO import-library entry at all -- there is exactly one slot
        // per symbol name in a PE import table and the two would fight over it. The
        // backend binds those at first call instead.
        if (function.importRequiresRuntimeBinding)
        {
            anyRuntimeBoundImport = true;
            continue;
        }
        // The symbol that goes into the import library is the EXTERNAL one
        // (`cAbiName`), never the internal mangled name -- they coincide for a
        // statically resolved import, and this is the line that keeps them from
        // being confused if they ever stop coinciding.
        addSymbol(function.importDllName, function.cAbiName);
    }
    if (anyRuntimeBoundImport)
    {
        // The two entry points every runtime-bound import needs, requested through
        // the SAME door the project's own imports use, so the runtime's single
        // import table merges and deduplicates them (a project that never needs
        // runtime binding never imports them).
        addSymbol("KERNEL32.dll", "LoadLibraryA");
        addSymbol("KERNEL32.dll", "GetProcAddress");
    }
    return modules;
}

std::unique_ptr<llvm_backend::ProjectOopObjectBundle> generateProjectObjects(
    frontend::ProjectCompilation &compilation, llvm_backend::DebugInformationKind debugInformation,
    llvm_backend::LlvmOptimizationLevel optimizationLevel,
    const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
    llvm_backend::LinkTimeOptimizationMode linkTimeOptimization, std::string &error,
    bool captureThinLtoUnits = false, std::vector<llvm_backend::ThinLtoLibraryUnits> thinLtoLibraries = {})
{
    llvm_backend::ProjectOopObjectGenerator objectGenerator;
    configureObjectGeneratorProgress(objectGenerator);
    objectGenerator.setThinLtoUnitBitcodeCapture(captureThinLtoUnits);
    objectGenerator.setThinLtoLibraryUnits(std::move(thinLtoLibraries));
    std::unique_ptr<llvm_backend::ProjectOopObjectBundle> objectBundle =
        measureDriverPhase("llvm-object-generation",
                           [&]
                           {
                               return objectGenerator.generate(
                                   *compilation.nameLookup(), *compilation.memberModel(), *compilation.oopMirModel(),
                                   compilation.sourceManager(), debugInformation, optimizationLevel,
                                   effectiveCompilationJobs(), debugPrefixMaps, linkTimeOptimization, error,
                                   &compilation.lifetime().workerPool());
                           });
    if (objectBundle == nullptr)
        return nullptr;
    if (!compilation.completeCodeGeneration(error))
        return nullptr;
    return objectBundle;
}

bool prepareExecutableProject(std::string_view inputPath,
                              compilation::CompilationInputManifest &manifest,
                              frontend::ProjectCompilation &compilation,
                              const frontend::ProjectOopMirFunction *&entryFunction,
                               bool &restored, std::ostream &error)
{
    configureProjectExecution(compilation);
    std::string loadError;
    if (!measureDriverPhase(
            "precompiled-metadata",
            [&] { return configurePrecompiledModules(manifest, compilation, loadError, true); }))
    {
        error << "nsc: error: " << loadError << '\n';
        return false;
    }
    if (!restoreCompilationBundle(manifest, restored, loadError))
    {
        error << "nsc: error: compilation bundle restore failed: "
              << loadError << '\n';
        return false;
    }
    if (restored) return true;
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty())
            error << "nsc: error: " << loadError << '\n';
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return false;
    }
    if (compilation.nameLookup() == nullptr || compilation.memberModel() == nullptr ||
        compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 45.12 OOP executable compilation did not "
                 "produce required lowering models\n";
        return false;
    }
    if (compilation.namespaceFunctionModel() == nullptr)
    {
        error << "nsc: internal error: namespace function model is unavailable" << std::endl;
        return false;
    }
    bool ambiguousEntryPoint = false;
    entryFunction =
        compilation.oopMirModel()->executableEntryPoint(*compilation.namespaceFunctionModel(), ambiguousEntryPoint);
    if (ambiguousEntryPoint)
    {
        error << "nsc: error: OOP executable has multiple int Main() entry points" << std::endl;
        return false;
    }
    if (entryFunction == nullptr)
    {
        error << "nsc: error: OOP executable requires exactly one int Main() "
                 "function or static method with no parameters"
              << std::endl;
        return false;
    }
    return true;
}

void printExecutableGenerationResult(const frontend::ProjectOopMirFunction &entryFunction,
                                     const llvm_backend::ProjectOopObjectBundle &objectBundle,
                                     const linker::WindowsExecutableBuilder &builder,
                                     llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                     llvm_backend::DebugInformationKind debugInformation,
                                     llvm_backend::LinkTimeOptimizationMode linkTimeOptimization, bool incrementalLink,
                                     linker::WindowsExecutableSubsystem subsystem, std::size_t linkedLibraryCount,
                                     std::size_t packagedRuntimeCount, std::ostream &output)
{
    output << "project OOP PE executable emission: ok\n"
           << "entry-symbol: " << entryFunction.mangledName << '\n'
           << "machine: AMD64 (0x8664)\n"
           << "format: PE32+\n"
           << "subsystem: " << subsystemDisplayName(subsystem) << '\n'
           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "debug-information: " << llvm_backend::debugInformationKindName(debugInformation) << '\n'
           << "link-time-optimization: " << llvm_backend::linkTimeOptimizationModeName(linkTimeOptimization) << '\n'
           << "code-units: " << objectBundle.codeUnitCount() << '\n'
           << "artifact-cache-hits: " << objectBundle.cacheHitCount() << '\n'
           << "artifact-cache-misses: " << objectBundle.cacheMissCount() << '\n'
           << "thinlto-summaries: " << objectBundle.thinLtoSummaryCount() << '\n'
           << "thinlto-index: "
           << (objectBundle.thinLtoIndexDigest().empty() ? "none" : objectBundle.thinLtoIndexDigest()) << '\n'
           << "thinlto-imports: " << objectBundle.thinLtoImportCount() << '\n'
           << "thinlto-cached-backends: " << objectBundle.thinLtoCachedBackendCount() << '\n'
           << "incremental-link: "
           << (incrementalLink ? (builder.reusedIncrementalLink() ? "hit" : "miss") : "disabled") << '\n'
           << "linked-libraries: " << linkedLibraryCount << '\n'
           << "packaged-runtimes: " << packagedRuntimeCount << '\n'
           << "sections: " << builder.info().sectionCount << '\n'
           << "entry-rva: " << builder.info().entryPointRva << '\n'
           << "exception-directory-rva: " << builder.info().exceptionDirectoryRva << '\n'
           << "exception-directory-size: " << builder.info().exceptionDirectorySize << '\n'
           << "imports:";
    for (const std::string &library : builder.info().importedLibraries)
        output << ' ' << library;
    output << '\n';
    if (activeExecutionScope != nullptr && activeExecutionScope->reportTiming)
        appendCodeUnitTelemetry(output, objectBundle);
}

bool prepareExecutableBuilder(const frontend::ProjectCompilation &compilation,
                              const std::vector<std::filesystem::path> &extraLinkLibraries,
                              llvm_backend::DebugInformationKind debugInformation, bool incrementalLink,
                              linker::WindowsExecutableSubsystem subsystem,
                              std::vector<std::filesystem::path> &linkLibraries,
                              std::vector<std::filesystem::path> &runtimeLibraries,
                              linker::WindowsExecutableBuilder &builder, std::ostream &error)
{
    linkLibraries = extraLinkLibraries;
    std::vector<linker::ExternalImportModule> inheritedImports;
    std::string referenceError;
    if (!appendValidatedLibraryReferences(compilation, linkLibraries, runtimeLibraries, referenceError,
                                          &inheritedImports))
    {
        error << "nsc: error: precompiled library reference validation failed: " << referenceError << '\n';
        return false;
    }
    builder.setProgramObjectsVerified(true);
    builder.setRuntimeLibrary(linker::configuredWindowsRuntimeLibrary());
    builder.setSubsystem(subsystem);
    builder.setDebugInformation(debugInformation);
    builder.setIncrementalLink(incrementalLink);
    std::vector<linker::ExternalImportModule> externalImports = collectExternalImports(*compilation.oopMirModel());
    ExternalImportListBuilder externalImportList(externalImports);
    for (const linker::ExternalImportModule &module : inheritedImports)
        for (const std::string &symbol : module.symbols)
            externalImportList.append(module.dllName, symbol);
    builder.setExternalImports(std::move(externalImports));
    builder.setExtraLinkLibraries(linkLibraries);
    return true;
}

bool publishExecutableArtifact(std::string_view outputPath, const frontend::ProjectOopMirFunction &entryFunction,
                               const llvm_backend::ProjectOopObjectBundle &objectBundle,
                               llvm_backend::LlvmOptimizationLevel optimizationLevel,
                               llvm_backend::DebugInformationKind debugInformation, bool incrementalLink,
                               std::span<const std::filesystem::path> runtimeLibraries,
                               linker::WindowsExecutableBuilder &builder,
                               std::vector<std::filesystem::path> &packagedRuntimes, std::ostream &error)
{
    std::string buildError;
    std::string referenceError;
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    ArtifactPublisher artifactPublisher;
    ExecutablePublication publication;
    const bool publishPdb = debugInformation != llvm_backend::DebugInformationKind::None;
    if (!artifactPublisher.prepareExecutable(destination, publishPdb, incrementalLink, publication, buildError))
    {
        error << "nsc: error: executable output preflight failed: " << buildError << '\n';
        return false;
    }
    if (incrementalLink && !ArtifactPublisher::seedExecutable(publication, buildError))
    {
        error << "nsc: error: incremental-link staging failed: " << buildError << '\n';
        return false;
    }
    if (incrementalLink && publishPdb && !ArtifactPublisher::seedExecutablePdb(publication, buildError))
    {
        error << "nsc: error: incremental-link PDB staging failed: " << buildError << '\n';
        return false;
    }
    if (!measureDriverPhase("executable-build",
                            [&]
                            {
                                return builder.buildObjects(objectBundle.objectFiles(), objectBundle.combinedLlvmText(),
                                                            publication.stage, optimizationLevel,
                                                            entryFunction.mangledName, buildError);
                            }))
    {
        error << "nsc: error: OOP Windows executable generation failed: " << buildError << '\n';
        return false;
    }
    if (!verifyLinkedRuntimeLibraries(runtimeLibraries, builder.info(), referenceError))
    {
        error << "nsc: error: precompiled DLL/import-library mismatch: " << referenceError << '\n';
        return false;
    }
    if (!packageRuntimeClosure(runtimeLibraries, publication.stage, packagedRuntimes, referenceError))
    {
        removePackagedFiles(packagedRuntimes);
        error << "nsc: error: runtime packaging failed: " << referenceError << '\n';
        return false;
    }
    if (publishPdb && !artifactPublisher.addExecutablePdb(publication, buildError))
    {
        error << "nsc: error: PDB output preflight failed: " << buildError << '\n';
        return false;
    }
    if (incrementalLink && !artifactPublisher.addExecutableState(publication, buildError))
    {
        error << "nsc: error: incremental-state output preflight failed: " << buildError << '\n';
        return false;
    }
    if (!artifactPublisher.addRuntimeArtifacts(destination.parent_path(), packagedRuntimes, buildError))
    {
        error << "nsc: error: runtime output preflight failed: " << buildError << '\n';
        return false;
    }
    if (!artifactPublisher.commit(buildError))
    {
        error << "nsc: error: executable publication failed: " << buildError << '\n';
        return false;
    }
    return true;
}

ExitCode Driver::runProjectOopExecutableGeneration(std::string_view inputPath, std::string_view outputPath,
                                                   llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                                   llvm_backend::DebugInformationKind debugInformation,
                                                   const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
                                                   llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
                                                   bool incrementalLink, linker::WindowsExecutableSubsystem subsystem,
                                                   const std::vector<std::filesystem::path> &extraLinkLibraries,
                                                   std::ostream &output, std::ostream &error)
{
    std::string loadError;
    std::optional<compilation::CompilationInputManifest> manifest =
        loadCompilationInputManifest(std::filesystem::path(inputPath), loadError);
    if (!manifest.has_value())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    addCompilationProductIdentity(
        *manifest, "executable", pathFromUtf8(outputPath));
    addCompilationCodegenIdentity(
        *manifest, optimizationLevel, debugInformation,
        debugPrefixMaps, linkTimeOptimization);
    addCompilationLinkIdentity(
        *manifest, incrementalLink, subsystem, extraLinkLibraries);
    frontend::ProjectCompilation compilation;
    const frontend::ProjectOopMirFunction *entryFunction = nullptr;
    bool restored = false;
    if (!prepareExecutableProject(
            inputPath, *manifest, compilation, entryFunction, restored, error))
        return ExitCode::CompilationError;
    if (restored)
    {
        output << manifest->bundleLookup().resultOutput;
        return ExitCode::Success;
    }
    std::string codeGenerationError;
    ThinLtoLibraryLinking thinLtoLibraries;
    if (!thinLtoLibraries.collect(
            compilation, linkTimeOptimization, optimizationLevel, debugInformation, codeGenerationError))
    {
        error << "nsc: error: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }
    std::unique_ptr<llvm_backend::ProjectOopObjectBundle> objectBundle = generateProjectObjects(
        compilation, debugInformation, optimizationLevel, debugPrefixMaps, linkTimeOptimization, codeGenerationError,
        false, thinLtoLibraries.takeLibraries());
    if (objectBundle == nullptr)
    {
        error << "nsc: error: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }

    std::vector<std::filesystem::path> linkLibraries;
    std::vector<std::filesystem::path> runtimeLibraries;
    linker::WindowsExecutableBuilder builder;
    std::vector<std::filesystem::path> packagedRuntimes;
    if (!prepareExecutableBuilder(compilation, extraLinkLibraries, debugInformation, incrementalLink, subsystem,
                                  linkLibraries, runtimeLibraries, builder, error))
        return ExitCode::CompilationError;
    if (!thinLtoLibraries.replaceLinkLibraries(*objectBundle, linkLibraries, codeGenerationError))
    {
        error << "nsc: error: ThinLTO library linking failed: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }
    builder.setExtraLinkLibraries(linkLibraries);
    if (!publishExecutableArtifact(outputPath, *entryFunction, *objectBundle, optimizationLevel, debugInformation,
                                   incrementalLink, runtimeLibraries, builder, packagedRuntimes, error))
        return ExitCode::CompilationError;
    for (const std::filesystem::path &runtime : packagedRuntimes)
        manifest->addRequiredBundleOutput(runtime.filename());
    std::ostringstream resultOutput;
    printExecutableGenerationResult(*entryFunction, *objectBundle, builder, optimizationLevel, debugInformation,
                                    linkTimeOptimization, incrementalLink, subsystem, linkLibraries.size(),
                                    packagedRuntimes.size(), resultOutput);
    thinLtoLibraries.printResult(resultOutput);
    manifest->setBundleResultOutput(resultOutput.str());
    publishCompilationBundle(*manifest);
    output << resultOutput.str();
    return ExitCode::Success;
}

bool prepareStaticLibraryProject(std::string_view inputPath,
                                 compilation::CompilationInputManifest &manifest,
                                 frontend::ProjectCompilation &compilation,
                                 std::vector<std::filesystem::path> &referencedLibraries,
                                 std::vector<std::filesystem::path> &runtimeLibraries,
                                 std::vector<linker::ExternalImportModule> &inheritedImports,
                                 bool &restored, std::ostream &error)
{
    configureProjectExecution(compilation);
    std::string loadError;
    if (!measureDriverPhase(
            "precompiled-metadata",
            [&] { return configurePrecompiledModules(manifest, compilation, loadError, true); }))
    {
        error << "nsc: error: " << loadError << '\n';
        return false;
    }
    if (!restoreCompilationBundle(manifest, restored, loadError))
    {
        error << "nsc: error: compilation bundle restore failed: "
              << loadError << '\n';
        return false;
    }
    if (restored) return true;
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty())
            error << "nsc: error: " << loadError << '\n';
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return false;
    }
    if (compilation.projectGraph() == nullptr || compilation.nameLookup() == nullptr ||
        compilation.memberModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 46 static-library compilation did not produce required models\n";
        return false;
    }
    const compilation::Project *rootProject = compilation.projectGraph()->rootProject();
    if (rootProject == nullptr || rootProject->outputKind() != compilation::ProjectOutputKind::Library)
    {
        error << "nsc: error: --emit-project-oop-lib requires root OutputType Library or StaticLibrary\n";
        return false;
    }
    std::string referenceError;
    if (!appendValidatedLibraryReferences(compilation, referencedLibraries, runtimeLibraries, referenceError,
                                          &inheritedImports))
    {
        error << "nsc: error: precompiled library reference validation failed: " << referenceError << '\n';
        return false;
    }
    return true;
}

bool publishStaticLibraryArtifact(std::string_view outputPath, const frontend::ProjectCompilation &compilation,
                                  const llvm_backend::ProjectOopObjectBundle &objectBundle,
                                  std::span<const std::filesystem::path> referencedLibraries,
                                  std::span<const linker::ExternalImportModule> inheritedImports,
                                  llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                  linker::WindowsLibraryBuilder &builder, StaticLibraryPublication &publication,
                                  std::ostream &error)
{
    builder.setProgramObjectsVerified(true);
    builder.setRuntimeLibrary(linker::configuredWindowsRuntimeLibrary());
    std::vector<linker::ExternalImportModule> externalImports = collectExternalImports(*compilation.oopMirModel());
    ExternalImportListBuilder externalImportList(externalImports);
    for (const linker::ExternalImportModule &module : inheritedImports)
        for (const std::string &symbol : module.symbols)
            externalImportList.append(module.dllName, symbol);
    builder.setExternalImports(std::move(externalImports));
    builder.setExtraLinkLibraries(
        std::vector<std::filesystem::path>(referencedLibraries.begin(), referencedLibraries.end()));
    std::string buildError;
    ArtifactPublisher artifactPublisher;
    if (!artifactPublisher.prepareStaticLibrary(pathFromUtf8(outputPath), publication, buildError))
    {
        error << "nsc: error: static-library output preflight failed: " << buildError << '\n';
        return false;
    }
    if (!measureDriverPhase("static-library-build",
                            [&]
                            {
                                return builder.buildStaticObjects(objectBundle.objectFiles(), publication.stage,
                                                                  optimizationLevel, buildError);
                            }))
    {
        error << "nsc: error: NativeSharp static-library generation failed: " << buildError << '\n';
        return false;
    }
    interop::CHeaderGenerator headerGenerator;
    std::string headerError;
    if (!measureDriverPhase("host-header", [&]
                            { return headerGenerator.writeHeader(compilation, publication.headerStage, headerError); }))
    {
        error << "nsc: error: Stage 48.1 static-library C header generation failed: " << headerError << '\n';
        return false;
    }
    std::string metadataError;
    if (!measureDriverPhase(
            "library-metadata",
            [&] { return writeVerifiedLibraryMetadata(compilation, publication.metadataStage, metadataError); }))
    {
        error << "nsc: error: Stage 48.2 static-library metadata generation failed: " << metadataError << '\n';
        return false;
    }
    publishPrecompiledSnapshot(compilation, publication.metadataStage);
    if (!objectBundle.thinLtoUnitBitcode().empty())
    {
        // The sidecar binds the staged .lib and .nsmeta, so it is written after both (item 48).
        // El sidecar liga la .lib y la .nsmeta en staging, asi que se escribe despues de ambas.
        if (!ThinLtoLibraryLinking::writeSidecar(objectBundle, publication.stage, publication.metadataStage,
                                                 publication.thinLtoSidecarStage, buildError) ||
            !artifactPublisher.addStaticLibraryThinLtoSidecar(publication, buildError))
        {
            error << "nsc: error: static-library ThinLTO sidecar generation failed: " << buildError << '\n';
            return false;
        }
        publication.thinLtoSidecarStaged = true;
    }
    if (!artifactPublisher.commit(buildError))
    {
        error << "nsc: error: static-library publication failed: " << buildError << '\n';
        return false;
    }
    if (!publication.thinLtoSidecarStaged)
    {
        // A sidecar left by an earlier ThinLTO build would only be rejected; remove it anyway.
        // Un sidecar de un build ThinLTO anterior solo seria rechazado; se borra de todos modos.
        std::error_code staleError;
        std::filesystem::remove(publication.thinLtoSidecarDestination, staleError);
    }
    return true;
}

void printStaticLibraryGenerationResult(const llvm_backend::ProjectOopObjectBundle &objectBundle,
                                        const linker::WindowsLibraryBuilder &builder,
                                        const StaticLibraryPublication &publication,
                                        llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                        llvm_backend::DebugInformationKind debugInformation,
                                        llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
                                        std::size_t referencedLibraryCount, std::ostream &output)
{
    output << "project OOP static library emission: ok\n"
           << "format: COFF archive (.lib)\n"
           << "machine-members: AMD64 COFF\n"
           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "debug-information: " << llvm_backend::debugInformationKindName(debugInformation) << '\n'
           << "link-time-optimization: " << llvm_backend::linkTimeOptimizationModeName(linkTimeOptimization) << '\n'
           << "code-units: " << objectBundle.codeUnitCount() << '\n'
           << "artifact-cache-hits: " << objectBundle.cacheHitCount() << '\n'
           << "artifact-cache-misses: " << objectBundle.cacheMissCount() << '\n'
           << "runtime-object-cache: "
           << (!builder.runtimeObjectCacheAttempted() ? "disabled-or-external"
                                                      : (builder.reusedRuntimeObject() ? "hit" : "miss"))
           << '\n'
           << "static-archive-cache: "
           << (!builder.staticArchiveCacheAttempted() ? "disabled" : (builder.reusedStaticArchive() ? "hit" : "miss"))
           << '\n'
           << "thinlto-summaries: " << objectBundle.thinLtoSummaryCount() << '\n'
           << "thinlto-index: "
           << (objectBundle.thinLtoIndexDigest().empty() ? "none" : objectBundle.thinLtoIndexDigest()) << '\n'
           << "thinlto-imports: " << objectBundle.thinLtoImportCount() << '\n'
           << "thinlto-cached-backends: " << objectBundle.thinLtoCachedBackendCount() << '\n'
           << "debug-companion: "
           << (debugInformation == llvm_backend::DebugInformationKind::None ? "none"
                                                                            : "CodeView in COFF archive members")
           << '\n'
           << "library-references: " << referencedLibraryCount << '\n'
           << "archive-members: " << builder.archiveInfo().memberCount << '\n'
           << "host-header: " << publication.headerDestination.generic_string() << '\n'
           << "library-metadata: " << publication.metadataDestination.generic_string() << '\n';
    if (publication.thinLtoSidecarStaged)
        output << "thinlto-library-sidecar: " << publication.thinLtoSidecarDestination.generic_string() << '\n';
    if (activeExecutionScope != nullptr && activeExecutionScope->reportTiming)
        appendCodeUnitTelemetry(output, objectBundle);
}

ExitCode Driver::runProjectOopStaticLibraryGeneration(std::string_view inputPath, std::string_view outputPath,
                                                      llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                                      llvm_backend::DebugInformationKind debugInformation,
                                                      const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
                                                      llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
                                                      std::ostream &output, std::ostream &error)
{
    std::string loadError;
    std::optional<compilation::CompilationInputManifest> manifest =
        loadCompilationInputManifest(std::filesystem::path(inputPath), loadError);
    if (!manifest.has_value())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    addCompilationProductIdentity(
        *manifest, "static-library", pathFromUtf8(outputPath));
    addCompilationCodegenIdentity(
        *manifest, optimizationLevel, debugInformation,
        debugPrefixMaps, linkTimeOptimization);
    addCompilationLinkToolIdentity(*manifest);
    frontend::ProjectCompilation compilation;
    std::vector<std::filesystem::path> referencedLibraries;
    std::vector<std::filesystem::path> runtimeLibraries;
    std::vector<linker::ExternalImportModule> inheritedImports;
    bool restored = false;
    if (!prepareStaticLibraryProject(inputPath, *manifest, compilation, referencedLibraries,
                                     runtimeLibraries, inheritedImports, restored, error))
        return ExitCode::CompilationError;
    if (restored)
    {
        output << manifest->bundleLookup().resultOutput;
        return ExitCode::Success;
    }

    // A static library publishes a ThinLTO sidecar only when its archive holds exactly the objects of
    // its units, that is, with the NativeSharp runtime configured as a separate library (item 48).
    // Una biblioteca estatica publica sidecar ThinLTO solo cuando su archivo contiene exactamente los
    // objetos de sus unidades, es decir, con el runtime de NativeSharp configurado aparte (punto 48).
    const bool captureThinLtoUnits =
        llvm_backend::ProjectOopObjectGenerator::thinLtoRuns(linkTimeOptimization, optimizationLevel) &&
        !linker::configuredWindowsRuntimeLibrary().empty();
    std::string codeGenerationError;
    std::unique_ptr<llvm_backend::ProjectOopObjectBundle> objectBundle = generateProjectObjects(
        compilation, debugInformation, optimizationLevel, debugPrefixMaps, linkTimeOptimization, codeGenerationError,
        captureThinLtoUnits);
    if (objectBundle == nullptr)
    {
        error << "nsc: error: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }

    linker::WindowsLibraryBuilder builder;
    StaticLibraryPublication publication;
    if (!publishStaticLibraryArtifact(outputPath, compilation, *objectBundle, referencedLibraries, inheritedImports,
                                      optimizationLevel, builder, publication, error))
        return ExitCode::CompilationError;
    std::ostringstream resultOutput;
    printStaticLibraryGenerationResult(*objectBundle, builder, publication, optimizationLevel, debugInformation,
                                       linkTimeOptimization, referencedLibraries.size(), resultOutput);
    manifest->setBundleResultOutput(resultOutput.str());
    publishCompilationBundle(*manifest);
    output << resultOutput.str();
    return ExitCode::Success;
}

bool prepareDynamicLibraryProject(std::string_view inputPath,
                                  compilation::CompilationInputManifest &manifest,
                                  frontend::ProjectCompilation &compilation,
                                  bool &restored, std::ostream &error)
{
    configureProjectExecution(compilation);
    std::string loadError;
    if (!measureDriverPhase(
            "precompiled-metadata",
            [&] { return configurePrecompiledModules(manifest, compilation, loadError, true); }))
    {
        error << "nsc: error: " << loadError << '\n';
        return false;
    }
    if (!restoreCompilationBundle(manifest, restored, loadError))
    {
        error << "nsc: error: compilation bundle restore failed: "
              << loadError << '\n';
        return false;
    }
    if (restored) return true;
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty())
            error << "nsc: error: " << loadError << '\n';
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return false;
    }
    if (compilation.projectGraph() == nullptr || compilation.nameLookup() == nullptr ||
        compilation.memberModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        error << "nsc: error: Stage 47 DLL compilation did not produce required models\n";
        return false;
    }
    const compilation::Project *rootProject = compilation.projectGraph()->rootProject();
    if (rootProject == nullptr || rootProject->outputKind() != compilation::ProjectOutputKind::SharedLibrary)
    {
        error << "nsc: error: --emit-project-oop-dll requires root OutputType SharedLibrary\n";
        return false;
    }
    return true;
}

void collectDynamicMethodExports(const frontend::ProjectCompilation &compilation,
                                 std::span<const frontend::NamedTypeDeclaration> types,
                                 std::span<const frontend::ClassMemberSymbol> members, std::size_t rootProjectOrdinal,
                                 std::vector<std::string> &exportSymbols)
{
    for (const frontend::ProjectOopMirFunction &function : compilation.oopMirModel()->functions())
    {
        if (function.isLambda || function.destructor || function.declaringTypeOrdinal >= types.size())
            continue;
        const frontend::NamedTypeDeclaration &type = types[function.declaringTypeOrdinal];
        if (function.isSyntheticImplicitConstructor)
        {
            if (type.projectOrdinal == rootProjectOrdinal &&
                isPubliclyReachableNamedType(types, function.declaringTypeOrdinal))
                exportSymbols.push_back(function.mangledName);
            continue;
        }
        if (function.memberOrdinal >= members.size())
            continue;
        const frontend::ClassMemberSymbol &member = members[function.memberOrdinal];
        if (type.projectOrdinal != rootProjectOrdinal ||
            !isPubliclyReachableNamedType(types, function.declaringTypeOrdinal) ||
            !hasConsumerCallableAccessibility(member.accessibility))
            continue;
        if (function.constructor)
        {
            exportSymbols.push_back(function.mangledName);
            continue;
        }
        if (member.kind != frontend::ClassMemberKind::Method ||
            (!member.isStatic && type.kind != frontend::NamedTypeDeclarationKind::Struct &&
             type.kind != frontend::NamedTypeDeclarationKind::Class &&
             type.kind != frontend::NamedTypeDeclarationKind::Interface) ||
            member.isAbstract || member.isCompilerGenerated || (member.isExternC && !member.isExportedC))
            continue;
        exportSymbols.push_back(function.mangledName);
        if (member.isExportedC)
            exportSymbols.push_back(member.cAbiName);
    }
}

void collectDynamicNamespaceExports(const frontend::ProjectCompilation &compilation, std::size_t rootProjectOrdinal,
                                    std::vector<std::string> &exportSymbols)
{
    if (compilation.namespaceFunctionModel() == nullptr)
        return;
    const auto namespaceFunctions = compilation.namespaceFunctionModel()->functions();
    for (const frontend::ProjectOopMirFunction &function : compilation.oopMirModel()->functions())
    {
        if (!function.namespaceFunctionOrdinal.has_value() ||
            *function.namespaceFunctionOrdinal >= namespaceFunctions.size())
            continue;
        const frontend::NamespaceFunctionSymbol &symbol = namespaceFunctions[*function.namespaceFunctionOrdinal];
        if (symbol.projectOrdinal != rootProjectOrdinal ||
            symbol.accessibility != frontend::NamespaceFunctionAccessibility::Public || function.isImportedC ||
            function.isImportedNative)
            continue;
        exportSymbols.push_back(function.mangledName);
        if (function.isExportedC && !function.cAbiName.empty())
            exportSymbols.push_back(function.cAbiName);
    }
}

void collectDynamicLayoutExports(const frontend::ProjectCompilation &compilation,
                                 std::span<const frontend::NamedTypeDeclaration> types, std::size_t rootProjectOrdinal,
                                 std::vector<std::string> &exportSymbols)
{
    for (const frontend::ProjectOopMirDispatchTable &table : compilation.oopMirModel()->dispatchTables())
    {
        if (table.isImported || table.typeOrdinal >= types.size())
            continue;
        const frontend::NamedTypeDeclaration &type = types[table.typeOrdinal];
        if (type.projectOrdinal == rootProjectOrdinal && !type.isStatic &&
            isPubliclyReachableNamedType(types, table.typeOrdinal))
            exportSymbols.push_back(table.descriptorAccessorName);
    }
    for (const frontend::ProjectOopMirStructLayout &layout : compilation.oopMirModel()->structLayouts())
    {
        if (layout.isImported || !layout.requiresDrop || layout.dropGlueEntryName.empty() ||
            layout.typeOrdinal >= types.size())
            continue;
        const frontend::NamedTypeDeclaration &type = types[layout.typeOrdinal];
        if (type.projectOrdinal == rootProjectOrdinal && isPubliclyReachableNamedType(types, layout.typeOrdinal))
            exportSymbols.push_back(layout.dropGlueEntryName);
    }
}

void collectDynamicStateExports(const frontend::ProjectCompilation &compilation,
                                std::span<const frontend::NamedTypeDeclaration> types,
                                std::span<const frontend::ClassMemberSymbol> members, std::size_t rootProjectOrdinal,
                                std::vector<std::string> &exportSymbols)
{
    for (const frontend::ProjectOopMirStaticField &field : compilation.oopMirModel()->staticFields())
    {
        if (field.isImported || field.isConst || field.declaringTypeOrdinal >= types.size() ||
            field.memberOrdinal >= members.size())
            continue;
        const frontend::NamedTypeDeclaration &type = types[field.declaringTypeOrdinal];
        const frontend::ClassMemberSymbol &member = members[field.memberOrdinal];
        if (type.projectOrdinal == rootProjectOrdinal &&
            isPubliclyReachableNamedType(types, field.declaringTypeOrdinal) &&
            hasConsumerCallableAccessibility(member.accessibility))
            exportSymbols.push_back(field.accessorName);
    }
    if (compilation.namespaceFunctionModel() == nullptr)
        return;
    for (const frontend::NamespaceStateSymbol &state : compilation.namespaceFunctionModel()->states())
    {
        if (state.projectOrdinal == rootProjectOrdinal &&
            state.accessibility == frontend::NamespaceFunctionAccessibility::Public && !state.isConst &&
            !state.isPrecompiled)
            exportSymbols.push_back("__nsc_get_" + state.storageName);
    }
}

bool hasDynamicPublicMetadataSurface(const frontend::ProjectCompilation &compilation,
                                     std::span<const frontend::NamedTypeDeclaration> types,
                                     std::size_t rootProjectOrdinal)
{
    const bool hasPublicType = std::ranges::any_of(
        types,
        [&](const auto &type)
        {
            return type.projectOrdinal == rootProjectOrdinal &&
                   isPubliclyReachableNamedType(types, static_cast<std::size_t>(&type - types.data()));
        });
    const bool hasPublicFunction =
        compilation.namespaceFunctionModel() != nullptr &&
        std::ranges::any_of(compilation.namespaceFunctionModel()->functions(),
                            [&](const auto &function)
                            {
                                return function.projectOrdinal == rootProjectOrdinal &&
                                       function.accessibility == frontend::NamespaceFunctionAccessibility::Public &&
                                       !function.isPrecompiled;
                            });
    const bool hasPublicState =
        compilation.namespaceFunctionModel() != nullptr &&
        std::ranges::any_of(compilation.namespaceFunctionModel()->states(),
                            [&](const auto &state)
                            {
                                return state.projectOrdinal == rootProjectOrdinal &&
                                       state.accessibility == frontend::NamespaceFunctionAccessibility::Public &&
                                       !state.isPrecompiled;
                            });
    const bool hasPublicGeneric =
        compilation.genericModel() != nullptr &&
        std::ranges::any_of(compilation.genericModel()->templates(),
                            [&](const auto &generic)
                            {
                                if (generic.projectOrdinal != rootProjectOrdinal || generic.syntax == nullptr)
                                    return false;
                                return generic.kind == frontend::GenericTemplateKind::Method
                                           ? generic.syntax->hasModifier(lexer::TokenKind::KwPublic)
                                           : generic.syntax->tokenKind() == lexer::TokenKind::KwPublic;
                            });
    return hasPublicType || hasPublicFunction || hasPublicState || hasPublicGeneric;
}

bool collectDynamicLibraryExports(const frontend::ProjectCompilation &compilation,
                                  std::vector<std::string> &exportSymbols, std::ostream &error)
{
    const auto types = compilation.nameLookup()->namedTypes();
    const auto members = compilation.memberModel()->members();
    const std::size_t rootProjectOrdinal = compilation.projectGraph()->projects().size() - 1;
    collectDynamicMethodExports(compilation, types, members, rootProjectOrdinal, exportSymbols);
    collectDynamicNamespaceExports(compilation, rootProjectOrdinal, exportSymbols);
    collectDynamicLayoutExports(compilation, types, rootProjectOrdinal, exportSymbols);
    collectDynamicStateExports(compilation, types, members, rootProjectOrdinal, exportSymbols);
    std::sort(exportSymbols.begin(), exportSymbols.end());
    exportSymbols.erase(std::unique(exportSymbols.begin(), exportSymbols.end()), exportSymbols.end());
    if (!exportSymbols.empty() || hasDynamicPublicMetadataSurface(compilation, types, rootProjectOrdinal))
        return true;
    error << "nsc: error: NativeSharp DLL requires at least one publishable "
             "public type or method in the root project\n";
    return false;
}

bool prepareDynamicLibraryBuilder(const frontend::ProjectCompilation &compilation,
                                  const std::vector<std::filesystem::path> &extraLinkLibraries,
                                  std::vector<std::filesystem::path> &linkLibraries,
                                  std::vector<std::filesystem::path> &runtimeLibraries,
                                  linker::WindowsLibraryBuilder &builder, std::ostream &error)
{
    linkLibraries = extraLinkLibraries;
    std::vector<linker::ExternalImportModule> inheritedImports;
    std::string referenceError;
    if (!appendValidatedLibraryReferences(compilation, linkLibraries, runtimeLibraries, referenceError,
                                          &inheritedImports))
    {
        error << "nsc: error: precompiled library reference validation failed: " << referenceError << '\n';
        return false;
    }
    builder.setProgramObjectsVerified(true);
    builder.setRuntimeLibrary(linker::configuredWindowsRuntimeLibrary());
    std::vector<linker::ExternalImportModule> externalImports = collectExternalImports(*compilation.oopMirModel());
    ExternalImportListBuilder externalImportList(externalImports);
    for (const linker::ExternalImportModule &module : inheritedImports)
        for (const std::string &symbol : module.symbols)
            externalImportList.append(module.dllName, symbol);
    builder.setExternalImports(std::move(externalImports));
    builder.setExtraLinkLibraries(linkLibraries);
    return true;
}

bool buildDynamicLibraryProduct(const std::filesystem::path &destination,
                                const frontend::ProjectCompilation &compilation,
                                const llvm_backend::ProjectOopObjectBundle &objectBundle,
                                std::span<const std::string> exportSymbols,
                                std::span<const std::filesystem::path> runtimeLibraries,
                                llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                llvm_backend::DebugInformationKind debugInformation,
                                linker::WindowsLibraryBuilder &builder, ArtifactPublisher &artifactPublisher,
                                DynamicLibraryPublication &publication, std::string &buildError, std::ostream &error)
{
    const bool publishPdb = debugInformation != llvm_backend::DebugInformationKind::None;
    if (!artifactPublisher.prepareDynamicLibrary(destination, publishPdb, publication, buildError))
    {
        error << "nsc: error: DLL output preflight failed: " << buildError << '\n';
        return false;
    }
    if (!measureDriverPhase("dynamic-library-build",
                            [&]
                            {
                                return builder.buildDynamicObjects(objectBundle.objectFiles(),
                                                                   objectBundle.combinedLlvmText(), exportSymbols,
                                                                   publication.stage, publication.importLibraryStage,
                                                                   optimizationLevel, debugInformation, buildError);
                            }))
    {
        error << "nsc: error: NativeSharp DLL generation failed: " << buildError << '\n';
        return false;
    }
    std::string referenceError;
    if (!verifyLinkedRuntimeLibraries(runtimeLibraries, builder.dynamicLibraryInfo(), referenceError))
    {
        error << "nsc: error: precompiled DLL/import-library mismatch: " << referenceError << '\n';
        return false;
    }
    interop::CHeaderGenerator headerGenerator;
    std::string headerError;
    if (!measureDriverPhase("host-header", [&]
                            { return headerGenerator.writeHeader(compilation, publication.headerStage, headerError); }))
    {
        error << "nsc: error: Stage 48.1 DLL C header generation failed: " << headerError << '\n';
        return false;
    }
    std::string metadataError;
    if (!measureDriverPhase(
            "library-metadata",
            [&] { return writeVerifiedLibraryMetadata(compilation, publication.metadataStage, metadataError); }))
    {
        error << "nsc: error: Stage 48.2 DLL metadata generation failed: " << metadataError << '\n';
        return false;
    }
    publishPrecompiledSnapshot(compilation, publication.metadataStage);
    return true;
}

bool commitDynamicLibraryProduct(const std::filesystem::path &destination,
                                 std::span<const std::filesystem::path> runtimeLibraries, bool publishPdb,
                                 ArtifactPublisher &artifactPublisher, DynamicLibraryPublication &publication,
                                 std::string &buildError, std::vector<std::filesystem::path> &packagedRuntimes,
                                 std::ostream &error)
{
    std::string referenceError;
    if (!packageRuntimeClosure(runtimeLibraries, publication.stage, packagedRuntimes, referenceError))
    {
        removePackagedFiles(packagedRuntimes);
        error << "nsc: error: runtime packaging failed: " << referenceError << '\n';
        return false;
    }
    if (publishPdb && !artifactPublisher.addDynamicLibraryPdb(publication, buildError))
    {
        error << "nsc: error: DLL PDB output preflight failed: " << buildError << '\n';
        return false;
    }
    if (!artifactPublisher.addRuntimeArtifacts(destination.parent_path(), packagedRuntimes, buildError))
    {
        error << "nsc: error: DLL runtime output preflight failed: " << buildError << '\n';
        return false;
    }
    if (!artifactPublisher.commit(buildError))
    {
        error << "nsc: error: DLL publication failed: " << buildError << '\n';
        return false;
    }
    return true;
}

void printDynamicLibraryGenerationResult(const llvm_backend::ProjectOopObjectBundle &objectBundle,
                                         const linker::WindowsLibraryBuilder &builder,
                                         const DynamicLibraryPublication &publication,
                                         llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                         llvm_backend::DebugInformationKind debugInformation,
                                         std::size_t linkLibraryCount, std::size_t packagedRuntimeCount,
                                         std::ostream &output)
{
    output << "project OOP DLL emission: ok\n"
           << "format: PE32+ DLL\n"
           << "machine: AMD64 (0x8664)\n"
           << "optimization: " << llvm_backend::llvmOptimizationLevelName(optimizationLevel) << '\n'
           << "debug-information: " << llvm_backend::debugInformationKindName(debugInformation) << '\n'
           << "code-units: " << objectBundle.codeUnitCount() << '\n'
           << "artifact-cache-hits: " << objectBundle.cacheHitCount() << '\n'
           << "artifact-cache-misses: " << objectBundle.cacheMissCount() << '\n'
           << "program-database: "
           << (debugInformation == llvm_backend::DebugInformationKind::None
                   ? "none"
                   : publication.pdbDestination.generic_string())
           << '\n'
           << "linked-libraries: " << linkLibraryCount << '\n'
           << "packaged-runtimes: " << packagedRuntimeCount << '\n'
           << "import-library: " << publication.importLibraryDestination.generic_string() << '\n'
           << "host-header: " << publication.headerDestination.generic_string() << '\n'
           << "library-metadata: " << publication.metadataDestination.generic_string() << '\n'
           << "exports: " << builder.dynamicLibraryInfo().exportedSymbols.size() << '\n';
    for (const std::string &symbol : builder.dynamicLibraryInfo().exportedSymbols)
        output << "  " << symbol << '\n';
    output << "exception-directory-rva: " << builder.dynamicLibraryInfo().exceptionDirectoryRva << '\n'
           << "exception-directory-size: " << builder.dynamicLibraryInfo().exceptionDirectorySize << '\n'
           << "clr-header-rva: " << builder.dynamicLibraryInfo().clrDirectoryRva << '\n';
    if (activeExecutionScope != nullptr && activeExecutionScope->reportTiming)
        appendCodeUnitTelemetry(output, objectBundle);
}

ExitCode Driver::runProjectOopDynamicLibraryGeneration(std::string_view inputPath, std::string_view outputPath,
                                                       llvm_backend::LlvmOptimizationLevel optimizationLevel,
                                                       llvm_backend::DebugInformationKind debugInformation,
                                                       const std::vector<llvm_backend::DebugPrefixMap> &debugPrefixMaps,
                                                       llvm_backend::LinkTimeOptimizationMode linkTimeOptimization,
                                                       const std::vector<std::filesystem::path> &extraLinkLibraries,
                                                       std::ostream &output, std::ostream &error)
{
    std::string loadError;
    std::optional<compilation::CompilationInputManifest> manifest =
        loadCompilationInputManifest(std::filesystem::path(inputPath), loadError);
    if (!manifest.has_value())
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    addCompilationProductIdentity(
        *manifest, "dynamic-library", pathFromUtf8(outputPath));
    addCompilationCodegenIdentity(
        *manifest, optimizationLevel, debugInformation,
        debugPrefixMaps, linkTimeOptimization);
    addCompilationLinkIdentity(
        *manifest, false, linker::WindowsExecutableSubsystem::Console,
        extraLinkLibraries);
    frontend::ProjectCompilation compilation;
    bool restored = false;
    if (!prepareDynamicLibraryProject(
            inputPath, *manifest, compilation, restored, error))
        return ExitCode::CompilationError;
    if (restored)
    {
        output << manifest->bundleLookup().resultOutput;
        return ExitCode::Success;
    }

    std::vector<std::string> exportSymbols;
    if (!collectDynamicLibraryExports(compilation, exportSymbols, error))
        return ExitCode::CompilationError;
    std::string codeGenerationError;
    ThinLtoLibraryLinking thinLtoLibraries;
    if (!thinLtoLibraries.collect(
            compilation, linkTimeOptimization, optimizationLevel, debugInformation, codeGenerationError))
    {
        error << "nsc: error: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }
    std::unique_ptr<llvm_backend::ProjectOopObjectBundle> objectBundle = generateProjectObjects(
        compilation, debugInformation, optimizationLevel, debugPrefixMaps, linkTimeOptimization, codeGenerationError,
        false, thinLtoLibraries.takeLibraries());
    if (objectBundle == nullptr)
    {
        error << "nsc: error: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }

    const std::filesystem::path destination = pathFromUtf8(outputPath);
    std::vector<std::filesystem::path> linkLibraries;
    std::vector<std::filesystem::path> runtimeLibraries;
    linker::WindowsLibraryBuilder builder;
    if (!prepareDynamicLibraryBuilder(compilation, extraLinkLibraries, linkLibraries, runtimeLibraries, builder, error))
        return ExitCode::CompilationError;
    if (!thinLtoLibraries.replaceLinkLibraries(*objectBundle, linkLibraries, codeGenerationError))
    {
        error << "nsc: error: ThinLTO library linking failed: " << codeGenerationError << '\n';
        return ExitCode::CompilationError;
    }
    builder.setExtraLinkLibraries(linkLibraries);

    std::string buildError;
    ArtifactPublisher artifactPublisher;
    DynamicLibraryPublication publication;
    if (!buildDynamicLibraryProduct(destination, compilation, *objectBundle, exportSymbols, runtimeLibraries,
                                    optimizationLevel, debugInformation, builder, artifactPublisher, publication,
                                    buildError, error))
        return ExitCode::CompilationError;

    const bool publishPdb = debugInformation != llvm_backend::DebugInformationKind::None;
    std::vector<std::filesystem::path> packagedRuntimes;
    if (!commitDynamicLibraryProduct(destination, runtimeLibraries, publishPdb, artifactPublisher, publication,
                                     buildError, packagedRuntimes, error))
        return ExitCode::CompilationError;
    for (const std::filesystem::path &runtime : packagedRuntimes)
        manifest->addRequiredBundleOutput(runtime.filename());
    std::ostringstream resultOutput;
    printDynamicLibraryGenerationResult(*objectBundle, builder, publication, optimizationLevel, debugInformation,
                                        linkLibraries.size(), packagedRuntimes.size(), resultOutput);
    thinLtoLibraries.printResult(resultOutput);
    manifest->setBundleResultOutput(resultOutput.str());
    publishCompilationBundle(*manifest);
    output << resultOutput.str();
    return ExitCode::Success;
}
} // namespace nativesharp::driver
