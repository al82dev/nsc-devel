#include "nativesharp/LLVM/CompilationArtifactCache.h"

#include "../Frontend/Cache/CacheEntryStore.h"
#include "Internal/ArtifactCacheFilesystemPath.h"
#include "Internal/CacheEntryLock.h"
#include "Internal/CachePruneEntry.h"
#include "Internal/CachePruneMeasurement.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/FloatingPointMode.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmVersion.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <limits>
#include <span>
#include <system_error>
#include <vector>

namespace nativesharp::llvm_backend
{
namespace
{
thread_local const CompilationArtifactCache* activeCache = nullptr;
std::atomic<unsigned long long> temporaryOrdinal = 0;
frontend::internal::CacheEntryStore objectEntryStore(
    temporaryOrdinal, "NSCOBJ 1", "artifact-cache", "artifact-cache", false, true);
frontend::internal::CacheEntryStore codePartitionEntryStore(
    temporaryOrdinal, "NSCPART 1", "code-partition", "artifact-cache", false, true);

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

std::filesystem::path codePartitionDirectory(
    const std::filesystem::path& root,
    std::string_view key)
{
    return root / "partitions" / std::string(key.substr(0, 2));
}

std::filesystem::path codePartitionPayloadPath(
    const std::filesystem::path& root,
    std::string_view key)
{
    return codePartitionDirectory(root, key) / (std::string(key) + ".plan");
}

std::filesystem::path codePartitionMetadataPath(
    const std::filesystem::path& root,
    std::string_view key)
{
    return codePartitionDirectory(root, key) / (std::string(key) + ".meta");
}

void quarantineInvalidEntry(
    const std::filesystem::path& root,
    std::string_view key,
    const std::filesystem::path& object,
    const std::filesystem::path& metadata) noexcept
{
    std::error_code filesystemError;
    const std::filesystem::path quarantine = root / "quarantine";
    std::filesystem::create_directories(quarantine, filesystemError);
    if (filesystemError) return;
    const std::string suffix = std::to_string(
        temporaryOrdinal.fetch_add(1, std::memory_order_relaxed));
    const std::filesystem::path quarantinedMetadata =
        quarantine / (std::string(key) + "." + suffix + ".meta");
    const std::filesystem::path quarantinedObject =
        quarantine / (std::string(key) + "." + suffix + ".obj");
    filesystemError.clear();
    std::filesystem::rename(
        cacheFilesystemPath(metadata), cacheFilesystemPath(quarantinedMetadata), filesystemError);
    filesystemError.clear();
    std::filesystem::rename(
        cacheFilesystemPath(object), cacheFilesystemPath(quarantinedObject), filesystemError);
}

void quarantineInvalidCodePartitionPlan(
    const std::filesystem::path& root,
    std::string_view key,
    const std::filesystem::path& payload,
    const std::filesystem::path& metadata) noexcept
{
    std::error_code filesystemError;
    const std::filesystem::path quarantine = root / "quarantine";
    std::filesystem::create_directories(quarantine, filesystemError);
    if (filesystemError) return;
    const std::string suffix = std::to_string(
        temporaryOrdinal.fetch_add(1, std::memory_order_relaxed));
    filesystemError.clear();
    std::filesystem::rename(
        cacheFilesystemPath(metadata), cacheFilesystemPath(
            quarantine / (std::string(key) + "." + suffix + ".partition.meta")),
        filesystemError);
    filesystemError.clear();
    std::filesystem::rename(
        cacheFilesystemPath(payload), cacheFilesystemPath(
            quarantine / (std::string(key) + "." + suffix + ".partition.plan")),
        filesystemError);
}

bool cacheEntryFilesAvailable(
    const std::filesystem::path& cachedObject,
    const std::filesystem::path& cachedMetadata,
    CacheMissReason* reason)
{
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(
            cacheFilesystemPath(cachedObject), filesystemError) || filesystemError)
        return false;
    filesystemError.clear();
    if (std::filesystem::is_regular_file(
            cacheFilesystemPath(cachedMetadata), filesystemError) && !filesystemError)
        return true;
    if (reason != nullptr) *reason = CacheMissReason::IncompletePublication;
    return false;
}

bool collectPrunableEntries(
    const std::filesystem::path& directory,
    std::string_view payloadExtension,
    std::vector<CachePruneEntry>& entries,
    std::uintmax_t& totalBytes,
    std::string& error)
{
    std::error_code filesystemError;
    const bool exists = std::filesystem::is_directory(directory, filesystemError);
    if (filesystemError)
    {
        if (filesystemError == std::errc::no_such_file_or_directory) return true;
        error = "cannot inspect artifact cache for eviction: " + filesystemError.message();
        return false;
    }
    if (!exists) return true;
    std::filesystem::recursive_directory_iterator iterator(
        directory, std::filesystem::directory_options::skip_permission_denied, filesystemError);
    const std::filesystem::recursive_directory_iterator end;
    while (!filesystemError && iterator != end)
    {
        const std::filesystem::directory_entry entry = *iterator;
        iterator.increment(filesystemError);
        const bool regular = entry.is_regular_file(filesystemError);
        if (filesystemError) { filesystemError.clear(); continue; }
        if (!regular || entry.path().extension() != ".meta") continue;
        const std::string key = entry.path().stem().string();
        if (!frontend::internal::CacheEntryStore::validKey(key)) continue;
        std::filesystem::path payload = entry.path();
        payload.replace_extension(payloadExtension);
        const std::uintmax_t size = std::filesystem::file_size(payload, filesystemError);
        if (filesystemError) { filesystemError.clear(); continue; }
        const auto publication = entry.last_write_time(filesystemError);
        if (filesystemError) { filesystemError.clear(); continue; }
        entries.push_back({key, std::move(payload), entry.path(), publication, size});
        totalBytes += size;
    }
    if (!filesystemError) return true;
    error = "cannot enumerate artifact cache for eviction: " + filesystemError.message();
    return false;
}
}

