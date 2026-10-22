#include "Internal/DriverInternal.h"

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

ExitCode Driver::runProjectHostHeaderGeneration(std::string_view inputPath, std::string_view outputPath,
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
        *manifest, "host-header", pathFromUtf8(outputPath));
    frontend::ProjectCompilation compilation;
    configureProjectExecution(compilation);
    if (!measureDriverPhase(
            "precompiled-metadata",
            [&] { return configurePrecompiledModules(*manifest, compilation, loadError); }))
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    bool restored = false;
    if (!restoreCompilationBundle(*manifest, restored, loadError))
    {
        error << "nsc: error: compilation bundle restore failed: "
              << loadError << '\n';
        return ExitCode::CompilationError;
    }
    if (restored) return ExitCode::Success;
    if (!compilation.compileProjectObjectLayouts(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty())
            error << "nsc: error: " << loadError << '\n';
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }

    if (compilation.projectGraph() == nullptr || compilation.projectGraph()->rootProject() == nullptr)
    {
        error << "nsc: error: Stage 48.1 C header generation requires a valid root "
                 "project\n";
        return ExitCode::CompilationError;
    }
    const compilation::ProjectOutputKind outputKind = compilation.projectGraph()->rootProject()->outputKind();
    if (outputKind != compilation::ProjectOutputKind::Library &&
        outputKind != compilation::ProjectOutputKind::SharedLibrary)
    {
        error << "nsc: error: Stage 48.1 C headers require OutputType Library, "
                 "StaticLibrary, or SharedLibrary\n";
        return ExitCode::CompilationError;
    }

    interop::CHeaderGenerator generator;
    std::string generationError;
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    if (!measureDriverPhase("host-header",
                            [&] { return generator.writeHeader(compilation, destination, generationError); }))
    {
        error << "nsc: error: Stage 48.1 C header generation failed: " << generationError << '\n';
        return ExitCode::CompilationError;
    }

    output << "C host header emission: ok\n"
           << "output: " << destination.generic_string() << '\n';
    return ExitCode::Success;
}

ExitCode Driver::runProjectLibraryMetadataGeneration(std::string_view inputPath, std::string_view outputPath,
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
        *manifest, "library-metadata", pathFromUtf8(outputPath));
    frontend::ProjectCompilation compilation;
    configureProjectExecution(compilation);
    if (!measureDriverPhase(
            "precompiled-metadata",
            [&] { return configurePrecompiledModules(*manifest, compilation, loadError); }))
    {
        error << "nsc: error: " << loadError << '\n';
        return ExitCode::CompilationError;
    }
    bool restored = false;
    if (!restoreCompilationBundle(*manifest, restored, loadError))
    {
        error << "nsc: error: compilation bundle restore failed: "
              << loadError << '\n';
        return ExitCode::CompilationError;
    }
    if (restored) return ExitCode::Success;
    if (!compilation.compileProjectOopMir(std::filesystem::path(inputPath), loadError))
    {
        if (!loadError.empty())
            error << "nsc: error: " << loadError << '\n';
        if (compilation.diagnostics().hasErrors())
        {
            diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
            renderer.renderAll(compilation.diagnostics(), error);
        }
        return ExitCode::CompilationError;
    }
    if (compilation.projectGraph() == nullptr || compilation.projectGraph()->rootProject() == nullptr)
    {
        error << "nsc: error: Stage 48.2 metadata generation requires a valid root "
                 "project\n";
        return ExitCode::CompilationError;
    }
    const compilation::ProjectOutputKind outputKind = compilation.projectGraph()->rootProject()->outputKind();
    if (outputKind != compilation::ProjectOutputKind::Library &&
        outputKind != compilation::ProjectOutputKind::SharedLibrary)
    {
        error << "nsc: error: Stage 48.2 metadata requires OutputType Library, "
                 "StaticLibrary, or SharedLibrary\n";
        return ExitCode::CompilationError;
    }

    std::string metadataError;
    const std::filesystem::path destination = pathFromUtf8(outputPath);
    interop::LibraryMetadata metadata;
    if (!measureDriverPhase(
            "library-metadata",
            [&] { return writeVerifiedLibraryMetadata(compilation, destination, metadataError, &metadata); }))
    {
        error << "nsc: error: Stage 48.2 metadata generation failed: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }

    output << "NativeSharp library metadata emission: ok\n"
           << "output: " << destination.generic_string() << '\n'
           << "format: NSMETA " << metadata.formatMajor << '.' << metadata.formatMinor << '\n'
           << "module: " << metadata.moduleName << '\n'
           << "target: " << metadata.targetTriple << '\n'
           << "types: " << metadata.types.size() << '\n'
           << "members: " << metadata.members.size() << '\n'
           << "exports: " << metadata.exports.size() << '\n'
           << "abi-fingerprint: " << interop::LibraryMetadataCodec::fingerprintHex(metadata.abiFingerprint) << '\n';
    return ExitCode::Success;
}

