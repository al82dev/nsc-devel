#include "Internal/DriverInternal.h"

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

std::vector<frontend::PrecompiledMemberIdentity> precompiledMemberIdentities(const interop::LibraryMetadata &metadata)
{
    std::vector<frontend::PrecompiledMemberIdentity> result;
    result.reserve(metadata.members.size());
    for (const interop::LibraryMetadataMember &member : metadata.members)
    {
        std::vector<language::EscapeClass> summary;
        summary.reserve(member.parameterEscapeSummary.size());
        for (const interop::LibraryMetadataEscapeClass item : member.parameterEscapeSummary)
            summary.push_back(static_cast<language::EscapeClass>(item));
        result.push_back({static_cast<frontend::ClassMemberKind>(member.kind), member.declaringTypeKey, member.name,
                          member.signatureKey, member.mangledName, std::move(summary)});
    }
    return result;
}

std::vector<frontend::PrecompiledNamespaceFunctionIdentity> precompiledNamespaceIdentities(
    const interop::LibraryMetadata &metadata)
{
    std::vector<frontend::PrecompiledNamespaceFunctionIdentity> result;
    result.reserve(metadata.namespaceFunctions.size());
    for (const interop::LibraryMetadataNamespaceFunction &function : metadata.namespaceFunctions)
    {
        std::vector<language::EscapeClass> summary;
        summary.reserve(function.parameterEscapeSummary.size());
        for (const interop::LibraryMetadataEscapeClass item : function.parameterEscapeSummary)
            summary.push_back(static_cast<language::EscapeClass>(item));
        result.push_back(
            {metadata.moduleName, function.namespaceName, function.name, function.signatureKey, std::move(summary)});
    }
    return result;
}

std::vector<frontend::PrecompiledGenericSpecialization> precompiledGenericSpecializations(
    const interop::LibraryMetadata &metadata, const std::unordered_set<std::string_view> &concreteMemberNames)
{
    std::vector<frontend::PrecompiledGenericSpecialization> result;
    result.reserve(metadata.genericSpecializations.size());
    for (const interop::LibraryMetadataGenericSpecialization &specialization : metadata.genericSpecializations)
    {
        bool hasConcreteDeclaration = false;
        constexpr std::uint8_t kGenericMethodKind = 3U;
        if (specialization.kind == kGenericMethodKind)
            hasConcreteDeclaration = concreteMemberNames.contains(specialization.specializedName);
        else if (const std::optional<std::string> typeKey = metadataSpecializedTypeKey(specialization);
                 typeKey.has_value())
            hasConcreteDeclaration = metadataTypeForKey(metadata, *typeKey) != nullptr;
        result.push_back({static_cast<frontend::GenericTemplateKind>(specialization.kind), specialization.templateKey,
                          specialization.specializationKey, specialization.specializedName,
                          specialization.typeArguments, hasConcreteDeclaration});
    }
    return result;
}

bool materializePrecompiledModules(std::vector<LoadedPrecompiledMetadata> &loaded,
                                   std::vector<frontend::PrecompiledSyntaxModule> &modules, std::string &error)
{
    modules.reserve(loaded.size());
    for (LoadedPrecompiledMetadata &entry : loaded)
    {
        interop::LibraryMetadata &metadata = entry.metadata;
        std::unordered_set<std::string_view> concreteMemberNames;
        concreteMemberNames.reserve(metadata.members.size());
        for (const interop::LibraryMetadataMember &member : metadata.members)
            concreteMemberNames.insert(member.name);
        std::string publicSource;
        if (!buildPrecompiledPublicSource(metadata, publicSource, error))
        {
            error = "cannot materialize public API for module '" + metadata.moduleName + "': " + error;
            return false;
        }
        std::string templateSource;
        if (!buildPrecompiledGenericTemplateSource(metadata, templateSource, error))
        {
            error = "cannot materialize public templates for module '" + metadata.moduleName + "': " + error;
            return false;
        }
        modules.push_back({entry.ownerProjectFile, entry.metadataPath, metadata.moduleName, std::move(publicSource),
                           std::move(templateSource), precompiledMemberIdentities(metadata),
                           precompiledNamespaceIdentities(metadata),
                           precompiledGenericSpecializations(metadata, concreteMemberNames)});
    }
    return true;
}

