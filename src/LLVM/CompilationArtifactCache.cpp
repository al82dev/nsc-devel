#include "nativesharp/LLVM/CompilationArtifactCache.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/FloatingPointMode.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmVersion.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <limits>
#include <mutex>
#include <span>
#include <system_error>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace nativesharp::llvm_backend
{
namespace
{
thread_local const CompilationArtifactCache* activeCache = nullptr;
std::atomic<unsigned long long> temporaryOrdinal = 0;

std::filesystem::path temporarySibling(const std::filesystem::path& destination)
{
#if defined(_WIN32)
    const unsigned long process = ::GetCurrentProcessId();
#else
    const unsigned long process = 0;
#endif
    return destination.string() + ".tmp." + std::to_string(process) + "." +
        std::to_string(temporaryOrdinal.fetch_add(1, std::memory_order_relaxed));
}

bool replaceFileAtomically(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination,
    std::string& error)
{
#if defined(_WIN32)
    if (::MoveFileExW(temporary.c_str(), destination.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0)
        return true;
    const unsigned long failure = ::GetLastError();
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    error = "cannot atomically publish artifact-cache file '" + destination.string() +
        "' (Win32 error " + std::to_string(failure) + ")";
    return false;
#else
    std::error_code filesystemError;
    std::filesystem::rename(temporary, destination, filesystemError);
    if (!filesystemError) return true;
    std::filesystem::remove(temporary, filesystemError);
    error = "cannot atomically publish artifact-cache file '" + destination.string() + "'";
    return false;
#endif
}

class CacheEntryLock final
{
public:
    CacheEntryLock() = default;
    CacheEntryLock(const CacheEntryLock&) = delete;
    CacheEntryLock& operator=(const CacheEntryLock&) = delete;

    ~CacheEntryLock()
    {
#if defined(_WIN32)
        if (handle_ != INVALID_HANDLE_VALUE) ::CloseHandle(handle_);
        std::error_code ignored;
        if (!path_.empty()) std::filesystem::remove(path_, ignored);
#endif
    }

    bool acquire(const std::filesystem::path& path, std::string& error)
    {
#if defined(_WIN32)
        path_ = path;
        constexpr unsigned attempts = 3000;
        for (unsigned attempt = 0; attempt < attempts; ++attempt)
        {
            handle_ = ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
            if (handle_ != INVALID_HANDLE_VALUE) return true;
            const unsigned long failure = ::GetLastError();
            if (failure != ERROR_SHARING_VIOLATION && failure != ERROR_LOCK_VIOLATION)
            {
                error = "cannot lock artifact-cache entry '" + path.string() +
                    "' (Win32 error " + std::to_string(failure) + ")";
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        error = "timed out waiting for artifact-cache entry '" + path.string() + "'";
        return false;
#else
        lock_ = std::unique_lock(globalMutex_);
        (void)path;
        (void)error;
        return true;
#endif
    }

private:
#if defined(_WIN32)
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::filesystem::path path_;
#else
    inline static std::mutex globalMutex_;
    std::unique_lock<std::mutex> lock_;
#endif
};

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
    std::filesystem::rename(metadata, quarantinedMetadata, filesystemError);
    filesystemError.clear();
    std::filesystem::rename(object, quarantinedObject, filesystemError);
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
        metadata, quarantine / (std::string(key) + "." + suffix + ".partition.meta"),
        filesystemError);
    filesystemError.clear();
    std::filesystem::rename(
        payload, quarantine / (std::string(key) + "." + suffix + ".partition.plan"),
        filesystemError);
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
    const std::filesystem::path temporary = temporarySibling(destination);
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
    return replaceFileAtomically(temporary, destination, error);
}

bool readCodePartitionMetadata(const std::filesystem::path& path, CacheMetadata& metadata)
{
    std::ifstream stream(path, std::ios::binary);
    std::string header;
    std::string keyLine;
    std::string digestLine;
    std::string sizeLine;
    std::string trailing;
    if (!stream || !std::getline(stream, header) || !std::getline(stream, keyLine) ||
        !std::getline(stream, digestLine) || !std::getline(stream, sizeLine) ||
        std::getline(stream, trailing) || header != "NSCPART 1" ||
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

bool writeCodePartitionMetadataAtomically(
    const std::filesystem::path& destination,
    std::string_view key,
    std::string_view digest,
    std::uintmax_t size,
    std::string& error)
{
    const std::filesystem::path temporary = temporarySibling(destination);
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "cannot open code-partition metadata temporary '" +
                temporary.string() + "'";
            return false;
        }
        stream << "NSCPART 1\nkey " << key << "\nsha256 " << digest <<
            "\nsize " << size << '\n';
        if (!stream)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            error = "cannot write code-partition metadata '" + temporary.string() + "'";
            return false;
        }
    }
    return replaceFileAtomically(temporary, destination, error);
}

struct CachePruneEntry final
{
    std::string key;
    std::filesystem::path payload;
    std::filesystem::path metadata;
    std::filesystem::file_time_type publication;
    std::uintmax_t size = 0;
};

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
        if (!validKey(key)) continue;
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
    if (!enabled() || !validKey(key))
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
    if (!std::filesystem::is_regular_file(cachedObject, filesystemError) || filesystemError)
        return true;
    filesystemError.clear();
    if (!std::filesystem::is_regular_file(cachedMetadata, filesystemError) || filesystemError)
    {
        if (reason != nullptr) *reason = CacheMissReason::IncompletePublication;
        return true;
    }

    CacheEntryLock entryLock;
    if (!entryLock.acquire(cachedObject.string() + ".lock", error)) return false;
    filesystemError.clear();
    if (!std::filesystem::is_regular_file(cachedObject, filesystemError) || filesystemError)
        return true;
    filesystemError.clear();
    if (!std::filesystem::is_regular_file(cachedMetadata, filesystemError) || filesystemError)
    {
        if (reason != nullptr) *reason = CacheMissReason::IncompletePublication;
        return true;
    }

    CacheMetadata metadata;
    if (!readMetadata(cachedMetadata, metadata) || metadata.key != key)
    {
        if (reason != nullptr) *reason = CacheMissReason::Corruption;
        quarantineInvalidEntry(root_, key, cachedObject, cachedMetadata);
        return true;
    }
    basic::Sha256Digest digest{};
    std::uintmax_t size = 0;
    std::string hashError;
    if (!basic::sha256File(cachedObject, digest, size, hashError)) return true;
    if (size != metadata.size || basic::sha256Hex(digest) != metadata.digest)
    {
        if (reason != nullptr) *reason = CacheMissReason::Corruption;
        quarantineInvalidEntry(root_, key, cachedObject, cachedMetadata);
        return true;
    }

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
    CacheEntryLock entryLock;
    if (!entryLock.acquire(cachedObject.string() + ".lock", error)) return false;
    const std::filesystem::path temporaryObject = temporarySibling(cachedObject);
    std::filesystem::copy_file(source, temporaryObject,
        std::filesystem::copy_options::overwrite_existing, filesystemError);
    if (filesystemError)
    {
        error = "cannot stage cached object '" + cachedObject.string() + "': " +
            filesystemError.message();
        return false;
    }
    const std::filesystem::path cachedMetadata = metadataPath(root_, key);
    std::filesystem::remove(cachedMetadata, filesystemError);
    if (filesystemError)
    {
        std::error_code ignored;
        std::filesystem::remove(temporaryObject, ignored);
        error = "cannot invalidate artifact-cache metadata '" + cachedMetadata.string() +
            "': " + filesystemError.message();
        return false;
    }
    filesystemError.clear();
    if (!replaceFileAtomically(temporaryObject, cachedObject, error)) return false;
    return writeMetadataAtomically(
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
    if (!validKey(key))
    {
        error = "code-partition plan key is not canonical SHA-256";
        return false;
    }
    const std::filesystem::path payloadFile = codePartitionPayloadPath(root_, key);
    const std::filesystem::path metadataFile = codePartitionMetadataPath(root_, key);
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(payloadFile, filesystemError) || filesystemError)
        return true;

    CacheEntryLock entryLock;
    if (!entryLock.acquire(payloadFile.string() + ".lock", error)) return false;
    CacheMetadata metadata;
    if (!readCodePartitionMetadata(metadataFile, metadata) || metadata.key != key)
    {
        quarantineInvalidCodePartitionPlan(root_, key, payloadFile, metadataFile);
        return true;
    }
    const std::uintmax_t size = std::filesystem::file_size(payloadFile, filesystemError);
    if (filesystemError || size != metadata.size ||
        size > std::numeric_limits<std::size_t>::max())
    {
        quarantineInvalidCodePartitionPlan(root_, key, payloadFile, metadataFile);
        return true;
    }
    std::ifstream stream(payloadFile, std::ios::binary);
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
    if (!validKey(key))
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
    const std::filesystem::path temporary = temporarySibling(payloadFile);
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "cannot open code-partition payload temporary '" + temporary.string() + "'";
            return false;
        }
        stream.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        if (!stream)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            error = "cannot write code-partition payload '" + temporary.string() + "'";
            return false;
        }
    }
    std::filesystem::remove(metadataFile, filesystemError);
    if (filesystemError)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        error = "cannot invalidate code-partition metadata '" + metadataFile.string() +
            "': " + filesystemError.message();
        return false;
    }
    if (!replaceFileAtomically(temporary, payloadFile, error)) return false;
    const std::span<const std::uint8_t> bytes(
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size());
    return writeCodePartitionMetadataAtomically(
        metadataFile, key, basic::sha256Hex(basic::sha256(bytes)), payload.size(), error);
}

bool CompilationArtifactCache::prune(
    std::uintmax_t maximumBytes,
    std::size_t& evictedEntries,
    std::uintmax_t& evictedBytes,
    std::string& error) const
{
    evictedEntries = 0;
    evictedBytes = 0;
    error.clear();
    if (!enabled()) return true;
    std::vector<CachePruneEntry> entries;
    std::uintmax_t totalBytes = 0;
    if (!collectPrunableEntries(root_ / "objects", ".obj", entries, totalBytes, error) ||
        !collectPrunableEntries(root_ / "partitions", ".plan", entries, totalBytes, error))
        return false;
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
    }
    return true;
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