std::string codeGenerationPolicyIdentity(LinkTimeOptimizationMode linkTimeOptimization)
{
    const FloatingPointState state = captureFloatingPointState();
    return "fp-mode=" + std::string(floatingPointModeName(state.mode)) +
        ";fma-contract=" + std::string(fmaContractPolicyName(state.fmaContractPolicy)) +
        ";rounding=" + std::string(floatingPointRoundingModeName(state.roundingMode)) +
        ";denormal=" + std::string(floatingPointDenormalModeName(state.denormalMode)) +
        ";lto=" + std::string(linkTimeOptimizationModeName(linkTimeOptimization));
}

const char* cacheMissReasonName(CacheMissReason reason) noexcept
{
    switch (reason)
    {
        case CacheMissReason::Hit: return "hit";
        case CacheMissReason::Disabled: return "disabled";
        case CacheMissReason::KeyMismatch: return "key-mismatch";
        case CacheMissReason::Cold: return "cold";
        case CacheMissReason::IncompletePublication: return "incomplete-publication";
        case CacheMissReason::Corruption: return "corruption";
    }
    return "unknown";
}

std::filesystem::path defaultProjectArtifactCacheRoot(
    const std::filesystem::path& projectFile)
{
    return (projectFile.lexically_normal().parent_path() /
        "obj" / "nativesharp" / "artifact-cache").lexically_normal();
}

void CompilationArtifactCache::setRoot(std::filesystem::path root)
{
    root_ = std::move(root).lexically_normal();
}