bool loadExplicitPrecompiledMetadata(const compilation::ProjectGraph &graph, interop::LibraryMetadataCodec &codec,
                                     std::vector<LoadedPrecompiledMetadata> &loaded,
                                     std::unordered_map<std::string, std::size_t> &loadedByModule,
                                     std::unordered_map<std::string, std::filesystem::path> &sourceProjects,
                                     std::vector<compilation::SourceLibraryReferenceReplacement> &sourceReplacements,
                                     std::string &error)
{
    for (const compilation::Project &project : graph.projects())
        sourceProjects.emplace(project.moduleName(), project.projectFile());
    for (const compilation::Project &project : graph.projects())
    {
        for (const compilation::LibraryReference &reference : project.libraryReferences())
        {
            interop::LibraryMetadata metadata;
            if (!codec.read(reference.metadataPath, metadata, error) || !codec.verifyCurrentCompiler(metadata, error))
            {
                error =
                    "cannot import precompiled metadata '" + reference.metadataPath.generic_string() + "': " + error;
                return false;
            }
            const auto sourceProject = sourceProjects.find(metadata.moduleName);
            if (sourceProject != sourceProjects.end())
            {
                sourceReplacements.push_back({project.projectFile(), reference.metadataPath, sourceProject->second});
                continue;
            }
            const auto existing = loadedByModule.find(metadata.moduleName);
            if (existing != loadedByModule.end())
            {
                const LoadedPrecompiledMetadata &prior = loaded[existing->second];
                if (prior.metadata.abiFingerprint != metadata.abiFingerprint ||
                    foldedPathKey(prior.metadataPath) != foldedPathKey(reference.metadataPath))
                {
                    error = "precompiled module '" + metadata.moduleName +
                            "' was explicitly referenced with incompatible artifacts '" +
                            prior.metadataPath.generic_string() + "' and '" + reference.metadataPath.generic_string() +
                            "'";
                    return false;
                }
                continue;
            }
            loadedByModule.emplace(metadata.moduleName, loaded.size());
            loaded.push_back({project.projectFile(), reference.metadataPath, std::move(metadata)});
        }
    }
    return true;
}

bool resolveBinaryLibraryReference(const interop::LibraryMetadataBinaryDependency &dependency,
                                   const std::filesystem::path &parentMetadataPath, const std::string &parentModule,
                                   const compilation::ProjectLoadOptions &loadOptions,
                                   compilation::LibraryReference &resolved, std::string &error)
{
    std::vector<std::filesystem::path> roots = {parentMetadataPath.parent_path()};
    if (loadOptions.libraryReferenceRoot.has_value() &&
        foldedPathKey(*loadOptions.libraryReferenceRoot) != foldedPathKey(roots.front()))
        roots.push_back(*loadOptions.libraryReferenceRoot);
    if (loadOptions.libraryReferenceFallbackRoot.has_value() &&
        std::ranges::none_of(
            roots, [&](const std::filesystem::path &root)
            { return foldedPathKey(root) == foldedPathKey(*loadOptions.libraryReferenceFallbackRoot); }))
        roots.push_back(*loadOptions.libraryReferenceFallbackRoot);
    for (const std::filesystem::path &root : roots)
    {
        compilation::LibraryReference candidate{root / dependency.libraryFileName, root / dependency.metadataFileName,
                                                dependency.runtimeFileName.empty() ? std::filesystem::path{}
                                                                                   : root / dependency.runtimeFileName};
        std::error_code fileError;
        const bool hasLibrary = std::filesystem::is_regular_file(candidate.libraryPath, fileError);
        fileError.clear();
        const bool hasMetadata = std::filesystem::is_regular_file(candidate.metadataPath, fileError);
        fileError.clear();
        const bool hasRuntime =
            candidate.runtimePath.empty() || std::filesystem::is_regular_file(candidate.runtimePath, fileError);
        if (!hasLibrary || !hasMetadata || !hasRuntime)
            continue;
        resolved = std::move(candidate);
        return true;
    }
    error = "binary closure for module '" + parentModule + "' cannot resolve dependency '" + dependency.moduleName +
            "' (expected " + dependency.libraryFileName + " + " + dependency.metadataFileName +
            ") beside metadata or under LibraryReferenceRoot";
    return false;
}

