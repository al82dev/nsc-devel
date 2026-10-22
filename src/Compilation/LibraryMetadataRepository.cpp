#include "nativesharp/Compilation/LibraryMetadataRepository.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace nativesharp::compilation
{
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

const interop::LibraryMetadata* LibraryMetadataRepository::decode(
    const std::filesystem::path& path,
    const Decoder& decoder,
    std::string& error) const
{
    const LibraryMetadataRepositoryEntry* entry = load(path, error);
    if (entry == nullptr) return nullptr;
    if (entry->decodeAttempted_)
    {
        error = entry->decodeError_;
        return entry->decodedMetadata_.get();
    }
    entry->decodeAttempted_ = true;
    interop::LibraryMetadata metadata;
    if (!decoder(entry->bytes(), metadata, error))
    {
        entry->decodeError_ = error;
        return nullptr;
    }
    entry->decodedMetadata_ =
        std::make_shared<const interop::LibraryMetadata>(std::move(metadata));
    error.clear();
    return entry->decodedMetadata_.get();
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
    entry->verified_ = verifier(*entry->decodedMetadata_, error);
    entry->verificationError_ = error;
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
