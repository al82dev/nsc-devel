#include "nativesharp/Compilation/CompilationInputManifest.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Source/SourceManager.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace nativesharp::compilation
{
namespace
{
std::string inputPathKey(const std::filesystem::path& path)
{
    std::string key = path.lexically_normal().generic_string();
    std::transform(
        key.begin(), key.end(), key.begin(),
        [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        });
    return key;
}

void appendIdentityField(std::string& material, std::string_view value)
{
    material += std::to_string(value.size());
    material.push_back(':');
    material.append(value);
}

std::string logicalInputPath(
    const std::filesystem::path& rootDirectory,
    const std::filesystem::path& path)
{
    const std::filesystem::path relative = path.lexically_relative(rootDirectory);
    if (!relative.empty() && *relative.begin() != "..")
    {
        return relative.generic_string();
    }
    return path.filename().generic_string();
}

bool appendInputIdentity(
    std::string& material,
    const std::filesystem::path& rootDirectory,
    const std::filesystem::path& path,
    const std::unordered_set<std::string>& sourceInputKeys,
    const std::unordered_map<std::string, std::string>& inputDigests,
    const std::unordered_map<std::string, std::uintmax_t>& inputSizes,
    std::unordered_map<std::string, std::string>& newSourceBuffers,
    std::unordered_map<std::string, std::string>& newInputDigests,
    std::unordered_map<std::string, std::uintmax_t>& newInputSizes,
    const LibraryMetadataRepository& metadataRepository,
    const FileDigestCache& digestCache,
    std::string& error)
{
    const std::string pathKey = inputPathKey(path);
    const auto cachedDigest = inputDigests.find(pathKey);
    const auto cachedSize = inputSizes.find(pathKey);
    const bool cached =
        cachedDigest != inputDigests.end() && cachedSize != inputSizes.end();
    std::string digestText;
    std::uintmax_t size = 0;
    if (cached)
    {
        digestText = cachedDigest->second;
        size = cachedSize->second;
    }
    else if (sourceInputKeys.contains(pathKey))
    {
        std::optional<std::string> buffer =
            source::SourceManager::readFileBuffer(path, error);
        if (!buffer.has_value())
        {
            return false;
        }
        const std::span<const std::uint8_t> bytes{
            reinterpret_cast<const std::uint8_t*>(buffer->data()), buffer->size()};
        digestText = basic::sha256Hex(basic::sha256(bytes));
        size = buffer->size();
        newSourceBuffers.insert_or_assign(pathKey, std::move(*buffer));
    }
    else if (const LibraryMetadataRepositoryEntry* entry = metadataRepository.find(path))
    {
        digestText = basic::sha256Hex(entry->contentDigest());
        size = entry->bytes().size();
    }
    else
    {
        basic::Sha256Digest digest{};
        if (!digestCache.digest(path, digest, size, error))
        {
            return false;
        }
        digestText = basic::sha256Hex(digest);
    }
    if (!cached)
    {
        newInputDigests.insert_or_assign(pathKey, digestText);
        newInputSizes.insert_or_assign(pathKey, size);
    }
    appendIdentityField(material, logicalInputPath(rootDirectory, path));
    appendIdentityField(material, std::to_string(size));
    appendIdentityField(material, digestText);
    return true;
}
}

CompilationInputManifest::CompilationInputManifest(
    ProjectGraph graph,
    ProjectLoadOptions loadOptions)
    : graph_(std::move(graph)),
      rootDirectory_(graph_.rootProjectFile().parent_path()),
      loadOptions_(std::move(loadOptions))
{
    collectGraphInputs(graph_);
    canonicalizeInputOrder();
}

const ProjectGraph& CompilationInputManifest::projectGraph() const noexcept
{
    return graph_;
}

const std::filesystem::path& CompilationInputManifest::rootDirectory() const noexcept
{
    return rootDirectory_;
}

ProjectGraph CompilationInputManifest::releaseProjectGraph()
{
    return std::move(graph_);
}

const ProjectLoadOptions& CompilationInputManifest::loadOptions() const noexcept
{
    return loadOptions_;
}

std::span<const std::filesystem::path> CompilationInputManifest::inputFiles() const noexcept
{
    return inputFiles_;
}

void CompilationInputManifest::addInputFile(std::filesystem::path path)
{
    if (path.empty())
    {
        return;
    }
    path = path.lexically_normal();
    const std::string key = inputPathKey(path);
    const auto position = std::lower_bound(
        inputFiles_.begin(), inputFiles_.end(), key,
        [](const std::filesystem::path& candidate, const std::string& expected)
        {
            return inputPathKey(candidate) < expected;
        });
    if (position != inputFiles_.end() && inputPathKey(*position) == key)
    {
        return;
    }
    inputFiles_.insert(position, std::move(path));
    identity_.clear();
}

void CompilationInputManifest::addIdentityField(std::string name, std::string value)
{
    identityFields_.emplace_back(std::move(name), std::move(value));
    identity_.clear();
}

void CompilationInputManifest::addVerifiedInputDigest(
    std::filesystem::path path,
    std::string digest,
    std::uintmax_t size)
{
    const std::string key = inputPathKey(path.lexically_normal());
    inputDigests_.insert_or_assign(key, std::move(digest));
    inputSizes_.insert_or_assign(key, size);
    identity_.clear();
}

void CompilationInputManifest::addProjectGraphInputs(const ProjectGraph& graph)
{
    collectGraphInputs(graph);
    canonicalizeInputOrder();
    identity_.clear();
}

void CompilationInputManifest::setInputDigestCacheRoot(std::filesystem::path root)
{
    inputDigestCache_.setRoot(std::move(root));
}

bool CompilationInputManifest::calculateIdentity(
    const LibraryMetadataRepository& metadataRepository,
    std::string& error)
{
    std::vector<std::pair<std::string, std::string>> fields = identityFields_;
    std::sort(fields.begin(), fields.end());
    std::string material;
    appendIdentityField(material, "nsc-compilation-bundle-v1");
    appendIdentityField(material, kCompilerVersion);
    appendIdentityField(material, "generic-instantiation-depth");
    appendIdentityField(
        material,
        loadOptions_.genericInstantiationDepth.has_value()
            ? std::to_string(*loadOptions_.genericInstantiationDepth)
            : "default");
    appendIdentityField(material, "generic-instantiation-count");
    appendIdentityField(
        material,
        loadOptions_.genericInstantiationCount.has_value()
            ? std::to_string(*loadOptions_.genericInstantiationCount)
            : "default");
    for (const auto& [name, value] : fields)
    {
        appendIdentityField(material, name);
        appendIdentityField(material, value);
    }
    std::unordered_map<std::string, std::string> sourceBuffers;
    sourceBuffers.reserve(sourceInputKeys_.size());
    std::unordered_map<std::string, std::string> inputDigests;
    std::unordered_map<std::string, std::uintmax_t> inputSizes;
    inputDigests.reserve(inputFiles_.size());
    inputSizes.reserve(inputFiles_.size());
    for (const std::filesystem::path& path : inputFiles_)
    {
        if (!appendInputIdentity(
                material,
                rootDirectory_,
                path,
                sourceInputKeys_,
                inputDigests_,
                inputSizes_,
                sourceBuffers,
                inputDigests,
                inputSizes,
                metadataRepository,
                inputDigestCache_,
                error))
        {
            identity_.clear();
            return false;
        }
    }
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size()};
    identity_ = basic::sha256Hex(basic::sha256(bytes));
    sourceBuffers_.merge(sourceBuffers);
    inputDigests_.merge(inputDigests);
    inputSizes_.merge(inputSizes);
    return true;
}