bool loadBinaryDependencyMetadata(const interop::LibraryMetadataBinaryDependency &dependency,
                                  const std::string &parentModule, const compilation::LibraryReference &resolved,
                                  interop::LibraryMetadataCodec &codec, interop::LibraryMetadata &childMetadata,
                                  std::string &error)
{
    if (!codec.read(resolved.metadataPath, childMetadata, error) || !codec.verifyCurrentCompiler(childMetadata, error))
    {
        error = "cannot import transitive metadata '" + resolved.metadataPath.generic_string() +
                "' required by module '" + parentModule + "': " + error;
        return false;
    }
    if (childMetadata.moduleName != dependency.moduleName)
    {
        error = "binary closure expected module '" + dependency.moduleName + "' but metadata '" +
                resolved.metadataPath.generic_string() + "' identifies '" + childMetadata.moduleName + "'";
        return false;
    }
    if (childMetadata.abiFingerprint != dependency.abiFingerprint)
    {
        error = "binary closure ABI fingerprint mismatch for module '" + dependency.moduleName + "' required by '" +
                parentModule + "'";
        return false;
    }
    return true;
}

bool expandPrecompiledBinaryClosure(
    const compilation::ProjectLoadOptions &loadOptions, interop::LibraryMetadataCodec &codec,
    std::vector<LoadedPrecompiledMetadata> &loaded, std::unordered_map<std::string, std::size_t> &loadedByModule,
    const std::unordered_map<std::string, std::filesystem::path> &sourceProjects,
    std::vector<compilation::SourceLibraryReferenceReplacement> &sourceReplacements,
    std::unordered_map<std::string, std::vector<compilation::LibraryReference>> &discoveredByOwner, std::string &error)
{
    // NSMETA 1.25 expands the static binary closure without restore or source enumeration. Each
    // edge is resolved either beside its referring metadata (relocatable package layout) or below
    // the explicit LibraryReferenceRoot. The recorded ABI fingerprint prevents same-name artifact
    // substitution before a linker is invoked.
    for (std::size_t cursor = 0; cursor < loaded.size(); ++cursor)
    {
        const std::filesystem::path parentOwner = loaded[cursor].ownerProjectFile;
        const std::filesystem::path parentMetadataPath = loaded[cursor].metadataPath;
        const std::string parentModule = loaded[cursor].metadata.moduleName;
        const std::vector<interop::LibraryMetadataBinaryDependency> parentDependencies =
            loaded[cursor].metadata.binaryDependencies;
        for (const interop::LibraryMetadataBinaryDependency &dependency : parentDependencies)
        {
            // A source module already in this graph supplies declarations and object code; do
            // not restore or materialize its precompiled counterpart from the binary closure.
            const auto sourceProject = sourceProjects.find(dependency.moduleName);
            if (sourceProject != sourceProjects.end())
            {
                sourceReplacements.push_back({parentOwner, std::filesystem::path{}, sourceProject->second});
                continue;
            }
            // A dependency may already be an explicit LibraryReference of this graph and may
            // legitimately live in a sibling package directory. Module identity plus the frozen
            // ABI fingerprint is the closure contract; do not require a second physical copy
            // beside the parent metadata or under the global package root.
            const auto loadedDependency = loadedByModule.find(dependency.moduleName);
            if (loadedDependency != loadedByModule.end())
            {
                const LoadedPrecompiledMetadata &prior = loaded[loadedDependency->second];
                if (prior.metadata.abiFingerprint != dependency.abiFingerprint)
                {
                    error = "binary closure ABI fingerprint mismatch for explicitly loaded module '" +
                            dependency.moduleName + "' required by '" + parentModule + "'";
                    return false;
                }
                continue;
            }
            compilation::LibraryReference resolved;
            if (!resolveBinaryLibraryReference(dependency, parentMetadataPath, parentModule, loadOptions, resolved,
                                               error))
                return false;
            interop::LibraryMetadata childMetadata;
            if (!loadBinaryDependencyMetadata(dependency, parentModule, resolved, codec, childMetadata, error))
                return false;
            const auto existing = loadedByModule.find(childMetadata.moduleName);
            if (existing != loadedByModule.end() &&
                loaded[existing->second].metadata.abiFingerprint != childMetadata.abiFingerprint)
            {
                error = "binary closure selects incompatible versions of module '" + childMetadata.moduleName + "'";
                return false;
            }
            if (existing != loadedByModule.end())
                continue;
            loadedByModule.emplace(childMetadata.moduleName, loaded.size());
            loaded.push_back({parentOwner, resolved.metadataPath, std::move(childMetadata)});
            discoveredByOwner[foldedPathKey(parentOwner)].push_back(std::move(resolved));
        }
    }
    return true;
}

