#include "nativesharp/LLVM/CompilationArtifactCache.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmVersion.h"

#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <span>
#include <system_error>
#include <vector>

namespace nativesharp::llvm_backend
{
namespace
{
thread_local const CompilationArtifactCache* activeCache = nullptr;

bool validKey(std::string_view key) noexcept
{
    if (key.size() != 64U) return false;
    for (const char character : key)
    {
        if ((character < '0' || character > '9') &&
            (character < 'a' || character > 'f'))
            return false;
    }
    return true;
}

std::filesystem::path objectDirectory(
    const std::filesystem::path& root,
    std::string_view key)
{
    return root / "objects" / std::string(key.substr(0, 2));
}

std::filesystem::path objectPath(
    const std::filesystem::path& root,
    std::string_view key)
{
    return objectDirectory(root, key) / (std::string(key) + ".obj");
}

std::filesystem::path metadataPath(
    const std::filesystem::path& root,
    std::string_view key)
{
    return objectDirectory(root, key) / (std::string(key) + ".meta");
}

struct CacheMetadata final
{
    std::string key;
    std::string digest;
    std::uintmax_t size = 0;
};

bool readMetadata(const std::filesystem::path& path, CacheMetadata& metadata)
{
    std::ifstream stream(path, std::ios::binary);
    std::string header;
    std::string keyLine;
    std::string digestLine;
    std::string sizeLine;
    std::string trailing;
    if (!stream || !std::getline(stream, header) || !std::getline(stream, keyLine) ||
        !std::getline(stream, digestLine) || !std::getline(stream, sizeLine) ||
        std::getline(stream, trailing) || header != "NSCOBJ 1" ||
        !keyLine.starts_with("key ") || !digestLine.starts_with("sha256 ") ||
        !sizeLine.starts_with("size "))
        return false;
    metadata.key = keyLine.substr(4U);
    metadata.digest = digestLine.substr(7U);
    if (!validKey(metadata.key) || !validKey(metadata.digest)) return false;
    const std::string_view sizeText(sizeLine.data() + 5U, sizeLine.size() - 5U);
    const auto [end, parseError] = std::from_chars(
        sizeText.data(), sizeText.data() + sizeText.size(), metadata.size);
    return parseError == std::errc{} && end == sizeText.data() + sizeText.size();
}

bool writeMetadataAtomically(
    const std::filesystem::path& destination,
    std::string_view key,
    std::string_view digest,
    std::uintmax_t size,
    std::string& error)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path temporary =
        destination.string() + ".tmp." + std::to_string(stamp);
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "cannot open artifact-cache metadata temporary '" +
                temporary.string() + "'";
            return false;
        }
        stream << "NSCOBJ 1\nkey " << key << "\nsha256 " << digest <<
            "\nsize " << size << '\n';
        if (!stream)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            error = "cannot write artifact-cache metadata '" + temporary.string() + "'";
            return false;
        }
    }
    std::error_code filesystemError;
    std::filesystem::remove(destination, filesystemError);
    filesystemError.clear();
    std::filesystem::rename(temporary, destination, filesystemError);
    if (filesystemError)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        error = "cannot commit artifact-cache metadata '" + destination.string() +
            "': " + filesystemError.message();
        return false;
    }
    return true;
}
}

void CompilationArtifactCache::setRoot(std::filesystem::path root)
{
    root_ = std::move(root).lexically_normal();
}

void CompilationArtifactCache::setToolchainIdentity(std::string identity)
{
    toolchainIdentity_ = std::move(identity);
}

bool CompilationArtifactCache::enabled() const noexcept
{
    return !root_.empty();
}

const std::filesystem::path& CompilationArtifactCache::root() const noexcept
{
    return root_;
}

std::string CompilationArtifactCache::objectKey(
    std::string_view optimizedLlvm,
    std::string_view targetTriple,
    std::string_view dataLayout,
    LlvmOptimizationLevel optimization,
    DebugInformationKind debugInformation) const
{
    const LlvmVersion llvm = loadedLlvmVersion();
    std::string material = "NSCOBJKEY 1\n";
    material += "toolchain " + toolchainIdentity_ + "\n";
    material += "llvm " + llvmVersionString(llvm) + "\n";
    material += "target " + std::string(targetTriple) + "\n";
    material += "layout " + std::string(dataLayout) + "\n";
    material += "optimization " + std::string(llvmOptimizationLevelName(optimization)) + "\n";
    material += "debug " + std::string(debugInformationKindName(debugInformation)) + "\n";
    material += "ir-size " + std::to_string(optimizedLlvm.size()) + "\n";
    material.append(optimizedLlvm);
    const auto bytes = std::span(
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size());
    return basic::sha256Hex(basic::sha256(bytes));
}