void CompilationArtifactCache::setCodegenIdentity(std::string identity)
{
    codegenIdentity_ = std::move(identity);
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
    std::string_view codeGenerationPolicy,
    LlvmOptimizationLevel optimization,
    DebugInformationKind debugInformation) const
{
    const LlvmVersion llvm = loadedLlvmVersion();
    std::string material = "NSCOBJKEY 4\n";
    material += "codegen " + codegenIdentity_ + "\n";
    material += "llvm " + llvmVersionString(llvm) + "\n";
    material += "target " + std::string(targetTriple) + "\n";
    material += "layout " + std::string(dataLayout) + "\n";
    material += "codegen " + std::string(codeGenerationPolicy) + "\n";
    material += "optimization " + std::string(llvmOptimizationLevelName(optimization)) + "\n";
    material += "pipeline " + std::string(llvmOptimizationPipelineName(optimization)) + "\n";
    material += "pipeline-version " +
        std::to_string(llvmOptimizationPipelineVersion(optimization)) + "\n";
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
    std::string_view codeGenerationPolicy,
    LlvmOptimizationLevel optimization,
    DebugInformationKind debugInformation) const
{
    const LlvmVersion llvm = loadedLlvmVersion();
    std::string material = "NSCOBJINPUTKEY 4\n";
    material += "codegen " + codegenIdentity_ + "\n";
    material += "llvm " + llvmVersionString(llvm) + "\n";
    material += "target " + std::string(targetIdentity) + "\n";
    material += "codegen " + std::string(codeGenerationPolicy) + "\n";
    material += "optimization " + std::string(llvmOptimizationLevelName(optimization)) + "\n";
    material += "pipeline " + std::string(llvmOptimizationPipelineName(optimization)) + "\n";
    material += "pipeline-version " +
        std::to_string(llvmOptimizationPipelineVersion(optimization)) + "\n";
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
    std::string& error,
    CacheMissReason* reason) const
{
    hit = false;
    error.clear();
    if (reason != nullptr) *reason = CacheMissReason::Cold;
    if (!enabled() || !frontend::internal::CacheEntryStore::validKey(key))
    {
        if (!enabled())
        {
            if (reason != nullptr) *reason = CacheMissReason::Disabled;
            return true;
        }
        if (reason != nullptr) *reason = CacheMissReason::KeyMismatch;
        error = "artifact-cache object key is not canonical SHA-256";
        return false;
    }
    const std::filesystem::path cachedObject = objectPath(root_, key);
    const std::filesystem::path cachedMetadata = metadataPath(root_, key);
    std::error_code filesystemError;
    if (!cacheEntryFilesAvailable(cachedObject, cachedMetadata, reason)) return true;

    CacheEntryLock entryLock;
    if (!entryLock.acquire(cachedObject.string() + ".lock", error)) return false;
    if (!cacheEntryFilesAvailable(cachedObject, cachedMetadata, reason)) return true;

    frontend::internal::CacheMetadata metadata;
    if (!objectEntryStore.readMetadata(cachedMetadata, metadata) || metadata.key != key)
    {
        if (reason != nullptr) *reason = CacheMissReason::Corruption;
        quarantineInvalidEntry(root_, key, cachedObject, cachedMetadata);
        return true;
    }
    basic::Sha256Digest digest{};
    std::uintmax_t size = 0;
    std::string hashError;
    if (!basic::sha256File(cacheFilesystemPath(cachedObject), digest, size, hashError)) return true;
    if (size != metadata.size || basic::sha256Hex(digest) != metadata.digest)
    {
        if (reason != nullptr) *reason = CacheMissReason::Corruption;
        quarantineInvalidEntry(root_, key, cachedObject, cachedMetadata);
        return true;
    }

    std::filesystem::copy_file(
        cacheFilesystemPath(cachedObject), cacheFilesystemPath(destination),
        std::filesystem::copy_options::overwrite_existing,
        filesystemError);
    if (filesystemError)
    {
        error = "cannot restore cached object '" + cachedObject.string() + "' to '" +
            destination.string() + "': " + filesystemError.message();
        return false;
    }
    hit = true;
    if (reason != nullptr) *reason = CacheMissReason::Hit;
    return true;
}

