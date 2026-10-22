#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Compilation/PrecompiledProductCache.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarCodec.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarPath.h"

#include "../Driver/Internal/DriverInternal.h"

#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace nativesharp::compilation
{
namespace
{
using LoadedMetadata = driver::LoadedPrecompiledMetadata;

void appendCacheIdentityField(std::string& material, std::string_view field)
{
    material += std::to_string(field.size());
    material.push_back(':');
    material.append(field);
}

std::shared_ptr<const interop::LibraryMetadata> readMetadata(
    const LibraryMetadataRepository& repository,
    interop::LibraryMetadataCodec& codec,
    const std::filesystem::path& path,
    std::string& error)
{
    std::shared_ptr<const interop::LibraryMetadata> decoded = repository.decode(
        path,
        [&codec](
            std::span<const std::uint8_t> bytes,
            interop::LibraryMetadata& result,
            std::string& decodeError) {
            return codec.read(bytes, result, decodeError);
        },
        error);
    if (decoded == nullptr) return {};
    if (!repository.verify(
            path,
            [&codec](const interop::LibraryMetadata& candidate, std::string& verificationError) {
                return codec.verifyCurrentCompiler(candidate, verificationError);
            },
            error))
    {
        return {};
    }
    return decoded;
}

std::string precompiledProductCacheKey(
    const LoadedMetadata& entry,
    const LibraryMetadataRepository& repository)
{
    std::string ignoredError;
    const LibraryMetadataRepositoryEntry* file = repository.load(entry.metadataPath, ignoredError);
    if (file == nullptr) return {};
    std::string material;
    appendCacheIdentityField(material, "nsc-precompiled-products-v1");
    appendCacheIdentityField(material, kCompilerBuildId);
    appendCacheIdentityField(material, entry.metadata->moduleName);
    appendCacheIdentityField(material, std::to_string(file->bytes().size()));
    appendCacheIdentityField(material, basic::sha256Hex(file->contentDigest()));
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size()};
    return basic::sha256Hex(basic::sha256(bytes));
}

bool restorePrecompiledSourceProducts(
    const std::filesystem::path& cacheRoot,
    std::string_view cacheKey,
    std::string& publicSource,
    std::string& templateSource)
{
    if (cacheRoot.empty() || cacheKey.empty()) return false;
    PrecompiledProductCache cache;
    cache.setRoot(cacheRoot);
    bool hit = false;
    std::string ignoredError;
    return cache.restore(
        cacheKey, publicSource, templateSource, hit, ignoredError) && hit;
}

void storePrecompiledSourceProducts(
    const std::filesystem::path& cacheRoot,
    std::string_view cacheKey,
    std::string_view publicSource,
    std::string_view templateSource)
{
    if (cacheRoot.empty() || cacheKey.empty()) return;
    PrecompiledProductCache cache;
    cache.setRoot(cacheRoot);
    std::string ignoredError;
    (void)cache.store(
        cacheKey, publicSource, templateSource, ignoredError);
}

// A present sidecar is accepted only after binding it to the already loaded NSMETA bytes.
bool readPublishedPrecompiledProducts(
    const LoadedMetadata& entry,
    const LibraryMetadataRepository& repository,
    std::optional<PrecompiledProductSidecar>& products,
    std::string& error)
{
    products.reset();
    if (entry.productSidecarPath.empty()) return true;
    const LibraryMetadataRepositoryEntry* metadataFile =
        repository.load(entry.metadataPath, error);
    if (metadataFile == nullptr) return false;
    PrecompiledProductSidecar decoded;
    if (!PrecompiledProductSidecarCodec::read(
            entry.productSidecarPath, metadataFile->bytes(), decoded, error))
    {
        error = "cannot restore precompiled products for module '" +
            entry.metadata->moduleName + "': " + error;
        return false;
    }
    products = std::move(decoded);
    return true;
}

// Selects published products before any local cache lookup or metadata-to-source reconstruction.
bool materializeModuleProducts(
    const LoadedMetadata& entry,
    const LibraryMetadataRepository& repository,
    const std::filesystem::path& cacheRoot,
    std::string_view productCacheKey,
    std::string& publicSource,
    std::string& templateSource,
    std::string& publicSyntax,
    std::string& templateSyntax,
    std::string& error)
{
    std::optional<PrecompiledProductSidecar> publishedProducts;
    if (!readPublishedPrecompiledProducts(
            entry, repository, publishedProducts, error))
    {
        return false;
    }
    if (publishedProducts.has_value())
    {
        publicSource = std::move(publishedProducts->publicApiSource);
        templateSource = std::move(publishedProducts->publicTemplateSource);
        publicSyntax = std::move(publishedProducts->publicApiSyntax);
        templateSyntax = std::move(publishedProducts->publicTemplateSyntax);
        return true;
    }
    if (restorePrecompiledSourceProducts(
            cacheRoot, productCacheKey, publicSource, templateSource))
    {
        return true;
    }
    const interop::LibraryMetadata& metadata = *entry.metadata;
    if (!driver::buildPrecompiledPublicSource(metadata, publicSource, error))
    {
        error = "cannot materialize public API for module '" +
            metadata.moduleName + "': " + error;
        return false;
    }
    if (!driver::buildPrecompiledGenericTemplateSource(
            metadata, templateSource, error))
    {
        error = "cannot materialize public templates for module '" +
            metadata.moduleName + "': " + error;
        return false;
    }
    storePrecompiledSourceProducts(
        cacheRoot, productCacheKey, publicSource, templateSource);
    return true;
}

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
    const LibraryMetadataRepository& repository,
    const std::filesystem::path& cacheRoot,
    std::vector<frontend::PrecompiledSyntaxModule>& modules,
    std::string& error)
{
    modules.reserve(loaded.size());
    for (LoadedMetadata& entry : loaded)
    {
        const interop::LibraryMetadata& metadata = *entry.metadata;
        std::unordered_set<std::string_view> concreteMemberNames;
        concreteMemberNames.reserve(metadata.members.size());
        for (const interop::LibraryMetadataMember& member : metadata.members)
        {
            concreteMemberNames.insert(member.name);
        }
        const std::string productCacheKey = precompiledProductCacheKey(entry, repository);
        std::string publicSource;
        std::string templateSource;
        std::string publicSyntax;
        std::string templateSyntax;
        if (!materializeModuleProducts(
                entry,
                repository,
                cacheRoot,
                productCacheKey,
                publicSource,
                templateSource,
                publicSyntax,
                templateSyntax,
                error))
        {
            return false;
        }
        modules.push_back({
            entry.ownerProjectFile,
            entry.metadataPath,
            metadata.moduleName,
            productCacheKey,
            std::move(publicSource),
            std::move(templateSource),
            std::move(publicSyntax),
            std::move(templateSyntax),
            memberIdentities(metadata),
            namespaceIdentities(metadata),
            genericSpecializations(metadata, concreteMemberNames),
            entry.productSidecarPath});
    }
    return true;
}