std::string CompilationArtifactCache::inputObjectKey(
    std::string_view mirIdentity,
    std::string_view targetIdentity,
    LlvmOptimizationLevel optimization,
    DebugInformationKind debugInformation) const
{
    const LlvmVersion llvm = loadedLlvmVersion();
    std::string material = "NSCOBJINPUTKEY 1\n";
    material += "toolchain " + toolchainIdentity_ + "\n";
    material += "llvm " + llvmVersionString(llvm) + "\n";
    material += "target " + std::string(targetIdentity) + "\n";
    material += "optimization " + std::string(llvmOptimizationLevelName(optimization)) + "\n";
    material += "debug " + std::string(debugInformationKindName(debugInformation)) + "\n";
    material += "mir-size " + std::to_string(mirIdentity.size()) + "\n";
    material.append(mirIdentity);
    const auto bytes = std::span(
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size());
    return basic::sha256Hex(basic::sha256(bytes));
}

bool CompilationArtifactCache::restoreObject(
    std::string_view key,
    const std::filesystem::path& destination,
    bool& hit,
    std::string& error) const
{
    hit = false;
    error.clear();
    if (!enabled() || !validKey(key))
    {
        if (!enabled()) return true;
        error = "artifact-cache object key is not canonical SHA-256";
        return false;
    }
    const std::filesystem::path cachedObject = objectPath(root_, key);
    const std::filesystem::path cachedMetadata = metadataPath(root_, key);
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(cachedObject, filesystemError) || filesystemError)
        return true;
    filesystemError.clear();
    if (!std::filesystem::is_regular_file(cachedMetadata, filesystemError) || filesystemError)
        return true;

    CacheMetadata metadata;
    if (!readMetadata(cachedMetadata, metadata) || metadata.key != key) return true;
    basic::Sha256Digest digest{};
    std::uintmax_t size = 0;
    std::string hashError;
    if (!basic::sha256File(cachedObject, digest, size, hashError)) return true;
    if (size != metadata.size || basic::sha256Hex(digest) != metadata.digest) return true;

    std::filesystem::copy_file(
        cachedObject, destination, std::filesystem::copy_options::overwrite_existing,
        filesystemError);
    if (filesystemError)
    {
        error = "cannot restore cached object '" + cachedObject.string() + "' to '" +
            destination.string() + "': " + filesystemError.message();
        return false;
    }
    hit = true;
    return true;
}

bool CompilationArtifactCache::storeObject(
    std::string_view key,
    const std::filesystem::path& source,
    std::string& error) const
{
    error.clear();
    if (!enabled()) return true;
    if (!validKey(key))
    {
        error = "artifact-cache object key is not canonical SHA-256";
        return false;
    }
    basic::Sha256Digest digest{};
    std::uintmax_t size = 0;
    if (!basic::sha256File(source, digest, size, error)) return false;

    const std::filesystem::path directory = objectDirectory(root_, key);
    std::error_code filesystemError;
    std::filesystem::create_directories(directory, filesystemError);
    if (filesystemError)
    {
        error = "cannot create artifact-cache directory '" + directory.string() +
            "': " + filesystemError.message();
        return false;
    }
    const std::filesystem::path cachedObject = objectPath(root_, key);
    std::filesystem::copy_file(
        source, cachedObject, std::filesystem::copy_options::overwrite_existing,
        filesystemError);
    if (filesystemError)
    {
        error = "cannot store cached object '" + cachedObject.string() + "': " +
            filesystemError.message();
        return false;
    }
    return writeMetadataAtomically(
        metadataPath(root_, key), key, basic::sha256Hex(digest), size, error);
}

CompilationArtifactCacheScope::CompilationArtifactCacheScope(
    const CompilationArtifactCache& cache) noexcept
    : previous_(activeCache)
{
    activeCache = &cache;
}

CompilationArtifactCacheScope::~CompilationArtifactCacheScope()
{
    activeCache = previous_;
}

const CompilationArtifactCache* activeCompilationArtifactCache() noexcept
{
    return activeCache;
}
}
