#include "nativesharp/Compilation/LibraryMetadataRepository.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace nativesharp::compilation
{
namespace
{
std::string verifiedCacheContract()
{
    std::string material = "nsc-verified-nsmeta-verification-contract-v1\n";
    material += kCompilerBuildId;
    material.push_back('\n');
    material += std::to_string(interop::kMetadataFormatMajor);
    material.push_back('.');
    material += std::to_string(interop::kMetadataFormatMinor);
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size()};
    return basic::sha256Hex(basic::sha256(bytes));
}

std::string verifiedCacheKey(const LibraryMetadataRepositoryEntry& entry)
{
    std::string material = verifiedCacheContract();
    material.push_back('\n');
    material += std::to_string(entry.bytes().size());
    material.push_back('\n');
    material += basic::sha256Hex(entry.contentDigest());
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size()};
    return basic::sha256Hex(basic::sha256(bytes));
}
}

void LibraryMetadataRepository::setCacheRoot(std::filesystem::path root)
{
    verifiedCache_.setRoot(std::move(root));
}

LibraryMetadataRepositoryEntry& LibraryMetadataRepository::entryFor(
    const std::filesystem::path& path)
{
    const std::filesystem::path canonical = canonicalPath(path);
    const std::string key = pathKey(canonical);
    auto [iterator, inserted] = entries_.try_emplace(key);
    if (inserted)
    {
        iterator->second = std::make_unique<LibraryMetadataRepositoryEntry>(canonical);
    }
    return *iterator->second;
}

const LibraryMetadataRepositoryEntry* LibraryMetadataRepository::load(
    const std::filesystem::path& path,
    std::string& error) const
{
    const std::filesystem::path canonical = canonicalPath(path);
    const std::string key = pathKey(canonical);
    auto [iterator, inserted] = entries_.try_emplace(key);
    if (inserted)
    {
        iterator->second = std::make_unique<LibraryMetadataRepositoryEntry>(canonical);
    }
    return iterator->second->load(error) ? iterator->second.get() : nullptr;
}

std::shared_ptr<const interop::LibraryMetadata> LibraryMetadataRepository::decode(
    const std::filesystem::path& path,
    const Decoder& decoder,
    std::string& error) const
{
    const LibraryMetadataRepositoryEntry* entry = load(path, error);
    if (entry == nullptr) return {};
    if (entry->decodeAttempted_)
    {
        error = entry->decodeError_;
        return entry->decodedMetadata_;
    }
    entry->decodeAttempted_ = true;
    entry->persistentCacheKey_ = verifiedCacheKey(*entry);
    std::vector<std::uint8_t> restoredBytes;
    bool restored = false;
    std::string ignoredCacheError;
    const bool cacheReadable = verifiedCache_.restore(
        entry->persistentCacheKey_, restoredBytes, restored, ignoredCacheError);
    const bool restoredContentMatches =
        restored && restoredBytes.size() == entry->bytes().size() &&
        basic::sha256(restoredBytes) == entry->contentDigest();
    if (cacheReadable && restoredContentMatches)
    {
        entry->restoredVerified_ = true;
    }
    else if (restored)
    {
        verifiedCache_.quarantine(entry->persistentCacheKey_);
    }
    interop::LibraryMetadata metadata;
    if (entry->restoredVerified_ && !decoder(restoredBytes, metadata, ignoredCacheError))
    {
        verifiedCache_.quarantine(entry->persistentCacheKey_);
        entry->restoredVerified_ = false;
    }
    if (!entry->restoredVerified_ && !decoder(entry->bytes(), metadata, error))
    {
        entry->decodeError_ = error;
        return {};
    }
    entry->decodedMetadata_ =
        std::make_shared<const interop::LibraryMetadata>(std::move(metadata));
    error.clear();
    return entry->decodedMetadata_;
}

bool LibraryMetadataRepository::verify(
    const std::filesystem::path& path,
    const Verifier& verifier,
    std::string& error) const
{
    const LibraryMetadataRepositoryEntry* entry = find(path);
    if (entry == nullptr || entry->decodedMetadata_ == nullptr)
    {
        error = entry == nullptr ? std::string{} : entry->decodeError_;
        return false;
    }
    if (entry->verificationAttempted_)
    {
        error = entry->verificationError_;
        return entry->verified_;
    }
    entry->verificationAttempted_ = true;
    if (entry->restoredVerified_)
    {
        entry->verified_ = true;
        entry->verificationError_.clear();
        error.clear();
        return true;
    }
    entry->verified_ = verifier(*entry->decodedMetadata_, error);
    entry->verificationError_ = error;
    if (entry->verified_)
    {
        std::string ignoredCacheError;
        (void)verifiedCache_.store(
            entry->persistentCacheKey_, entry->bytes(), ignoredCacheError);
    }
    return entry->verified_;
}

const LibraryMetadataRepositoryEntry* LibraryMetadataRepository::find(
    const std::filesystem::path& path) const
{
    const auto iterator = entries_.find(pathKey(canonicalPath(path)));
    return iterator == entries_.end() ? nullptr : iterator->second.get();
}

std::size_t LibraryMetadataRepository::size() const noexcept
{
    return entries_.size();
}

void LibraryMetadataRepository::clear() noexcept
{
    entries_.clear();
}

std::filesystem::path LibraryMetadataRepository::canonicalPath(
    const std::filesystem::path& path)
{
    std::error_code filesystemError;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, filesystemError);
    if (!filesystemError) return canonical;

    filesystemError.clear();
    canonical = std::filesystem::absolute(path, filesystemError);
    return (filesystemError ? path : canonical).lexically_normal();
}

std::string LibraryMetadataRepository::pathKey(const std::filesystem::path& path)
{
    std::string key = path.generic_string();
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return key;
}
}