bool loadExplicitMetadata(
    const ProjectGraph& graph,
    const LibraryMetadataRepository& repository,
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
            std::shared_ptr<const interop::LibraryMetadata> metadata =
                readMetadata(repository, codec, reference.metadataPath, error);
            if (metadata == nullptr)
            {
                error = "cannot import precompiled metadata '" +
                    reference.metadataPath.generic_string() + "': " + error;
                return false;
            }
            const auto sourceProject = sourceProjects.find(metadata->moduleName);
            if (sourceProject != sourceProjects.end())
            {
                sourceReplacements.push_back({
                    project.projectFile(),
                    reference.metadataPath,
                    sourceProject->second});
                continue;
            }
            const auto existing = loadedByModule.find(metadata->moduleName);
            if (existing != loadedByModule.end())
            {
                const LoadedMetadata& prior = loaded[existing->second];
                if (prior.metadata->abiFingerprint != metadata->abiFingerprint ||
                    driver::foldedPathKey(prior.metadataPath) !=
                        driver::foldedPathKey(reference.metadataPath))
                {
                    error = "precompiled module '" + metadata->moduleName +
                        "' was explicitly referenced with incompatible artifacts '" +
                        prior.metadataPath.generic_string() + "' and '" +
                        reference.metadataPath.generic_string() + "'";
                    return false;
                }
                continue;
            }
            loadedByModule.emplace(metadata->moduleName, loaded.size());
            loaded.push_back({
                project.projectFile(),
                reference.metadataPath,
                std::move(metadata),
                reference.productSidecarPath});
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
                : root / dependency.runtimeFileName,
            availablePrecompiledProductSidecarPath(
                root / dependency.metadataFileName)};
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
    const LibraryMetadataRepository& repository,
    interop::LibraryMetadataCodec& codec,
    std::shared_ptr<const interop::LibraryMetadata>& childMetadata,
    std::string& error)
{
    childMetadata = readMetadata(repository, codec, resolved.metadataPath, error);
    if (childMetadata == nullptr)
    {
        error = "cannot import transitive metadata '" +
            resolved.metadataPath.generic_string() + "' required by module '" +
            parentModule + "': " + error;
        return false;
    }
    if (childMetadata->moduleName != dependency.moduleName)
    {
        error = "binary closure expected module '" + dependency.moduleName +
            "' but metadata '" + resolved.metadataPath.generic_string() +
            "' identifies '" + childMetadata->moduleName + "'";
        return false;
    }
    if (childMetadata->abiFingerprint != dependency.abiFingerprint)
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
    if (prior.metadata->abiFingerprint == dependency.abiFingerprint) return true;
    error = "binary closure ABI fingerprint mismatch for explicitly "
        "loaded module '" + dependency.moduleName +
        "' required by '" + parentModule + "'";
    return false;
}

