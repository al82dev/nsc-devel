#include "Internal/DriverInternal.h"

#include <memory>

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

bool filesHaveEqualContents(const std::filesystem::path &left, const std::filesystem::path &right);

void appendExternalImport(std::vector<linker::ExternalImportModule> &modules, const std::string &dllName,
                          const std::string &symbol)
{
    auto module = std::ranges::find_if(modules, [&](const auto &candidate)
                                       { return foldedName(candidate.dllName) == foldedName(dllName); });
    if (module == modules.end())
    {
        modules.push_back(linker::ExternalImportModule{dllName, {}});
        module = std::prev(modules.end());
    }
    if (std::ranges::find(module->symbols, symbol) == module->symbols.end())
        module->symbols.push_back(symbol);
}

ExternalImportListBuilder::ExternalImportListBuilder(std::vector<linker::ExternalImportModule> &modules)
    : modules_(modules)
{
    // emplace keeps the first module of a folded name, the one find_if would return.
    for (std::size_t index = 0; index < modules_.size(); ++index)
        moduleByFoldedName_.emplace(foldedName(modules_[index].dllName), index);
}

void ExternalImportListBuilder::append(const std::string &dllName, const std::string &symbol)
{
    const auto [module, inserted] = moduleByFoldedName_.emplace(foldedName(dllName), modules_.size());
    if (inserted)
        modules_.push_back(linker::ExternalImportModule{dllName, {}});
    std::vector<std::string> &symbols = modules_[module->second].symbols;
    if (std::ranges::find(symbols, symbol) == symbols.end())
        symbols.push_back(symbol);
}

bool loadLibraryReferenceMetadata(const compilation::LibraryReference &reference,
                                   const compilation::LibraryMetadataRepository &repository,
                                  interop::LibraryMetadataCodec &metadataCodec,
                                   linker::CoffArchiveVerifier &archiveVerifier,
                                   std::shared_ptr<const interop::LibraryMetadata> &metadata,
                                  std::string &error)
{
    if (!archiveVerifier.verify(reference.libraryPath))
    {
        error = "precompiled library '" + reference.libraryPath.string() +
                "' is not a valid COFF archive: " + archiveVerifier.error();
        return false;
    }
    std::string metadataError;
    std::shared_ptr<const interop::LibraryMetadata> decoded = repository.decode(
        reference.metadataPath,
        [&metadataCodec](std::span<const std::uint8_t> bytes,
                         interop::LibraryMetadata &result,
                         std::string &decodeError)
        {
            return metadataCodec.read(bytes, result, decodeError);
        },
        metadataError);
    if (decoded == nullptr)
    {
        error =
            "cannot read metadata for precompiled library '" + reference.libraryPath.string() + "': " + metadataError;
        return false;
    }
    if (!repository.verify(
            reference.metadataPath,
            [&metadataCodec](
                const interop::LibraryMetadata &candidate,
                std::string &verificationError)
            {
                return metadataCodec.verifyCurrentCompiler(candidate, verificationError);
            },
            metadataError))
    {
        error =
            "incompatible metadata for precompiled library '" + reference.libraryPath.string() + "': " + metadataError;
        return false;
    }
    if (decoded->moduleName.empty())
    {
        error = "precompiled library metadata has an empty module identity: '" + reference.metadataPath.string() + "'";
        return false;
    }
    metadata = std::move(decoded);
    return true;
}

bool validateLibraryReferenceSymbols(const compilation::LibraryReference &reference,
                                     const linker::CoffArchiveVerifier &archiveVerifier,
                                     const interop::LibraryMetadata &metadata,
                                     std::unordered_map<std::string, std::string> &publishedNativeMethods,
                                     std::vector<std::string> &nativeSymbols, std::string &error)
{
    const std::unordered_set<std::string> archiveSymbols(archiveVerifier.info().symbolNames.begin(),
                                                         archiveVerifier.info().symbolNames.end());
    for (const interop::LibraryMetadataExport &exportRecord : metadata.exports)
    {
        if (archiveSymbols.contains(exportRecord.cAbiName))
            continue;
        error = "precompiled library '" + reference.libraryPath.string() + "' does not contain C ABI symbol '" +
                exportRecord.cAbiName + "' published by metadata module '" + metadata.moduleName + "'";
        return false;
    }
    nativeSymbols = publishedNativeSymbols(metadata);
    for (const std::string &symbol : nativeSymbols)
    {
        if (!archiveSymbols.contains(symbol))
        {
            error = "precompiled library '" + reference.libraryPath.string() +
                    "' does not contain NativeSharp symbol '" + symbol + "' published by metadata module '" +
                    metadata.moduleName + "'";
            return false;
        }
        const auto [ownerIterator, inserted] = publishedNativeMethods.emplace(symbol, metadata.moduleName);
        if (!inserted && ownerIterator->second != metadata.moduleName)
        {
            error = "NativeSharp symbol '" + symbol + "' is published by both module '" + ownerIterator->second +
                    "' and '" + metadata.moduleName + "'";
            return false;
        }
    }
    return true;
}

