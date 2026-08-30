#include "nativesharp/Source/SourceManager.h"

#include <fstream>
#include <iterator>
#include <limits>

namespace nativesharp::source
{
std::optional<FileId> SourceManager::loadFile(
    const std::filesystem::path& path,
    std::string& errorMessage)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        errorMessage = "cannot open source file '" + path.string() + "'";
        return std::nullopt;
    }

    std::string buffer{
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};

    if (!stream.good() && !stream.eof())
    {
        errorMessage = "cannot read source file '" + path.string() + "'";
        return std::nullopt;
    }

    if (buffer.size() > std::numeric_limits<std::uint32_t>::max())
    {
        errorMessage = "source file is larger than the stage 10 32-bit source-offset limit";
        return std::nullopt;
    }

    return addBuffer(path, std::move(buffer));
}

FileId SourceManager::addBuffer(std::filesystem::path path, std::string buffer)
{
    const FileId id(static_cast<std::uint32_t>(files_.size() + 1));
    files_.emplace_back(id, std::move(path), std::move(buffer));
    return id;
}

const SourceFile* SourceManager::getFile(FileId id) const noexcept
{
    if (!id.isValid() || id.value() > files_.size())
    {
        return nullptr;
    }

    return &files_[id.value() - 1];
}

SourcePosition SourceManager::getPosition(SourceLocation location) const noexcept
{
    const SourceFile* file = getFile(location.fileId());
    if (file == nullptr)
    {
        return {};
    }

    return file->positionForOffset(location.offset());
}

std::string_view SourceManager::getText(SourceRange range) const noexcept
{
    if (!range.isValid())
    {
        return {};
    }

    const SourceFile* file = getFile(range.begin().fileId());
    if (file == nullptr || range.end().offset() > file->size())
    {
        return {};
    }

    return file->buffer().substr(
        range.begin().offset(),
        range.end().offset() - range.begin().offset());
}

std::string_view SourceManager::getLineText(SourceLocation location) const noexcept
{
    const SourceFile* file = getFile(location.fileId());
    if (file == nullptr)
    {
        return {};
    }

    return file->lineText(getPosition(location).line);
}

std::string_view SourceManager::getPath(SourceLocation location) const noexcept
{
    const SourceFile* file = getFile(location.fileId());
    if (file == nullptr)
    {
        return {};
    }

    return file->displayPath();
}
}