std::optional<std::string> CompilationInputManifest::sourceBuffer(
    const std::filesystem::path& path) const
{
    const auto position = sourceBuffers_.find(inputPathKey(path));
    if (position == sourceBuffers_.end())
    {
        return std::nullopt;
    }
    return position->second;
}

bool CompilationInputManifest::inputDigest(
    const std::filesystem::path& path,
    std::string& digest,
    std::uintmax_t& size) const
{
    const std::string key = inputPathKey(path);
    const auto digestPosition = inputDigests_.find(key);
    const auto sizePosition = inputSizes_.find(key);
    if (digestPosition == inputDigests_.end() || sizePosition == inputSizes_.end())
    {
        return false;
    }
    digest = digestPosition->second;
    size = sizePosition->second;
    return true;
}

std::string_view CompilationInputManifest::identity() const noexcept
{
    return identity_;
}

void CompilationInputManifest::setBundleProductKind(std::string productKind)
{
    bundleProductKind_ = std::move(productKind);
}

std::string_view CompilationInputManifest::bundleProductKind() const noexcept
{
    return bundleProductKind_;
}

void CompilationInputManifest::setBundleOutputPath(std::filesystem::path outputPath)
{
    bundleOutputDirectory_ = outputPath.parent_path();
    addRequiredBundleOutput(std::move(outputPath));
}