bool CompilationArtifactCache::storeObject(
    std::string_view key,
    const std::filesystem::path& source,
    std::string& error) const
{
    error.clear();
    if (!enabled()) return true;
    if (!frontend::internal::CacheEntryStore::validKey(key))
    {
        error = "artifact-cache object key is not canonical SHA-256";
        return false;
    }
    basic::Sha256Digest digest{};
    std::uintmax_t size = 0;
    if (!basic::sha256File(cacheFilesystemPath(source), digest, size, error)) return false;

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
    CacheEntryLock entryLock;
    if (!entryLock.acquire(cachedObject.string() + ".lock", error)) return false;
    const std::filesystem::path temporaryObject =
        objectEntryStore.temporarySibling(cachedObject);
    std::filesystem::copy_file(cacheFilesystemPath(source), cacheFilesystemPath(temporaryObject),
        std::filesystem::copy_options::overwrite_existing, filesystemError);
    if (filesystemError)
    {
        error = "cannot stage cached object '" + cachedObject.string() + "': " +
            filesystemError.message();
        return false;
    }
    const std::filesystem::path cachedMetadata = metadataPath(root_, key);
    std::filesystem::remove(cacheFilesystemPath(cachedMetadata), filesystemError);
    if (filesystemError)
    {
        std::error_code ignored;
        std::filesystem::remove(cacheFilesystemPath(temporaryObject), ignored);
        error = "cannot invalidate artifact-cache metadata '" + cachedMetadata.string() +
            "': " + filesystemError.message();
        return false;
    }
    filesystemError.clear();
    if (!objectEntryStore.replaceFileAtomically(temporaryObject, cachedObject, error)) return false;
    return objectEntryStore.writeMetadataAtomically(
        cachedMetadata, key, basic::sha256Hex(digest), size, error);
}

bool CompilationArtifactCache::restoreCodePartitionPlan(
    std::string_view key,
    std::string& payload,
    bool& hit,
    std::string& error) const
{
    hit = false;
    payload.clear();
    error.clear();
    if (!enabled()) return true;
    if (!frontend::internal::CacheEntryStore::validKey(key))
    {
        error = "code-partition plan key is not canonical SHA-256";
        return false;
    }
    const std::filesystem::path payloadFile = codePartitionPayloadPath(root_, key);
    const std::filesystem::path metadataFile = codePartitionMetadataPath(root_, key);
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(
            cacheFilesystemPath(payloadFile), filesystemError) || filesystemError)
        return true;

    CacheEntryLock entryLock;
    if (!entryLock.acquire(payloadFile.string() + ".lock", error)) return false;
    frontend::internal::CacheMetadata metadata;
    if (!codePartitionEntryStore.readMetadata(metadataFile, metadata) || metadata.key != key)
    {
        quarantineInvalidCodePartitionPlan(root_, key, payloadFile, metadataFile);
        return true;
    }
    const std::uintmax_t size =
        std::filesystem::file_size(cacheFilesystemPath(payloadFile), filesystemError);
    if (filesystemError || size != metadata.size ||
        size > std::numeric_limits<std::size_t>::max())
    {
        quarantineInvalidCodePartitionPlan(root_, key, payloadFile, metadataFile);
        return true;
    }
    std::ifstream stream(cacheFilesystemPath(payloadFile), std::ios::binary);
    payload.resize(static_cast<std::size_t>(size));
    if (!stream.read(payload.data(), static_cast<std::streamsize>(size)))
    {
        payload.clear();
        quarantineInvalidCodePartitionPlan(root_, key, payloadFile, metadataFile);
        return true;
    }
    const std::span<const std::uint8_t> bytes(
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size());
    if (basic::sha256Hex(basic::sha256(bytes)) != metadata.digest)
    {
        payload.clear();
        quarantineInvalidCodePartitionPlan(root_, key, payloadFile, metadataFile);
        return true;
    }
    hit = true;
    return true;
}

