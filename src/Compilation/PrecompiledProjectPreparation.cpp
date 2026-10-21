#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"

#include "../Driver/Internal/DriverInternal.h"

namespace nativesharp::compilation
{
namespace
{
using LoadedMetadata = driver::LoadedPrecompiledMetadata;

std::vector<frontend::PrecompiledMemberIdentity> memberIdentities(
    const interop::LibraryMetadata& metadata)
{
    std::vector<frontend::PrecompiledMemberIdentity> result;
    result.reserve(metadata.members.size());
    for (const interop::LibraryMetadataMember& member : metadata.members)
    {
        std::vector<language::EscapeClass> summary;
        summary.reserve(member.parameterEscapeSummary.size());
        for (const interop::LibraryMetadataEscapeClass item : member.parameterEscapeSummary)
        {
            summary.push_back(static_cast<language::EscapeClass>(item));
        }
        result.push_back({
            static_cast<frontend::ClassMemberKind>(member.kind),
            member.declaringTypeKey,
            member.name,
            member.signatureKey,
            member.mangledName,
            std::move(summary)});
    }
    return result;
}

std::vector<frontend::PrecompiledNamespaceFunctionIdentity> namespaceIdentities(
    const interop::LibraryMetadata& metadata)
{
    std::vector<frontend::PrecompiledNamespaceFunctionIdentity> result;
    result.reserve(metadata.namespaceFunctions.size());
    for (const interop::LibraryMetadataNamespaceFunction& function :
         metadata.namespaceFunctions)
    {
        std::vector<language::EscapeClass> summary;
        summary.reserve(function.parameterEscapeSummary.size());
        for (const interop::LibraryMetadataEscapeClass item :
             function.parameterEscapeSummary)
        {
            summary.push_back(static_cast<language::EscapeClass>(item));
        }
        result.push_back({
            metadata.moduleName,
            function.namespaceName,
            function.name,
            function.signatureKey,
            std::move(summary)});
    }
    return result;
}

std::vector<frontend::PrecompiledGenericSpecialization> genericSpecializations(
    const interop::LibraryMetadata& metadata,
    const std::unordered_set<std::string_view>& concreteMemberNames)
{
    std::vector<frontend::PrecompiledGenericSpecialization> result;
    result.reserve(metadata.genericSpecializations.size());
    for (const interop::LibraryMetadataGenericSpecialization& specialization :
         metadata.genericSpecializations)
    {
        bool hasConcreteDeclaration = false;
        constexpr std::uint8_t kGenericMethodKind = 3U;
        if (specialization.kind == kGenericMethodKind)
        {
            hasConcreteDeclaration =
                concreteMemberNames.contains(specialization.specializedName);
        }
        else if (const std::optional<std::string> typeKey =
                     driver::metadataSpecializedTypeKey(specialization);
                 typeKey.has_value())
        {
            hasConcreteDeclaration =
                driver::metadataTypeForKey(metadata, *typeKey) != nullptr;
        }
        result.push_back({
            static_cast<frontend::GenericTemplateKind>(specialization.kind),
            specialization.templateKey,
            specialization.specializationKey,
            specialization.specializedName,
            specialization.typeArguments,
            hasConcreteDeclaration});
    }
    return result;
}

bool materializeModules(
    std::vector<LoadedMetadata>& loaded,
    std::vector<frontend::PrecompiledSyntaxModule>& modules,
    std::string& error)
{
    modules.reserve(loaded.size());
    for (LoadedMetadata& entry : loaded)
    {
        interop::LibraryMetadata& metadata = entry.metadata;
        std::unordered_set<std::string_view> concreteMemberNames;
        concreteMemberNames.reserve(metadata.members.size());
        for (const interop::LibraryMetadataMember& member : metadata.members)
        {
            concreteMemberNames.insert(member.name);
        }
        std::string publicSource;
        if (!driver::buildPrecompiledPublicSource(metadata, publicSource, error))
        {
            error = "cannot materialize public API for module '" +
                metadata.moduleName + "': " + error;
            return false;
        }
        std::string templateSource;
        if (!driver::buildPrecompiledGenericTemplateSource(
                metadata, templateSource, error))
        {
            error = "cannot materialize public templates for module '" +
                metadata.moduleName + "': " + error;
            return false;
        }
        modules.push_back({
            entry.ownerProjectFile,
            entry.metadataPath,
            metadata.moduleName,
            std::move(publicSource),
            std::move(templateSource),
            memberIdentities(metadata),
            namespaceIdentities(metadata),
            genericSpecializations(metadata, concreteMemberNames)});
    }
    return true;
}

bool loadExplicitMetadata(
    const ProjectGraph& graph,
    interop::LibraryMetadataCodec& codec,
    std::vector<LoadedMetadata>& loaded,
    std::unordered_map<std::string, std::size_t>& loadedByModule,
    std::unordered_map<std::string, std::filesystem::path>& sourceProjects,
    std::vector<SourceLibraryReferenceReplacement>& sourceReplacements,
    std::string& error)
{
    for (const Project& project : graph.projects())
    {
        sourceProjects.emplace(project.moduleName(), project.projectFile());
    }
    for (const Project& project : graph.projects())
    {
        for (const LibraryReference& reference : project.libraryReferences())
        {
            interop::LibraryMetadata metadata;
            if (!codec.read(reference.metadataPath, metadata, error) ||
                !codec.verifyCurrentCompiler(metadata, error))
            {
                error = "cannot import precompiled metadata '" +
                    reference.metadataPath.generic_string() + "': " + error;
                return false;
            }
            const auto sourceProject = sourceProjects.find(metadata.moduleName);
            if (sourceProject != sourceProjects.end())
            {
                sourceReplacements.push_back({
                    project.projectFile(),
                    reference.metadataPath,
                    sourceProject->second});
                continue;
            }
            const auto existing = loadedByModule.find(metadata.moduleName);
            if (existing != loadedByModule.end())
            {
                const LoadedMetadata& prior = loaded[existing->second];
                if (prior.metadata.abiFingerprint != metadata.abiFingerprint ||
                    driver::foldedPathKey(prior.metadataPath) !=
                        driver::foldedPathKey(reference.metadataPath))
                {
                    error = "precompiled module '" + metadata.moduleName +
                        "' was explicitly referenced with incompatible artifacts '" +
                        prior.metadataPath.generic_string() + "' and '" +
                        reference.metadataPath.generic_string() + "'";
                    return false;
                }
                continue;
            }
            loadedByModule.emplace(metadata.moduleName, loaded.size());
            loaded.push_back({
                project.projectFile(),
                reference.metadataPath,
                std::move(metadata)});
        }
    }
    return true;
}

bool resolveBinaryReference(
    const interop::LibraryMetadataBinaryDependency& dependency,
    const std::filesystem::path& parentMetadataPath,
    const std::string& parentModule,
    const ProjectLoadOptions& loadOptions,
    LibraryReference& resolved,
    std::string& error)
{
    std::vector<std::filesystem::path> roots = {
        parentMetadataPath.parent_path()};
    if (loadOptions.libraryReferenceRoot.has_value() &&
        driver::foldedPathKey(*loadOptions.libraryReferenceRoot) !=
            driver::foldedPathKey(roots.front()))
    {
        roots.push_back(*loadOptions.libraryReferenceRoot);
    }
    if (loadOptions.libraryReferenceFallbackRoot.has_value() &&
        std::ranges::none_of(
            roots,
            [&](const std::filesystem::path& root)
            {
                return driver::foldedPathKey(root) ==
                    driver::foldedPathKey(
                        *loadOptions.libraryReferenceFallbackRoot);
            }))
    {
        roots.push_back(*loadOptions.libraryReferenceFallbackRoot);
    }
    for (const std::filesystem::path& root : roots)
    {
        LibraryReference candidate{
            root / dependency.libraryFileName,
            root / dependency.metadataFileName,
            dependency.runtimeFileName.empty()
                ? std::filesystem::path{}
                : root / dependency.runtimeFileName};
        std::error_code fileError;
        const bool hasLibrary =
            std::filesystem::is_regular_file(candidate.libraryPath, fileError);
        fileError.clear();
        const bool hasMetadata =
            std::filesystem::is_regular_file(candidate.metadataPath, fileError);
        fileError.clear();
        const bool hasRuntime = candidate.runtimePath.empty() ||
            std::filesystem::is_regular_file(candidate.runtimePath, fileError);
        if (!hasLibrary || !hasMetadata || !hasRuntime)
        {
            continue;
        }
        resolved = std::move(candidate);
        return true;
    }
    error = "binary closure for module '" + parentModule +
        "' cannot resolve dependency '" + dependency.moduleName +
        "' (expected " + dependency.libraryFileName + " + " +
        dependency.metadataFileName +
        ") beside metadata or under LibraryReferenceRoot";
    return false;
}

bool loadBinaryDependency(
    const interop::LibraryMetadataBinaryDependency& dependency,
    const std::string& parentModule,
    const LibraryReference& resolved,
    interop::LibraryMetadataCodec& codec,
    interop::LibraryMetadata& childMetadata,
    std::string& error)
{
    if (!codec.read(resolved.metadataPath, childMetadata, error) ||
        !codec.verifyCurrentCompiler(childMetadata, error))
    {
        error = "cannot import transitive metadata '" +
            resolved.metadataPath.generic_string() + "' required by module '" +
            parentModule + "': " + error;
        return false;
    }
    if (childMetadata.moduleName != dependency.moduleName)
    {
        error = "binary closure expected module '" + dependency.moduleName +
            "' but metadata '" + resolved.metadataPath.generic_string() +
            "' identifies '" + childMetadata.moduleName + "'";
        return false;
    }
    if (childMetadata.abiFingerprint != dependency.abiFingerprint)
    {
        error = "binary closure ABI fingerprint mismatch for module '" +
            dependency.moduleName + "' required by '" + parentModule + "'";
        return false;
    }
    return true;
}

bool reuseAvailableBinaryDependency(
    const interop::LibraryMetadataBinaryDependency& dependency,
    const std::filesystem::path& parentOwner,
    const std::string& parentModule,
    const std::vector<LoadedMetadata>& loaded,
    const std::unordered_map<std::string, std::size_t>& loadedByModule,
    const std::unordered_map<std::string, std::filesystem::path>& sourceProjects,
    std::vector<SourceLibraryReferenceReplacement>& sourceReplacements,
    bool& handled,
    std::string& error)
{
    handled = false;
    const auto sourceProject = sourceProjects.find(dependency.moduleName);
    if (sourceProject != sourceProjects.end())
    {
        sourceReplacements.push_back({
            parentOwner,
            std::filesystem::path{},
            sourceProject->second});
        handled = true;
        return true;
    }
    const auto loadedDependency = loadedByModule.find(dependency.moduleName);
    if (loadedDependency == loadedByModule.end()) return true;
    handled = true;
    const LoadedMetadata& prior = loaded[loadedDependency->second];
    if (prior.metadata.abiFingerprint == dependency.abiFingerprint) return true;
    error = "binary closure ABI fingerprint mismatch for explicitly "
        "loaded module '" + dependency.moduleName +
        "' required by '" + parentModule + "'";
    return false;
}

bool appendResolvedBinaryDependency(
    const std::filesystem::path& parentOwner,
    LibraryReference resolved,
    interop::LibraryMetadata childMetadata,
    std::vector<LoadedMetadata>& loaded,
    std::unordered_map<std::string, std::size_t>& loadedByModule,
    std::unordered_map<std::string, std::vector<LibraryReference>>& discoveredByOwner,
    std::string& error)
{
    const auto existing = loadedByModule.find(childMetadata.moduleName);
    if (existing != loadedByModule.end() &&
        loaded[existing->second].metadata.abiFingerprint !=
            childMetadata.abiFingerprint)
    {
        error = "binary closure selects incompatible versions of module '" +
            childMetadata.moduleName + "'";
        return false;
    }
    if (existing != loadedByModule.end()) return true;
    loadedByModule.emplace(childMetadata.moduleName, loaded.size());
    loaded.push_back({
        parentOwner,
        resolved.metadataPath,
        std::move(childMetadata)});
    discoveredByOwner[driver::foldedPathKey(parentOwner)].push_back(
        std::move(resolved));
    return true;
}

bool expandBinaryDependency(
    const ProjectLoadOptions& loadOptions,
    const interop::LibraryMetadataBinaryDependency& dependency,
    const std::filesystem::path& parentOwner,
    const std::filesystem::path& parentMetadataPath,
    const std::string& parentModule,
    interop::LibraryMetadataCodec& codec,
    std::vector<LoadedMetadata>& loaded,
    std::unordered_map<std::string, std::size_t>& loadedByModule,
    const std::unordered_map<std::string, std::filesystem::path>& sourceProjects,
    std::vector<SourceLibraryReferenceReplacement>& sourceReplacements,
    std::unordered_map<std::string, std::vector<LibraryReference>>&
        discoveredByOwner,
    std::string& error)
{
    bool handled = false;
    if (!reuseAvailableBinaryDependency(
            dependency,
            parentOwner,
            parentModule,
            loaded,
            loadedByModule,
            sourceProjects,
            sourceReplacements,
            handled,
            error))
    {
        return false;
    }
    if (handled) return true;
    LibraryReference resolved;
    if (!resolveBinaryReference(
            dependency,
            parentMetadataPath,
            parentModule,
            loadOptions,
            resolved,
            error))
    {
        return false;
    }
    interop::LibraryMetadata childMetadata;
    if (!loadBinaryDependency(
            dependency,
            parentModule,
            resolved,
            codec,
            childMetadata,
            error))
    {
        return false;
    }
    return appendResolvedBinaryDependency(
        parentOwner,
        std::move(resolved),
        std::move(childMetadata),
        loaded,
        loadedByModule,
        discoveredByOwner,
        error);
}

bool expandBinaryClosure(
    const ProjectLoadOptions& loadOptions,
    interop::LibraryMetadataCodec& codec,
    std::vector<LoadedMetadata>& loaded,
    std::unordered_map<std::string, std::size_t>& loadedByModule,
    const std::unordered_map<std::string, std::filesystem::path>& sourceProjects,
    std::vector<SourceLibraryReferenceReplacement>& sourceReplacements,
    std::unordered_map<std::string, std::vector<LibraryReference>>&
        discoveredByOwner,
    std::string& error)
{
    for (std::size_t cursor = 0; cursor < loaded.size(); ++cursor)
    {
        const std::filesystem::path parentOwner = loaded[cursor].ownerProjectFile;
        const std::filesystem::path parentMetadataPath = loaded[cursor].metadataPath;
        const std::string parentModule = loaded[cursor].metadata.moduleName;
        const std::vector<interop::LibraryMetadataBinaryDependency> dependencies =
            loaded[cursor].metadata.binaryDependencies;
        for (const interop::LibraryMetadataBinaryDependency& dependency : dependencies)
        {
            if (!expandBinaryDependency(
                    loadOptions,
                    dependency,
                    parentOwner,
                    parentMetadataPath,
                    parentModule,
                    codec,
                    loaded,
                    loadedByModule,
                    sourceProjects,
                    sourceReplacements,
                    discoveredByOwner,
                    error)) return false;
        }
    }
    return true;
}

bool applyGraphClosure(
    ProjectGraph& graph,
    const std::unordered_map<std::string, std::vector<LibraryReference>>&
        discoveredByOwner,
    const std::vector<SourceLibraryReferenceReplacement>& sourceReplacements,
    const std::vector<LoadedMetadata>& loaded,
    const std::unordered_map<std::string, std::filesystem::path>& sourceProjects,
    std::string& error)
{
    std::vector<std::filesystem::path> closureOwners;
    closureOwners.reserve(discoveredByOwner.size());
    for (const Project& project : graph.projects())
    {
        if (discoveredByOwner.contains(
                driver::foldedPathKey(project.projectFile())))
        {
            closureOwners.push_back(project.projectFile());
        }
    }
    for (const std::filesystem::path& owner : closureOwners)
    {
        const auto additions = discoveredByOwner.find(driver::foldedPathKey(owner));
        if (!graph.addLibraryReferences(owner, additions->second, error))
        {
            return false;
        }
    }
    if (!graph.replaceLibraryReferencesWithSourceProjects(
            sourceReplacements, error))
    {
        return false;
    }
    return driver::validatePrecompiledExternalTypeClosure(
        loaded, sourceProjects, error);
}
}

bool preparePrecompiledProject(
    ProjectGraph graph,
    const ProjectLoadOptions& loadOptions,
    frontend::ProjectCompilation& compilation,
    std::string& error)
{
    interop::LibraryMetadataCodec codec;
    std::vector<LoadedMetadata> loaded;
    std::unordered_map<std::string, std::size_t> loadedByModule;
    std::unordered_map<std::string, std::filesystem::path> sourceProjects;
    std::vector<SourceLibraryReferenceReplacement> sourceReplacements;
    if (!loadExplicitMetadata(
            graph,
            codec,
            loaded,
            loadedByModule,
            sourceProjects,
            sourceReplacements,
            error))
    {
        return false;
    }

    std::unordered_map<std::string, std::vector<LibraryReference>>
        discoveredByOwner;
    if (!expandBinaryClosure(
            loadOptions,
            codec,
            loaded,
            loadedByModule,
            sourceProjects,
            sourceReplacements,
            discoveredByOwner,
            error) ||
        !applyGraphClosure(
            graph,
            discoveredByOwner,
            sourceReplacements,
            loaded,
            sourceProjects,
            error))
    {
        return false;
    }

    std::vector<frontend::PrecompiledSyntaxModule> modules;
    if (!materializeModules(loaded, modules, error) ||
        !compilation.setPrecompiledModules(std::move(modules), error))
    {
        return false;
    }
    return compilation.setPreloadedProjectGraph(std::move(graph), error);
}
}