void CompilationInputManifest::addRequiredBundleOutput(
    std::filesystem::path outputPath)
{
    const std::string name = outputPath.filename().generic_string();
    if (name.empty() || std::ranges::find(requiredBundleOutputNames_, name) !=
            requiredBundleOutputNames_.end())
    {
        return;
    }
    requiredBundleOutputNames_.push_back(name);
}

const std::filesystem::path&
CompilationInputManifest::bundleOutputDirectory() const noexcept
{
    return bundleOutputDirectory_;
}

const std::vector<std::string>&
CompilationInputManifest::requiredBundleOutputNames() const noexcept
{
    return requiredBundleOutputNames_;
}

void CompilationInputManifest::setBundleResultOutput(std::string output)
{
    bundleResultOutput_ = std::move(output);
}

std::string_view CompilationInputManifest::bundleResultOutput() const noexcept
{
    return bundleResultOutput_;
}

void CompilationInputManifest::setBundleLookup(CompilationBundleLookup lookup)
{
    bundleLookup_ = std::move(lookup);
}

const CompilationBundleLookup& CompilationInputManifest::bundleLookup() const noexcept
{
    return bundleLookup_;
}

void CompilationInputManifest::collectGraphInputs(const ProjectGraph& graph)
{
    for (const Project& project : graph.projects())
    {
        inputFiles_.push_back(project.projectFile());
        for (const ProjectSource& source : project.sourceSet().sources())
        {
            inputFiles_.push_back(source.absolutePath);
            sourceInputKeys_.insert(inputPathKey(source.absolutePath));
        }
        for (const LibraryReference& reference : project.libraryReferences())
        {
            if (!reference.libraryPath.empty())
            {
                inputFiles_.push_back(reference.libraryPath);
            }
            if (!reference.metadataPath.empty())
            {
                inputFiles_.push_back(reference.metadataPath);
            }
            if (!reference.runtimePath.empty())
            {
                inputFiles_.push_back(reference.runtimePath);
            }
            if (!reference.productSidecarPath.empty())
            {
                inputFiles_.push_back(reference.productSidecarPath);
            }
        }
        for (const SdkManifest& sdk : project.sdkReferences())
        {
            if (!sdk.manifestPath.empty())
            {
                inputFiles_.push_back(sdk.manifestPath);
            }
            for (const SdkLibraryDependency& library : sdk.libraries)
            {
                inputFiles_.push_back(
                    sdk.manifestPath.parent_path() / library.relativePath);
            }
        }
    }
}

void CompilationInputManifest::canonicalizeInputOrder()
{
    std::sort(
        inputFiles_.begin(), inputFiles_.end(),
        [](const std::filesystem::path& left, const std::filesystem::path& right)
        {
            return inputPathKey(left) < inputPathKey(right);
        });
    inputFiles_.erase(
        std::unique(
            inputFiles_.begin(), inputFiles_.end(),
            [](const std::filesystem::path& left, const std::filesystem::path& right)
            {
                return inputPathKey(left) == inputPathKey(right);
            }),
        inputFiles_.end());
}
}