ExitCode Driver::verifyLibraryMetadata(std::string_view inputPath, std::ostream &output, std::ostream &error)
{
    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata metadata;
    std::string metadataError;
    if (!codec.read(std::filesystem::path(inputPath), metadata, metadataError))
    {
        error << "nsc: error: invalid NativeSharp library metadata: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }
    if (!codec.verifyCurrentCompiler(metadata, metadataError))
    {
        error << "nsc: error: incompatible NativeSharp library metadata: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }
    output << "NativeSharp library metadata: valid\n"
           << "producer: " << metadata.producerVersion << '\n'
           << "format: NSMETA " << metadata.formatMajor << '.' << metadata.formatMinor << '\n'
           << "module: " << metadata.moduleName << '\n'
           << "target: " << metadata.targetTriple << '\n'
           << "types: " << metadata.types.size() << '\n'
           << "members: " << metadata.members.size() << '\n'
           << "generic-templates: " << metadata.genericTemplates.size() << '\n'
           << "generic-specializations: " << metadata.genericSpecializations.size() << '\n'
           << "dispatch-tables: " << metadata.dispatchTables.size() << '\n'
           << "exports: " << metadata.exports.size() << '\n'
           << "abi-fingerprint: " << interop::LibraryMetadataCodec::fingerprintHex(metadata.abiFingerprint) << '\n';
    printMetadataAbiContract(metadata, output);
    return ExitCode::Success;
}

ExitCode Driver::diffLibraryApi(std::string_view oldMetadataPath, std::string_view newMetadataPath,
                                std::ostream &output, std::ostream &error)
{
    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata oldMetadata;
    interop::LibraryMetadata newMetadata;
    std::string metadataError;
    if (!codec.read(std::filesystem::path(oldMetadataPath), oldMetadata, metadataError) ||
        !codec.verifyCurrentCompiler(oldMetadata, metadataError))
    {
        error << "nsc: error: invalid old NativeSharp library metadata: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }
    if (!codec.read(std::filesystem::path(newMetadataPath), newMetadata, metadataError) ||
        !codec.verifyCurrentCompiler(newMetadata, metadataError))
    {
        error << "nsc: error: invalid new NativeSharp library metadata: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }

    const interop::LibraryApiDiffReport report = interop::LibraryApiDiffer::compare(oldMetadata, newMetadata);
    output << "NativeSharp API diff v1\n"
           << "old-module: " << oldMetadata.moduleName << '\n'
           << "new-module: " << newMetadata.moduleName << '\n'
           << "old-source-fingerprint: " << report.oldSourceFingerprint << '\n'
           << "new-source-fingerprint: " << report.newSourceFingerprint << '\n'
           << "old-abi-surface-fingerprint: " << report.oldAbiSurfaceFingerprint << '\n'
           << "new-abi-surface-fingerprint: " << report.newAbiSurfaceFingerprint << '\n'
           << "old-metadata-abi-fingerprint: "
           << interop::LibraryMetadataCodec::fingerprintHex(oldMetadata.abiFingerprint) << '\n'
           << "new-metadata-abi-fingerprint: "
           << interop::LibraryMetadataCodec::fingerprintHex(newMetadata.abiFingerprint) << '\n'
           << "source-compatible: " << (report.sourceCompatible ? "yes" : "no") << '\n'
           << "abi-compatible: " << (report.abiCompatible ? "yes" : "no") << '\n'
           << "classification: " << interop::LibraryApiDiffer::classification(report) << '\n'
           << "changes: " << report.changes.size() << '\n';
    for (const interop::LibraryApiChange &change : report.changes)
    {
        output << "  [" << interop::LibraryApiDiffer::impactName(change.impact) << "] " << change.domain << ' '
               << change.identity << ": " << change.detail << '\n';
    }
    return report.sourceCompatible && report.abiCompatible ? ExitCode::Success : ExitCode::CompilationError;
}

ExitCode Driver::dumpLibraryApiSurface(std::string_view metadataPath, std::ostream &output, std::ostream &error)
{
    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata metadata;
    std::string metadataError;
    if (!codec.read(std::filesystem::path(metadataPath), metadata, metadataError) ||
        !codec.verifyCurrentCompiler(metadata, metadataError))
    {
        error << "nsc: error: invalid NativeSharp library metadata: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }
    const std::string source = interop::LibraryApiDiffer::sourceSurfaceManifest(metadata);
    const std::string abi = interop::LibraryApiDiffer::abiSurfaceManifest(metadata);
    output << "NativeSharp canonical API surface v2\n"
           << "module: " << metadata.moduleName << '\n'
           << "source-fingerprint: " << interop::LibraryApiDiffer::sourceFingerprint(metadata) << '\n'
           << "abi-fingerprint: " << interop::LibraryApiDiffer::abiSurfaceFingerprint(metadata) << '\n'
           << "source-bytes: " << source.size() << '\n'
           << source << '\n'
           << "abi-bytes: " << abi.size() << '\n'
           << abi << '\n';
    return ExitCode::Success;
}

ExitCode Driver::dumpLibraryAbi(std::string_view metadataPath, std::ostream &output, std::ostream &error)
{
    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata metadata;
    std::string metadataError;
    if (!codec.read(std::filesystem::path(metadataPath), metadata, metadataError) ||
        !codec.verifyCurrentCompiler(metadata, metadataError))
    {
        error << "nsc: error: invalid NativeSharp library metadata: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }
    interop::LibraryAbiReporter::write(metadata, output);
    return ExitCode::Success;
}

ExitCode Driver::diffLibraryAbi(std::string_view oldMetadataPath, std::string_view newMetadataPath,
                                std::ostream &output, std::ostream &error)
{
    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata oldMetadata;
    interop::LibraryMetadata newMetadata;
    std::string metadataError;
    if (!codec.read(std::filesystem::path(oldMetadataPath), oldMetadata, metadataError) ||
        !codec.verifyCurrentCompiler(oldMetadata, metadataError))
    {
        error << "nsc: error: invalid old NativeSharp library metadata: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }
    if (!codec.read(std::filesystem::path(newMetadataPath), newMetadata, metadataError) ||
        !codec.verifyCurrentCompiler(newMetadata, metadataError))
    {
        error << "nsc: error: invalid new NativeSharp library metadata: " << metadataError << '\n';
        return ExitCode::CompilationError;
    }

    const interop::LibraryApiDiffReport report = interop::LibraryApiDiffer::compare(oldMetadata, newMetadata);
    std::vector<const interop::LibraryApiChange *> abiChanges;
    for (const interop::LibraryApiChange &change : report.changes)
        if (change.domain == "abi")
            abiChanges.push_back(&change);
    const std::string classification = abiChanges.empty()     ? "identical"
                                       : report.abiCompatible ? "additive"
                                                              : "abi-breaking";
    output << "NativeSharp ABI diff v1\n"
           << "old-module: " << oldMetadata.moduleName << '\n'
           << "new-module: " << newMetadata.moduleName << '\n'
           << "old-abi-surface-fingerprint: " << report.oldAbiSurfaceFingerprint << '\n'
           << "new-abi-surface-fingerprint: " << report.newAbiSurfaceFingerprint << '\n'
           << "abi-compatible: " << (report.abiCompatible ? "yes" : "no") << '\n'
           << "classification: " << classification << '\n'
           << "changes: " << abiChanges.size() << '\n';
    for (const interop::LibraryApiChange *change : abiChanges)
    {
        output << "  [" << interop::LibraryApiDiffer::impactName(change->impact) << "] " << change->identity << ": "
               << change->detail << '\n';
    }
    return report.abiCompatible ? ExitCode::Success : ExitCode::CompilationError;
}

std::optional<ExitCode> verifyRuntimeArtifact(std::string_view runtimePath, const std::filesystem::path &runtimeFile,
                                              const interop::LibraryMetadata &metadata,
                                              std::span<const std::string> requiredSymbols,
                                              std::size_t &runtimeExportCount, linker::PeExecutableInfo &runtimeInfo,
                                              std::ostream &error)
{
    if (runtimePath.empty())
        return std::nullopt;

    linker::PeExecutableVerifier runtimeVerifier;
    if (!runtimeVerifier.verifyDynamicLibrary(runtimeFile))
    {
        error << "nsc: error: runtime artifact '" << runtimeFile.generic_string()
              << "' is not a valid PE DLL: " << runtimeVerifier.error() << '\n';
        return ExitCode::CompilationError;
    }
    std::vector<std::string> actualExports = runtimeVerifier.info().exportedSymbols;
    std::sort(actualExports.begin(), actualExports.end());
    actualExports.erase(std::unique(actualExports.begin(), actualExports.end()), actualExports.end());
    const std::vector<std::string> runtimeMissing = missingSymbols(requiredSymbols, actualExports);
    const std::vector<std::string> runtimeUnexpected = missingSymbols(actualExports, requiredSymbols);
    if (!runtimeMissing.empty() || !runtimeUnexpected.empty())
    {
        error << "nsc: error: runtime DLL export table does not match metadata module '" << metadata.moduleName << "'";
        if (!runtimeMissing.empty())
            error << "; missing " << runtimeMissing.size() << ": " << describeSymbols(runtimeMissing);
        if (!runtimeUnexpected.empty())
            error << "; unexpected " << runtimeUnexpected.size() << ": " << describeSymbols(runtimeUnexpected);
        error << '\n';
        return ExitCode::CompilationError;
    }
    runtimeExportCount = actualExports.size();
    runtimeInfo = runtimeVerifier.info();
    return std::nullopt;
}

std::optional<ExitCode> verifyPdbArtifact(std::string_view runtimePath, std::string_view pdbPath,
                                          const std::filesystem::path &pdbFile,
                                          const linker::PeExecutableInfo &runtimeInfo,
                                          std::optional<linker::PdbInfo> &debugInfo, std::ostream &error)
{
    if (pdbPath.empty())
        return std::nullopt;
    if (runtimePath.empty())
    {
        error << "nsc: error: a PDB artifact requires its PE DLL artifact\n";
        return ExitCode::CommandLineError;
    }
    if (!runtimeInfo.hasCodeViewRsds)
    {
        error << "nsc: error: runtime DLL has no RSDS CodeView record for PDB verification\n";
        return ExitCode::CompilationError;
    }
    linker::PdbVerifier pdbVerifier;
    if (!pdbVerifier.verify(pdbFile))
    {
        error << "nsc: error: PDB artifact '" << pdbFile.generic_string()
              << "' is not a valid Microsoft PDB: " << pdbVerifier.error() << '\n';
        return ExitCode::CompilationError;
    }
    const linker::PdbInfo &pdb = pdbVerifier.info();
    if (pdb.guid != runtimeInfo.codeViewGuid || pdb.age != runtimeInfo.codeViewAge)
    {
        error << "nsc: error: PDB identity does not match the runtime DLL RSDS record\n";
        return ExitCode::CompilationError;
    }
    if (std::filesystem::path(runtimeInfo.codeViewPdbFileName).filename() != pdbFile.filename() ||
        std::filesystem::path(runtimeInfo.codeViewPdbFileName).has_parent_path())
    {
        error << "nsc: error: runtime DLL RSDS PDB name is not the portable matching basename: '"
              << runtimeInfo.codeViewPdbFileName << "'\n";
        return ExitCode::CompilationError;
    }
    if (!pdb.hasModuleRecords || !pdb.hasPublicSymbols || !pdb.hasSourceFiles || !pdb.hasSourceChecksums)
    {
        error << "nsc: error: PDB debug surface is incomplete; modules=" << pdb.hasModuleRecords
              << ", publics=" << pdb.hasPublicSymbols << ", sources=" << pdb.hasSourceFiles
              << ", checksums=" << pdb.hasSourceChecksums << '\n';
        return ExitCode::CompilationError;
    }
    debugInfo = pdb;
    return std::nullopt;
}

void printVerifiedLibraryArtifacts(const interop::LibraryMetadata &metadata, const std::filesystem::path &metadataFile,
                                   const std::filesystem::path &libraryFile, std::string_view runtimePath,
                                   const std::filesystem::path &runtimeFile, const std::filesystem::path &pdbFile,
                                   const linker::CoffArchiveVerifier &archiveVerifier,
                                   std::span<const std::string> requiredSymbols, std::size_t runtimeExportCount,
                                   const linker::PeExecutableInfo &runtimeInfo,
                                   const std::optional<linker::PdbInfo> &debugInfo, std::ostream &output)
{
    output << "NativeSharp library artifacts: valid\n"
           << "module: " << metadata.moduleName << '\n'
           << "metadata: " << metadataFile.generic_string() << '\n'
           << "library: " << libraryFile.generic_string() << '\n'
           << "archive-members: " << archiveVerifier.info().memberCount << '\n'
           << "archive-symbols: " << archiveVerifier.info().symbolNames.size() << '\n'
           << "required-symbols: " << requiredSymbols.size() << '\n';
    if (runtimePath.empty())
    {
        output << "product-kind: static archive\n";
    }
    else
    {
        output << "product-kind: DLL plus import archive\n"
               << "runtime: " << runtimeFile.generic_string() << '\n'
               << "runtime-exports: " << runtimeExportCount << '\n';
    }
    if (debugInfo.has_value())
    {
        output << "pdb: " << pdbFile.generic_string() << '\n'
               << "pdb-age: " << debugInfo->age << '\n'
               << "pdb-streams: " << debugInfo->streamCount << '\n'
               << "pdb-modules: present\n"
               << "pdb-public-symbols: present\n"
               << "pdb-sources: present\n"
               << "pdb-source-checksums: present\n"
               << "rsds-pdb-name: " << runtimeInfo.codeViewPdbFileName << '\n';
    }
    output << "abi-fingerprint: " << interop::LibraryMetadataCodec::fingerprintHex(metadata.abiFingerprint) << '\n';
    printMetadataAbiContract(metadata, output);
}

ExitCode Driver::verifyLibraryArtifacts(std::string_view metadataPath, std::string_view libraryPath,
                                        std::string_view runtimePath, std::string_view pdbPath, std::ostream &output,
                                        std::ostream &error)
{
    const std::filesystem::path metadataFile(metadataPath);
    const std::filesystem::path libraryFile(libraryPath);
    const std::filesystem::path runtimeFile(runtimePath);
    const std::filesystem::path pdbFile(pdbPath);

    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata metadata;
    std::string detail;
    if (!codec.read(metadataFile, metadata, detail))
    {
        error << "nsc: error: invalid metadata in library artifact set: " << detail << '\n';
        return ExitCode::CompilationError;
    }
    if (!codec.verifyCurrentCompiler(metadata, detail))
    {
        error << "nsc: error: incompatible metadata in library artifact set: " << detail << '\n';
        return ExitCode::CompilationError;
    }
    linker::CoffArchiveVerifier archiveVerifier;
    if (!archiveVerifier.verify(libraryFile))
    {
        error << "nsc: error: library artifact '" << libraryFile.generic_string()
              << "' is not a valid COFF archive: " << archiveVerifier.error() << '\n';
        return ExitCode::CompilationError;
    }

    const std::vector<std::string> requiredSymbols = metadataRequiredSymbols(metadata);
    const std::vector<std::string> archiveMissing = missingSymbols(requiredSymbols, archiveVerifier.info().symbolNames);
    if (!archiveMissing.empty())
    {
        error << "nsc: error: library artifact '" << libraryFile.generic_string() << "' is missing "
              << archiveMissing.size() << " symbol(s) required by metadata module '" << metadata.moduleName
              << "': " << describeSymbols(archiveMissing) << '\n';
        return ExitCode::CompilationError;
    }

    std::size_t runtimeExportCount = 0;
    linker::PeExecutableInfo runtimeInfo;
    if (const std::optional<ExitCode> result = verifyRuntimeArtifact(
            runtimePath, runtimeFile, metadata, requiredSymbols, runtimeExportCount, runtimeInfo, error))
        return *result;

    std::optional<linker::PdbInfo> debugInfo;
    if (const std::optional<ExitCode> result =
            verifyPdbArtifact(runtimePath, pdbPath, pdbFile, runtimeInfo, debugInfo, error))
        return *result;

    // Artifact integrity is independent from source reconstruction. Validate COFF and the
    // optional runtime first so a syntax failure in generated public API does not hide a broken
    // archive/export set (or falsely suggest that those layers were never examined).
    if (!verifyPrecompiledPublicSource(metadata, detail))
    {
        error << "nsc: error: metadata public API cannot be materialized for a consumer: " << detail
              << "\nphysical archive" << (runtimePath.empty() ? "" : " and runtime")
              << " validated before source materialization\n";
        return ExitCode::CompilationError;
    }

    printVerifiedLibraryArtifacts(metadata, metadataFile, libraryFile, runtimePath, runtimeFile, pdbFile,
                                  archiveVerifier, requiredSymbols, runtimeExportCount, runtimeInfo, debugInfo, output);
    return ExitCode::Success;
}
} // namespace nativesharp::driver
