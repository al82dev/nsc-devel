#include "nativesharp/Compilation/CompilationInputManifest.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"

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
    const LibraryMetadataRepository& metadataRepository,
    std::string& error)
{
    basic::Sha256Digest digest{};
    std::uintmax_t size = 0;
    if (const LibraryMetadataRepositoryEntry* entry = metadataRepository.find(path))
    {
        digest = entry->contentDigest();
        size = entry->bytes().size();
    }
    else if (!basic::sha256File(path, digest, size, error))
    {
        return false;
    }
    appendIdentityField(material, logicalInputPath(rootDirectory, path));
    appendIdentityField(material, std::to_string(size));
    appendIdentityField(material, basic::sha256Hex(digest));
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

void CompilationInputManifest::addProjectGraphInputs(const ProjectGraph& graph)
{
    collectGraphInputs(graph);
    canonicalizeInputOrder();
    identity_.clear();
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
    for (const std::filesystem::path& path : inputFiles_)
    {
        if (!appendInputIdentity(
                material, rootDirectory_, path, metadataRepository, error))
        {
            identity_.clear();
            return false;
        }
    }
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size()};
    identity_ = basic::sha256Hex(basic::sha256(bytes));
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
