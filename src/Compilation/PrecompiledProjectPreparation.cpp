#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Compilation/PrecompiledProductCache.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarCodec.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarPath.h"

#include "PrecompiledSurfaceReachability.h"

#include "../Driver/Internal/DriverInternal.h"
#include "../Frontend/Cache/SyntaxProductCacheCodec.h"

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
        [&codec](
            std::span<const std::uint8_t> bytes,
            interop::LibraryMetadata& result,
            std::string& decodeError) {
            return codec.readAuthenticated(bytes, result, decodeError);
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

bool publishedSyntaxProductIsValid(
    std::string_view sourceText,
    std::string_view syntaxProduct)
{
    if (syntaxProduct.empty()) return sourceText.empty();
    source::SourceFile sourceFile(
        FileId{1U}, std::filesystem::path{}, std::string(sourceText));
    std::vector<lexer::Token> tokens;
    std::unique_ptr<ast::AstArena> arena;
    ast::AstNode* syntaxTree = nullptr;
    return frontend::internal::deserializeRelocatableSyntaxProductUnit(
        sourceFile, syntaxProduct, tokens, arena, syntaxTree);
}

// Rejects the complete publication on any mismatch so the existing generation path stays atomic.
std::optional<PrecompiledProductSidecar> readPublishedPrecompiledProducts(
    const LoadedMetadata& entry,
    const LibraryMetadataRepository& repository)
{
    if (entry.productSidecarPath.empty()) return std::nullopt;
    std::string ignoredError;
    const LibraryMetadataRepositoryEntry* metadataFile =
        repository.load(entry.metadataPath, ignoredError);
    if (metadataFile == nullptr) return std::nullopt;
    PrecompiledProductSidecar decoded;
    if (!PrecompiledProductSidecarCodec::read(
            entry.productSidecarPath, metadataFile->bytes(), decoded, ignoredError))
    {
        return std::nullopt;
    }
    if (!publishedSyntaxProductIsValid(decoded.publicApiSource, decoded.publicApiSyntax) ||
        !publishedSyntaxProductIsValid(
            decoded.publicTemplateSource, decoded.publicTemplateSyntax))
    {
        return std::nullopt;
    }
    return decoded;
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
    bool& restoredPublishedProducts,
    std::string& error)
{
    restoredPublishedProducts = false;
    std::optional<PrecompiledProductSidecar> publishedProducts =
        readPublishedPrecompiledProducts(entry, repository);
    if (publishedProducts.has_value())
    {
        publicSource = std::move(publishedProducts->publicApiSource);
        templateSource = std::move(publishedProducts->publicTemplateSource);
        publicSyntax = std::move(publishedProducts->publicApiSyntax);
        templateSyntax = std::move(publishedProducts->publicTemplateSyntax);
        restoredPublishedProducts = true;
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

// The namespaces whose declarations the compiler refers to by key (strings, exceptions, numerics,
// atomics, files), without any name in the consumer's source.
bool isCompilerKnownNamespace(std::string_view name) noexcept
{
    return name == "NativeSharp.Core" || name == "NativeSharp.Core.Numerics" ||
        name == "NativeSharp.Core.Threading" || name == "NativeSharp.Core.IO";
}

// A module that declares a compiler-known namespace is never pruned. This covers the standard
// library and any library that embeds its own copy of it. The closed generic types every library
// publishes (`NativeSharp.Core.Span__g1__...`) do not count.
bool isStandardLibraryModule(const interop::LibraryMetadata& metadata)
{
    if (metadata.moduleName == "NativeSharpCore") return true;
    for (const interop::LibraryMetadataType& type : metadata.types)
    {
        const std::string_view name = type.fullyQualifiedName;
        if (name.find("__g") != std::string_view::npos) continue;
        const std::size_t nesting = name.find('+');
        const std::string_view outer = nesting == std::string_view::npos ? name : name.substr(0, nesting);
        const std::size_t separator = outer.find_last_of('.');
        if (separator != std::string_view::npos && isCompilerKnownNamespace(outer.substr(0, separator)))
            return true;
    }
    return false;
}

// Returns, in the order of `loaded`, the metadata whose surface each module materializes: the
// complete metadata without a demand, and otherwise only what the demanded names reach.
std::vector<std::shared_ptr<const interop::LibraryMetadata>> surfaceMetadata(
    const std::vector<LoadedMetadata>& loaded,
    const PrecompiledSurfaceDemand* demand)
{
    std::vector<std::shared_ptr<const interop::LibraryMetadata>> modules;
    modules.reserve(loaded.size());
    for (const LoadedMetadata& entry : loaded)
    {
        modules.push_back(entry.metadata);
    }
    if (demand == nullptr || !demand->enabled) return modules;
    std::vector<bool> fullModules;
    fullModules.reserve(loaded.size());
    for (const LoadedMetadata& entry : loaded)
    {
        fullModules.push_back(isStandardLibraryModule(*entry.metadata));
    }
    PrecompiledSurfaceReachability reachability(modules, fullModules);
    reachability.reach(demand->names);
    for (std::size_t index = 0; index < modules.size(); ++index)
    {
        modules[index] = reachability.prunedMetadata(index);
    }
    return modules;
}

// The cache key of a pruned surface: the key of the complete products plus the generated text, so a
// local syntax cache entry is shared only by compilations that materialize the same surface.
std::string prunedProductCacheKey(
    std::string_view productCacheKey,
    std::string_view publicSource,
    std::string_view templateSource)
{
    if (productCacheKey.empty()) return {};
    std::string material;
    appendCacheIdentityField(material, "nsc-precompiled-pruned-products-v1");
    appendCacheIdentityField(material, productCacheKey);
    appendCacheIdentityField(material, publicSource);
    appendCacheIdentityField(material, templateSource);
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size()};
    return basic::sha256Hex(basic::sha256(bytes));
}

bool materializePrunedProducts(
    const interop::LibraryMetadata& surface,
    std::string& publicSource,
    std::string& templateSource,
    std::string& error)
{
    if (!driver::buildPrecompiledPublicSource(surface, publicSource, error))
    {
        error = "cannot materialize public API for module '" + surface.moduleName + "': " + error;
        return false;
    }
    if (!driver::buildPrecompiledGenericTemplateSource(surface, templateSource, error))
    {
        error = "cannot materialize public templates for module '" + surface.moduleName + "': " + error;
        return false;
    }
    return true;
}

bool materializeModules(
    std::vector<LoadedMetadata>& loaded,
    const LibraryMetadataRepository& repository,
    const std::filesystem::path& cacheRoot,
    const PrecompiledSurfaceDemand* demand,
    std::vector<frontend::PrecompiledSyntaxModule>& modules,
    std::string& error)
{
    const std::vector<std::shared_ptr<const interop::LibraryMetadata>> surfaces =
        surfaceMetadata(loaded, demand);
    modules.reserve(loaded.size());
    for (std::size_t index = 0; index < loaded.size(); ++index)
    {
        LoadedMetadata& entry = loaded[index];
        const interop::LibraryMetadata& surface = *surfaces[index];
        const bool pruned = surfaces[index] != entry.metadata;
        std::unordered_set<std::string_view> concreteMemberNames;
        concreteMemberNames.reserve(surface.members.size());
        for (const interop::LibraryMetadataMember& member : surface.members)
        {
            concreteMemberNames.insert(member.name);
        }
        std::string productCacheKey = precompiledProductCacheKey(entry, repository);
        std::string publicSource;
        std::string templateSource;
        std::string publicSyntax;
        std::string templateSyntax;
        bool restoredPublishedProducts = false;
        if (pruned)
        {
            // The published products and the product cache hold the complete surface.
            if (!materializePrunedProducts(surface, publicSource, templateSource, error)) return false;
            productCacheKey = prunedProductCacheKey(productCacheKey, publicSource, templateSource);
        }
        else if (!materializeModuleProducts(
                entry,
                repository,
                cacheRoot,
                productCacheKey,
                publicSource,
                templateSource,
                publicSyntax,
                templateSyntax,
                restoredPublishedProducts,
                error))
        {
            return false;
        }
        modules.push_back({
            entry.ownerProjectFile,
            entry.metadataPath,
            surface.moduleName,
            productCacheKey,
            std::move(publicSource),
            std::move(templateSource),
            std::move(publicSyntax),
            std::move(templateSyntax),
            memberIdentities(surface),
            namespaceIdentities(surface),
            genericSpecializations(surface, concreteMemberNames),
            restoredPublishedProducts
                ? entry.productSidecarPath
                : std::filesystem::path{}});
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
    // Each known dependency may append one entry; reserving them avoids most reallocations
    // of loaded while the closure grows.
    std::size_t knownDependencies = 0;
    for (const LoadedMetadata& entry : loaded)
        knownDependencies += entry.metadata->binaryDependencies.size();
    loaded.reserve(loaded.size() + knownDependencies);
    for (std::size_t cursor = 0; cursor < loaded.size(); ++cursor)
    {
        const std::filesystem::path parentOwner = loaded[cursor].ownerProjectFile;
        const std::filesystem::path parentMetadataPath = loaded[cursor].metadataPath;
        // The metadata is shared and immutable: holding its pointer keeps the dependency list
        // valid while expansion appends to loaded, without copying the list.
        const std::shared_ptr<const interop::LibraryMetadata> parentMetadata =
            loaded[cursor].metadata;
        const std::string& parentModule = parentMetadata->moduleName;
        for (const interop::LibraryMetadataBinaryDependency& dependency :
             parentMetadata->binaryDependencies)
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
    return preparePrecompiledProject(
        std::move(graph), loadOptions, compilation, error, PrecompiledMaterializationGate{});
}

/// EN: Loads the explicit metadata, expands the binary closure, applies it to `graph`, asks
///     `gate` (when present) whether materialization is needed, materializes the precompiled
///     modules unless skipped, and installs the graph in `compilation`.
///     Parameters: `graph`, the loaded project graph (consumed); `loadOptions`, the load policy;
///     `compilation`, a compilation that has not started; `error`, the failure text; `gate`, the
///     optional hook described in the header.
///     Returns false on the first failure. Effects: fills the compilation's metadata repository,
///     its precompiled modules (unless skipped) and its preloaded graph. Not thread safe for one
///     compilation. Complexity: linear in the referenced metadata plus materialization.
///     Why the gate: on a verified bundle hit the materialized modules are never used, so building
///     them is wasted work. Measured on NativeSharp.Math.Functions (O0, audit 2026-10-22) it was
///     about 10-15% of `precompiled-metadata` on that path; loading and decoding the referenced
///     metadata, still needed to compute the identity, is the larger remaining part.
/// ES: Carga la metadata explicita, expande la clausura binaria, la aplica a `graph`, pregunta a
///     `gate` (si existe) si hace falta materializar, materializa los modulos precompilados salvo
///     que se omita, e instala el grafo en `compilation`.
///     Parametros: `graph`, el grafo cargado (se consume); `loadOptions`, la politica de carga;
///     `compilation`, una compilacion no iniciada; `error`, el texto de falla; `gate`, el gancho
///     opcional descrito en el header.
///     Retorno: false en la primera falla. Efectos: llena el repositorio de metadata, los modulos
///     precompilados (salvo omision) y el grafo precargado de la compilacion. No es seguro entre
///     hilos para una misma compilacion. Complejidad: lineal en la metadata mas la materializacion.
///     Motivo del gancho: en un acierto de bundle verificado los modulos materializados nunca se
///     usan, asi que construirlos es trabajo perdido. Medido en NativeSharp.Math.Functions (O0,
///     auditoria 2026-10-22) era cerca del 10-15% de `precompiled-metadata` en esa ruta; cargar y
///     decodificar la metadata referenciada, que sigue haciendo falta para la identidad, es la
///     parte mayor que queda.
bool preparePrecompiledProject(
    ProjectGraph graph,
    const ProjectLoadOptions& loadOptions,
    frontend::ProjectCompilation& compilation,
    std::string& error,
    const PrecompiledMaterializationGate& gate)
{
    return preparePrecompiledProject(std::move(graph), loadOptions, compilation, error, gate, nullptr);
}

bool preparePrecompiledProject(
    ProjectGraph graph,
    const ProjectLoadOptions& loadOptions,
    frontend::ProjectCompilation& compilation,
    std::string& error,
    const PrecompiledMaterializationGate& gate,
    const PrecompiledSurfaceSourceProvider* surfaceDemandSources)
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

    bool skipMaterialization = false;
    if (gate && !gate(graph, skipMaterialization, error))
    {
        return false;
    }
    if (skipMaterialization)
    {
        return compilation.setPreloadedProjectGraph(std::move(graph), error);
    }
    std::optional<PrecompiledSurfaceDemand> demand;
    if (surfaceDemandSources != nullptr)
    {
        demand = collectPrecompiledSurfaceDemand(graph, *surfaceDemandSources);
    }
    std::vector<frontend::PrecompiledSyntaxModule> modules;
    if (!materializeModules(
            loaded,
            compilation.libraryMetadataRepository(),
            compilation.precompiledProductCacheRoot(),
            demand.has_value() ? &*demand : nullptr,
            modules,
            error) ||
        !compilation.setPrecompiledModules(std::move(modules), error))
    {
        return false;
    }
    return compilation.setPreloadedProjectGraph(std::move(graph), error);
}
}