bool validateLibraryReferenceRuntime(const compilation::LibraryReference &reference,
                                     const interop::LibraryMetadata &metadata,
                                     std::span<const std::string> nativeSymbols,
                                     std::unordered_map<std::string, std::filesystem::path> &runtimeNames,
                                     std::unordered_map<std::string, std::filesystem::path> &runtimeByModule,
                                     std::string &error)
{
    if (reference.runtimePath.empty())
        return true;
    linker::PeExecutableVerifier peVerifier;
    if (!peVerifier.verifyDynamicLibrary(reference.runtimePath))
    {
        error = "runtime '" + reference.runtimePath.string() + "' is not a valid DLL: " + peVerifier.error();
        return false;
    }
    std::vector<std::string> expectedExports;
    expectedExports.reserve(metadata.exports.size() + nativeSymbols.size());
    for (const interop::LibraryMetadataExport &exportRecord : metadata.exports)
        expectedExports.push_back(exportRecord.cAbiName);
    expectedExports.insert(expectedExports.end(), nativeSymbols.begin(), nativeSymbols.end());
    std::sort(expectedExports.begin(), expectedExports.end());
    expectedExports.erase(std::unique(expectedExports.begin(), expectedExports.end()), expectedExports.end());
    std::vector<std::string> actualExports = peVerifier.info().exportedSymbols;
    std::sort(actualExports.begin(), actualExports.end());
    actualExports.erase(std::unique(actualExports.begin(), actualExports.end()), actualExports.end());
    if (actualExports != expectedExports)
    {
        const std::vector<std::string> missing = missingSymbols(expectedExports, actualExports);
        const std::vector<std::string> unexpected = missingSymbols(actualExports, expectedExports);
        error = "runtime DLL export table does not match metadata contract for module '" + metadata.moduleName + "'";
        if (!missing.empty())
            error += "; missing " + std::to_string(missing.size()) + ": " + describeSymbols(missing);
        if (!unexpected.empty())
            error += "; unexpected " + std::to_string(unexpected.size()) + ": " + describeSymbols(unexpected);
        return false;
    }
    const std::string runtimeName = foldedName(reference.runtimePath.filename().string());
    const auto runtimeOwner = runtimeNames.find(runtimeName);
    if (runtimeOwner != runtimeNames.end() &&
        foldedPathKey(runtimeOwner->second) != foldedPathKey(reference.runtimePath) &&
        !filesHaveEqualContents(runtimeOwner->second, reference.runtimePath))
    {
        error = "two precompiled library references require different DLLs named '" +
                reference.runtimePath.filename().string() + "'";
        return false;
    }
    if (runtimeOwner == runtimeNames.end())
        runtimeNames.emplace(runtimeName, reference.runtimePath);
    runtimeByModule.emplace(metadata.moduleName, reference.runtimePath);
    return true;
}

