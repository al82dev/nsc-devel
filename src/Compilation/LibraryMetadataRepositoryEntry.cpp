#include "nativesharp/Compilation/LibraryMetadataRepositoryEntry.h"

#include <fstream>
#include <utility>

namespace nativesharp::compilation
{
LibraryMetadataRepositoryEntry::LibraryMetadataRepositoryEntry(std::filesystem::path path)
    : path_(std::move(path))
{
}

bool LibraryMetadataRepositoryEntry::load(std::string& error)
{
    if (loadAttempted_)
    {
        error = loadError_;
        return loadError_.empty();
    }
    loadAttempted_ = true;
    std::ifstream stream(path_, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        loadError_ = "cannot open NativeSharp metadata '" + path_.string() + "'";
        error = loadError_;
        return false;
    }
    const std::streampos end = stream.tellg();
    constexpr std::uint64_t kMaximumFileSize = 100ULL + 128ULL * 1024ULL * 1024ULL;
    if (end < 0 || static_cast<std::uint64_t>(end) > kMaximumFileSize)
    {
        loadError_ = "NativeSharp metadata file has invalid size";
        error = loadError_;
        return false;
    }
    bytes_.resize(static_cast<std::size_t>(end));
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes_.data()), static_cast<std::streamsize>(bytes_.size()));
    if (!stream && !bytes_.empty())
    {
        loadError_ = "failed while reading NativeSharp metadata";
        error = loadError_;
        bytes_.clear();
        return false;
    }
    contentDigest_ = basic::sha256(bytes_);
    error.clear();
    return true;
}

const std::filesystem::path& LibraryMetadataRepositoryEntry::path() const noexcept
{
    return path_;
}

std::span<const std::uint8_t> LibraryMetadataRepositoryEntry::bytes() const noexcept
{
    return bytes_;
}

const basic::Sha256Digest& LibraryMetadataRepositoryEntry::contentDigest() const noexcept
{
    return contentDigest_;
}
}
