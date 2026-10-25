#include "SourceDependencyIndex.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Source/SourceManager.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace nativesharp::llvm_backend
{
namespace
{
constexpr std::string_view kManifestHeader = "NativeSharp.SourceDependencies.v1\n";

std::string digestText(std::string_view text)
{
    return basic::sha256Hex(basic::sha256(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(text.data()), text.size())));
}
}

SourceDependencyIndex::SourceDependencyIndex(const source::SourceManager& sourceManager)
    : sourceManager_(sourceManager)
{
    // Index 0 is the invalid id; files are numbered from 1 until getFile returns null.
    // El indice 0 es el id invalido; los archivos se numeran desde 1 hasta que getFile da null.
    paths_.emplace_back();
    for (std::uint32_t value = 1;; ++value)
    {
        const source::SourceFile* file = sourceManager_.getFile(FileId(value));
        if (file == nullptr) break;
        paths_.push_back(file->path().lexically_normal().generic_string());
        filesByPath_.emplace(paths_.back(), FileId(value));
    }
    digestOnce_ = std::make_unique<std::once_flag[]>(paths_.size());
    digests_.resize(paths_.size());
}

std::string SourceDependencyIndex::manifestKey(std::string_view unitKey)
{
    return digestText(std::string(kManifestHeader) + std::string(unitKey));
}

std::string SourceDependencyIndex::manifestText(std::span<const FileId> files) const
{
    std::vector<std::string> paths;
    for (const FileId file : files)
        if (file.value() < paths_.size()) paths.push_back(paths_[file.value()]);
    std::ranges::sort(paths);
    std::string text(kManifestHeader);
    for (const std::string& path : paths) text += path + "\n";
    return text;
}

bool SourceDependencyIndex::manifestFiles(std::string_view text, std::vector<FileId>& files) const
{
    files.clear();
    if (!text.starts_with(kManifestHeader)) return false;
    std::size_t begin = kManifestHeader.size();
    while (begin < text.size())
    {
        const std::size_t end = text.find('\n', begin);
        if (end == std::string_view::npos) return false;
        const auto found = filesByPath_.find(std::string(text.substr(begin, end - begin)));
        if (found == filesByPath_.end()) return false;
        files.push_back(found->second);
        begin = end + 1;
    }
    return true;
}

std::string SourceDependencyIndex::objectKey(std::string_view unitKey, std::span<const FileId> files) const
{
    std::vector<std::pair<std::string, std::uint32_t>> sorted;
    for (const FileId file : files)
    {
        if (file.value() > 0 && file.value() < paths_.size()) sorted.emplace_back(paths_[file.value()], file.value());
    }
    std::ranges::sort(sorted);
    std::string material = "NativeSharp.SourceBoundObject.v1\n";
    material += std::string(unitKey) + "\n";
    for (const auto& [path, value] : sorted)
    {
        const FileId file(value);
        const source::SourceFile* source = sourceManager_.getFile(file);
        material += path + "\n" + std::to_string(source != nullptr ? source->buffer().size() : 0) + "\n";
        material += digest(file) + "\n";
    }
    return digestText(material);
}

const std::string& SourceDependencyIndex::digest(FileId file) const
{
    std::call_once(digestOnce_[file.value()], &SourceDependencyIndex::computeDigest, this, file);
    return digests_[file.value()];
}

void SourceDependencyIndex::computeDigest(FileId file) const
{
    const source::SourceFile* source = sourceManager_.getFile(file);
    if (source != nullptr) digests_[file.value()] = digestText(source->buffer());
}
}