bool validateLibraryReference(const compilation::LibraryReference &reference,
                               const compilation::LibraryMetadataRepository &repository,
                              interop::LibraryMetadataCodec &metadataCodec,
                              std::unordered_set<std::string> &linkedLibraries,
                              std::unordered_set<std::string> &validatedReferences,
                              std::unordered_map<std::string, std::filesystem::path> &runtimeNames,
                              std::unordered_map<std::string, std::filesystem::path> &runtimeByModule,
                              std::unordered_map<std::string, std::filesystem::path> &moduleOwners,
                              std::unordered_map<std::string, std::vector<PrecompiledExportContract>> &publishedExports,
                              std::unordered_map<std::string, std::string> &publishedNativeMethods,
                              std::vector<std::filesystem::path> &libraries,
                              std::vector<linker::ExternalImportModule> *inheritedImports, std::string &error)
{
    const std::string libraryKey = foldedPathKey(reference.libraryPath);
    if (validatedReferences.contains(libraryKey))
        return true;
    linker::CoffArchiveVerifier archiveVerifier;
    std::shared_ptr<const interop::LibraryMetadata> metadata;
    if (!loadLibraryReferenceMetadata(
            reference, repository, metadataCodec, archiveVerifier, metadata, error))
        return false;
    if (inheritedImports != nullptr)
        for (const interop::LibraryMetadataPlatformImport &importRecord : metadata->platformImports)
            appendExternalImport(*inheritedImports, importRecord.dllName, importRecord.symbolName);
    std::vector<std::string> nativeSymbols;
    if (!validateLibraryReferenceSymbols(reference, archiveVerifier, *metadata, publishedNativeMethods, nativeSymbols,
                                         error))
        return false;
    const auto owner = moduleOwners.find(metadata->moduleName);
    if (owner != moduleOwners.end() && foldedPathKey(owner->second) != foldedPathKey(reference.metadataPath))
    {
        error = "duplicate module name '" + metadata->moduleName + "' in precompiled library '" +
                reference.libraryPath.string() + "' and '" + owner->second.string() + "'";
        return false;
    }
    if (owner == moduleOwners.end())
        moduleOwners.emplace(metadata->moduleName, reference.libraryPath);
    const std::string runtimeName =
        reference.runtimePath.empty() ? std::string() : foldedName(reference.runtimePath.filename().string());
    for (const interop::LibraryMetadataExport &exportRecord : metadata->exports)
        publishedExports[exportRecord.cAbiName].push_back(
            {metadata->moduleName, runtimeName, exportRecord.returnTypeKey, exportRecord.parameterTypeKeys});
    if (!validateLibraryReferenceRuntime(reference, *metadata, nativeSymbols, runtimeNames, runtimeByModule, error))
        return false;
    validatedReferences.insert(libraryKey);
    if (!linkedLibraries.contains(libraryKey))
    {
        linkedLibraries.insert(libraryKey);
        libraries.push_back(reference.libraryPath);
    }
    return true;
}

bool validateProjectLibraryReferences(
    const frontend::ProjectCompilation &compilation, std::vector<std::filesystem::path> &libraries,
    std::unordered_set<std::string> &linkedLibraries,
    std::unordered_map<std::string, std::filesystem::path> &runtimeNames,
    std::unordered_map<std::string, std::filesystem::path> &runtimeByModule,
    std::unordered_map<std::string, std::filesystem::path> &moduleOwners,
    std::unordered_map<std::string, std::vector<PrecompiledExportContract>> &publishedExports,
    std::unordered_map<std::string, std::string> &publishedNativeMethods,
    std::vector<linker::ExternalImportModule> *inheritedImports, std::string &error)
{
    for (const std::filesystem::path &library : libraries)
        linkedLibraries.insert(foldedPathKey(library));
    for (const compilation::Project &project : compilation.projectGraph()->projects())
        moduleOwners.emplace(project.moduleName(), project.projectFile());
    std::unordered_set<std::string> validatedReferences;
    interop::LibraryMetadataCodec metadataCodec;
    for (const compilation::Project &project : compilation.projectGraph()->projects())
        for (const compilation::LibraryReference &reference : project.libraryReferences())
            if (!validateLibraryReference(reference, compilation.libraryMetadataRepository(), metadataCodec,
                                           linkedLibraries, validatedReferences, runtimeNames,
                                          runtimeByModule, moduleOwners, publishedExports, publishedNativeMethods,
                                          libraries, inheritedImports, error))
                return false;
    return true;
}

void collectReferencedCallees(const frontend::ProjectOopMirNode *node,
                              std::unordered_set<std::string> &referencedCallees,
                              std::unordered_set<std::size_t> &referencedStaticFields)
{
    if (node == nullptr)
        return;
    if (!node->calleeName.empty())
        referencedCallees.insert(node->calleeName);
    if ((node->kind == frontend::ProjectOopMirNodeKind::StaticFieldLoad ||
         node->kind == frontend::ProjectOopMirNodeKind::StaticFieldAddress) &&
        node->memberOrdinal.has_value())
        referencedStaticFields.insert(*node->memberOrdinal);
    for (const auto &child : node->children)
        collectReferencedCallees(child.get(), referencedCallees, referencedStaticFields);
}