bool appendResolvedBinaryDependency(
    const std::filesystem::path& parentOwner,
    LibraryReference resolved,
    std::shared_ptr<const interop::LibraryMetadata> childMetadata,
    std::vector<LoadedMetadata>& loaded,
    std::unordered_map<std::string, std::size_t>& loadedByModule,
    std::unordered_map<std::string, std::vector<LibraryReference>>& discoveredByOwner,
    std::string& error)
{
    const auto existing = loadedByModule.find(childMetadata->moduleName);
    if (existing != loadedByModule.end() &&
        loaded[existing->second].metadata->abiFingerprint !=
            childMetadata->abiFingerprint)
    {
        error = "binary closure selects incompatible versions of module '" +
            childMetadata->moduleName + "'";
        return false;
    }
    if (existing != loadedByModule.end()) return true;
    loadedByModule.emplace(childMetadata->moduleName, loaded.size());
    loaded.push_back({
        parentOwner,
        resolved.metadataPath,
        std::move(childMetadata),
        resolved.productSidecarPath});
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
    const LibraryMetadataRepository& repository,
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
    std::shared_ptr<const interop::LibraryMetadata> childMetadata;
    if (!loadBinaryDependency(
            dependency,
            parentModule,
            resolved,
            repository,
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
    const LibraryMetadataRepository& repository,
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
        const std::string parentModule = loaded[cursor].metadata->moduleName;
        const std::vector<interop::LibraryMetadataBinaryDependency> dependencies =
            loaded[cursor].metadata->binaryDependencies;
        for (const interop::LibraryMetadataBinaryDependency& dependency : dependencies)
        {
            if (!expandBinaryDependency(
                    loadOptions,
                    dependency,
                    parentOwner,
                    parentMetadataPath,
                    parentModule,
                    repository,
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
            compilation.libraryMetadataRepository(),
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
            compilation.libraryMetadataRepository(),
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
    if (!materializeModules(
            loaded,
            compilation.libraryMetadataRepository(),
            compilation.precompiledProductCacheRoot(),
            modules,
            error) ||
        !compilation.setPrecompiledModules(std::move(modules), error))
    {
        return false;
    }
    return compilation.setPreloadedProjectGraph(std::move(graph), error);
}
}