bool CompilationArtifactCache::storeCodePartitionPlan(
    std::string_view key,
    std::string_view payload,
    std::string& error) const
{
    error.clear();
    if (!enabled()) return true;
    if (!frontend::internal::CacheEntryStore::validKey(key))
    {
        error = "code-partition plan key is not canonical SHA-256";
        return false;
    }
    const std::filesystem::path payloadFile = codePartitionPayloadPath(root_, key);
    const std::filesystem::path metadataFile = codePartitionMetadataPath(root_, key);
    std::error_code filesystemError;
    std::filesystem::create_directories(payloadFile.parent_path(), filesystemError);
    if (filesystemError)
    {
        error = "cannot create code-partition cache directory '" +
            payloadFile.parent_path().string() + "': " + filesystemError.message();
        return false;
    }
    CacheEntryLock entryLock;
    if (!entryLock.acquire(payloadFile.string() + ".lock", error)) return false;
    const std::filesystem::path temporary =
        codePartitionEntryStore.temporarySibling(payloadFile);
    {
        std::ofstream stream(cacheFilesystemPath(temporary), std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "cannot open code-partition payload temporary '" + temporary.string() + "'";
            return false;
        }
        stream.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        if (!stream)
        {
            std::error_code ignored;
            std::filesystem::remove(cacheFilesystemPath(temporary), ignored);
            error = "cannot write code-partition payload '" + temporary.string() + "'";
            return false;
        }
    }
    std::filesystem::remove(cacheFilesystemPath(metadataFile), filesystemError);
    if (filesystemError)
    {
        std::error_code ignored;
        std::filesystem::remove(cacheFilesystemPath(temporary), ignored);
        error = "cannot invalidate code-partition metadata '" + metadataFile.string() +
            "': " + filesystemError.message();
        return false;
    }
    if (!codePartitionEntryStore.replaceFileAtomically(temporary, payloadFile, error)) return false;
    const std::span<const std::uint8_t> bytes(
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size());
    return codePartitionEntryStore.writeMetadataAtomically(
        metadataFile, key, basic::sha256Hex(basic::sha256(bytes)), payload.size(), error);
}

bool CompilationArtifactCache::prune(
    std::uintmax_t maximumBytes,
    std::size_t& evictedEntries,
    std::uintmax_t& evictedBytes,
    std::string& error) const
{
    const CachePruneMeasurement measurement(lastPruneTelemetry_);
    evictedEntries = 0;
    evictedBytes = 0;
    error.clear();
    if (!enabled()) return true;
    std::vector<CachePruneEntry> entries;
    std::uintmax_t totalBytes = 0;
    if (!collectPrunableEntries(root_ / "objects", ".obj", entries, totalBytes, error) ||
        !collectPrunableEntries(root_ / "partitions", ".plan", entries, totalBytes, error))
        return false;
    lastPruneTelemetry_.inspectedEntries = entries.size();
    lastPruneTelemetry_.inspectedBytes = totalBytes;
    std::ranges::sort(entries, [](const CachePruneEntry& left, const CachePruneEntry& right) {
        if (left.publication != right.publication) return left.publication < right.publication;
        return left.key < right.key;
    });
    std::error_code filesystemError;
    for (const CachePruneEntry& entry : entries)
    {
        if (totalBytes <= maximumBytes) break;
        CacheEntryLock entryLock;
        if (!entryLock.acquire(entry.payload.string() + ".lock", error)) return false;
        filesystemError.clear();
        const std::uintmax_t currentSize = std::filesystem::file_size(entry.payload, filesystemError);
        if (filesystemError)
        {
            filesystemError.clear();
            continue;
        }
        std::filesystem::remove(entry.metadata, filesystemError);
        if (filesystemError)
        {
            error = "cannot evict artifact-cache metadata '" + entry.metadata.string() +
                "': " + filesystemError.message();
            return false;
        }
        std::filesystem::remove(entry.payload, filesystemError);
        if (filesystemError)
        {
            error = "cannot evict artifact-cache payload '" + entry.payload.string() +
                "': " + filesystemError.message();
            return false;
        }
        totalBytes = currentSize > totalBytes ? 0 : totalBytes - currentSize;
        ++evictedEntries;
        evictedBytes += currentSize;
        lastPruneTelemetry_.evictedEntries = evictedEntries;
        lastPruneTelemetry_.evictedBytes = evictedBytes;
    }
    return true;
}

const CachePruneTelemetry& CompilationArtifactCache::lastPruneTelemetry() const noexcept
{
    return lastPruneTelemetry_;
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