void collectProjectRuntimeReferences(const frontend::ProjectOopMirModel &model,
                                     std::unordered_set<std::string> &referencedCallees,
                                     std::unordered_set<std::size_t> &referencedStaticFields)
{
    for (const frontend::ProjectOopMirFunction &function : model.functions())
        collectReferencedCallees(function.body.get(), referencedCallees, referencedStaticFields);
    for (const frontend::ProjectOopMirFieldInitializer &initializer : model.fieldInitializers())
        collectReferencedCallees(initializer.value.get(), referencedCallees, referencedStaticFields);
}

void selectRuntimeForModule(const std::string &moduleName,
                            const std::unordered_map<std::string, std::filesystem::path> &runtimeByModule,
                            std::unordered_set<std::string> &selectedRuntimePaths,
                            std::vector<std::filesystem::path> &runtimeLibraries)
{
    const auto runtime = runtimeByModule.find(moduleName);
    if (runtime == runtimeByModule.end())
        return;
    const std::string key = foldedPathKey(runtime->second);
    if (selectedRuntimePaths.insert(key).second)
        runtimeLibraries.push_back(runtime->second);
}

bool validateImportedFunctionRuntime(
    const frontend::ProjectOopMirFunction &function, const std::unordered_set<std::string> &referencedCallees,
    const std::unordered_map<std::string, std::string> &publishedNativeMethods,
    const std::unordered_map<std::string, std::vector<PrecompiledExportContract>> &publishedExports,
    const std::unordered_map<std::string, std::filesystem::path> &runtimeByModule,
    std::unordered_set<std::string> &selectedRuntimePaths, std::vector<std::filesystem::path> &runtimeLibraries,
    std::string &error)
{
    if (function.isImportedNative)
    {
        if (!referencedCallees.contains(function.mangledName))
            return true;
        const auto owner = publishedNativeMethods.find(function.mangledName);
        if (owner == publishedNativeMethods.end())
        {
            error = "metadata-backed NativeSharp call resolved to unpublished symbol '" + function.mangledName + "'";
            return false;
        }
        selectRuntimeForModule(owner->second, runtimeByModule, selectedRuntimePaths, runtimeLibraries);
        return true;
    }
    if (!function.isImportedC || !referencedCallees.contains(function.mangledName))
        return true;
    const auto found = publishedExports.find(function.cAbiName);
    if (found == publishedExports.end())
        return true;
    const PrecompiledExportContract *contract = nullptr;
    if (!function.importDllName.empty())
    {
        const std::string requestedRuntime =
            foldedName(std::filesystem::path(function.importDllName).filename().string());
        for (const PrecompiledExportContract &candidate : found->second)
        {
            if (candidate.runtimeName != requestedRuntime)
                continue;
            if (contract != nullptr)
            {
                error = "extern C import '" + function.cAbiName + "' is published more than once by runtime DLL '" +
                        function.importDllName + "'";
                return false;
            }
            contract = &candidate;
        }
        // This symbol may belong to a foreign DLL that happens to share its name
        // with an unrelated LibraryReference export. Only metadata for the
        // requested DLL is binding.
        if (contract == nullptr)
            return true;
    }
    else
    {
        if (found->second.size() != 1)
        {
            error = "extern C import '" + function.cAbiName +
                    "' is ambiguous across precompiled library metadata; qualify the DLL import";
            return false;
        }
        contract = &found->second.front();
    }
    if (function.returnTypeKey != contract->returnTypeKey || function.parameterTypeKeys != contract->parameterTypeKeys)
    {
        error = "extern C import '" + function.cAbiName +
                "' does not match the metadata signature published by module '" + contract->moduleName + "'";
        return false;
    }
    // A C export implemented by the NativeSharp producer reaches that producer's runtime.
    // A reconstructed `extern C from "kernel32.dll"` import never enters metadata.exports
    // and therefore does not select the otherwise valid but unused NativeSharp DLL.
    selectRuntimeForModule(contract->moduleName, runtimeByModule, selectedRuntimePaths, runtimeLibraries);
    return true;
}