bool applyPrecompiledGraphClosure(
    compilation::ProjectGraph &graph,
    const std::unordered_map<std::string, std::vector<compilation::LibraryReference>> &discoveredByOwner,
    const std::vector<compilation::SourceLibraryReferenceReplacement> &sourceReplacements,
    const std::vector<LoadedPrecompiledMetadata> &loaded,
    const std::unordered_map<std::string, std::filesystem::path> &sourceProjects, std::string &error)
{
    std::vector<std::filesystem::path> closureOwners;
    closureOwners.reserve(discoveredByOwner.size());
    for (const compilation::Project &project : graph.projects())
        if (discoveredByOwner.contains(foldedPathKey(project.projectFile())))
            closureOwners.push_back(project.projectFile());
    for (const std::filesystem::path &owner : closureOwners)
    {
        const auto additions = discoveredByOwner.find(foldedPathKey(owner));
        if (!graph.addLibraryReferences(owner, additions->second, error))
            return false;
    }
    // ProjectReference wins over a binary edge to the same canonical module. Rewiring the
    // effective graph preserves dependency visibility and prevents a second definition at link.
    if (!graph.replaceLibraryReferencesWithSourceProjects(sourceReplacements, error))
        return false;
    return validatePrecompiledExternalTypeClosure(loaded, sourceProjects, error);
}

bool configurePrecompiledModules(const std::filesystem::path &projectFile, frontend::ProjectCompilation &compilation,
                                 std::string &error)
{
    const CompilationJobPolicy *const jobs = activeCompilationJobPolicy();
    compilation::ProjectLoader &projectLoader = activeProjectLoader();
    const compilation::ProjectLoadOptions loadOptions = activeProjectLoadOptions();
    compilation::ProjectGraphLoader loader(
        jobs != nullptr ? jobs->effectiveJobs() : 1,
        [&projectLoader, loadOptions](const std::filesystem::path &path, std::string &loadError)
        { return projectLoader.load(path, loadOptions, loadError); });
    std::optional<compilation::ProjectGraph> graph =
        measureDriverPhase("project-graph", [&] { return loader.load(projectFile, error); });
    if (!graph.has_value())
        return false;

    interop::LibraryMetadataCodec codec;
    std::vector<LoadedPrecompiledMetadata> loaded;
    std::unordered_map<std::string, std::size_t> loadedByModule;
    std::unordered_map<std::string, std::filesystem::path> sourceProjects;
    std::vector<compilation::SourceLibraryReferenceReplacement> sourceReplacements;
    if (!loadExplicitPrecompiledMetadata(*graph, codec, loaded, loadedByModule, sourceProjects, sourceReplacements,
                                         error))
        return false;

    std::unordered_map<std::string, std::vector<compilation::LibraryReference>> discoveredByOwner;
    if (!expandPrecompiledBinaryClosure(loadOptions, codec, loaded, loadedByModule, sourceProjects, sourceReplacements,
                                        discoveredByOwner, error) ||
        !applyPrecompiledGraphClosure(*graph, discoveredByOwner, sourceReplacements, loaded, sourceProjects, error))
        return false;

    std::vector<frontend::PrecompiledSyntaxModule> modules;
    if (!materializePrecompiledModules(loaded, modules, error))
        return false;
    if (!compilation.setPrecompiledModules(std::move(modules), error))
        return false;
    return compilation.setPreloadedProjectGraph(std::move(*graph), error);
}

void configureObjectGeneratorProgress(llvm_backend::ProjectOopObjectGenerator &generator)
{
    if (activeExecutionScope == nullptr || !activeExecutionScope->reportProgress)
        return;
    DriverExecutionScope *const scope = activeExecutionScope;
    generator.setProgressSink([scope](const basic::CompilationEvent &event) { scope->writeProgress(event); });
}
} // namespace nativesharp::driver