bool appendReferencedRuntimeLibraries(
    const frontend::ProjectOopMirModel &model,
    const std::unordered_map<std::string, std::string> &publishedNativeMethods,
    const std::unordered_map<std::string, std::vector<PrecompiledExportContract>> &publishedExports,
    const std::unordered_map<std::string, std::filesystem::path> &runtimeByModule,
    std::vector<std::filesystem::path> &runtimeLibraries, std::string &error)
{
    std::unordered_set<std::string> referencedCallees;
    std::unordered_set<std::size_t> referencedStaticFields;
    collectProjectRuntimeReferences(model, referencedCallees, referencedStaticFields);
    std::unordered_set<std::string> selectedRuntimePaths;
    for (const frontend::ProjectOopMirFunction &function : model.functions())
        if (!validateImportedFunctionRuntime(function, referencedCallees, publishedNativeMethods, publishedExports,
                                             runtimeByModule, selectedRuntimePaths, runtimeLibraries, error))
            return false;
    for (const frontend::ProjectOopMirStaticField &field : model.staticFields())
    {
        if (!field.isImported || field.isConst || !referencedStaticFields.contains(field.memberOrdinal))
            continue;
        const auto owner = publishedNativeMethods.find(field.accessorName);
        if (owner == publishedNativeMethods.end())
        {
            error = "metadata-backed NativeSharp static field resolved to unpublished accessor '" + field.accessorName +
                    "'";
            return false;
        }
        selectRuntimeForModule(owner->second, runtimeByModule, selectedRuntimePaths, runtimeLibraries);
    }
    return true;
}

bool appendValidatedLibraryReferences(const frontend::ProjectCompilation &compilation,
                                      std::vector<std::filesystem::path> &libraries,
                                      std::vector<std::filesystem::path> &runtimeLibraries, std::string &error,
                                      std::vector<linker::ExternalImportModule> *inheritedImports)
{
    error.clear();
    if (compilation.projectGraph() == nullptr)
    {
        error = "precompiled library validation requires a project graph";
        return false;
    }

    std::unordered_set<std::string> linkedLibraries;
    std::unordered_map<std::string, std::filesystem::path> runtimeNames;
    std::unordered_map<std::string, std::filesystem::path> runtimeByModule;
    std::unordered_map<std::string, std::filesystem::path> moduleOwners;
    std::unordered_map<std::string, std::vector<PrecompiledExportContract>> publishedExports;
    std::unordered_map<std::string, std::string> publishedNativeMethods;
    if (!validateProjectLibraryReferences(compilation, libraries, linkedLibraries, runtimeNames, runtimeByModule,
                                          moduleOwners, publishedExports, publishedNativeMethods, inheritedImports,
                                          error))
        return false;

    if (compilation.oopMirModel() == nullptr)
        return true;
    return appendReferencedRuntimeLibraries(*compilation.oopMirModel(), publishedNativeMethods, publishedExports,
                                            runtimeByModule, runtimeLibraries, error);
}

bool verifyLinkedRuntimeLibraries(std::span<const std::filesystem::path> runtimeLibraries,
                                  const linker::PeExecutableInfo &image, std::string &error)
{
    std::unordered_set<std::string> reachable;
    for (const std::string &imported : image.importedLibraries)
        reachable.insert(foldedName(imported));

    // An import archive is lazy: a runtime whose types are used only in another DLL's public
    // signature need not contribute a direct EXE import. Follow the declared runtime DLLs so a
    // valid transitive edge (Consumer -> Api.dll -> Types.dll) is certified without weakening
    // the Runtime identity check for libraries that do contribute symbols.
    std::unordered_set<std::string> expanded;
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const std::filesystem::path &runtime : runtimeLibraries)
        {
            const std::string runtimeName = foldedName(runtime.filename().string());
            if (!reachable.contains(runtimeName) || !expanded.insert(runtimeName).second)
                continue;
            linker::PeExecutableVerifier verifier;
            if (!verifier.verifyDynamicLibrary(runtime))
            {
                error = "runtime '" + runtime.string() + "' is not a valid DLL: " + verifier.error();
                return false;
            }
            for (const std::string &imported : verifier.info().importedLibraries)
            {
                changed = reachable.insert(foldedName(imported)).second || changed;
            }
        }
    }

    for (const std::filesystem::path &runtime : runtimeLibraries)
    {
        const std::string expected = foldedName(runtime.filename().string());
        if (!reachable.contains(expected))
        {
            error = "linked PE does not import declared runtime DLL '" + runtime.filename().string() +
                    "' directly or transitively; the import .lib does not match Runtime";
            return false;
        }
    }
    return true;
}
} // namespace nativesharp::driver
