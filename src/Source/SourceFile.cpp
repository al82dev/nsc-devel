#include "nativesharp/Source/SourceFile.h"

#include <algorithm>
#include <limits>

namespace nativesharp::source
{
SourceFile::SourceFile(FileId id, std::filesystem::path path, std::string buffer)
    : id_(id),
      path_(std::move(path)),
      displayPath_(path_.string()),
      buffer_(std::move(buffer))
{
    buildLineStarts();
}

FileId SourceFile::id() const noexcept
{
    return id_;
}

const std::filesystem::path& SourceFile::path() const noexcept
{
    return path_;
}

std::string_view SourceFile::displayPath() const noexcept
{
    return displayPath_;
}

std::string_view SourceFile::buffer() const noexcept
{
    return buffer_;
}

std::uint32_t SourceFile::size() const noexcept
{
    if (buffer_.size() > std::numeric_limits<std::uint32_t>::max())
    {
        return std::numeric_limits<std::uint32_t>::max();
    }

    return static_cast<std::uint32_t>(buffer_.size());
}

std::uint32_t SourceFile::lineCount() const noexcept
{
    return static_cast<std::uint32_t>(lineStarts_.size());
}

SourcePosition SourceFile::positionForOffset(std::uint32_t offset) const noexcept
{
    const std::uint32_t clamped = std::min(offset, size());
    const auto upper = std::upper_bound(lineStarts_.begin(), lineStarts_.end(), clamped);
    const std::size_t lineIndex = upper == lineStarts_.begin()
        ? 0
        : static_cast<std::size_t>(upper - lineStarts_.begin() - 1);
    const std::uint32_t lineStart = lineStarts_[lineIndex];

    return SourcePosition{
        static_cast<std::uint32_t>(lineIndex + 1),
        static_cast<std::uint32_t>(clamped - lineStart + 1)};
}

std::string_view SourceFile::lineText(std::uint32_t oneBasedLine) const noexcept
{
    if (oneBasedLine == 0 || oneBasedLine > lineStarts_.size())
    {
        return {};
    }

    const std::size_t index = static_cast<std::size_t>(oneBasedLine - 1);
    const std::uint32_t begin = lineStarts_[index];
    std::uint32_t end = size();

    if (index + 1 < lineStarts_.size())
    {
        end = lineStarts_[index + 1];
    }

    while (end > begin && (buffer_[end - 1] == '\n' || buffer_[end - 1] == '\r'))
    {
        --end;
    }

    return std::string_view(buffer_).substr(begin, end - begin);
}

void SourceFile::buildLineStarts()
{
    lineStarts_.clear();
    lineStarts_.push_back(0);

    std::size_t index = 0;
    while (index < buffer_.size())
    {
        if (buffer_[index] == '\r')
        {
            ++index;
            if (index < buffer_.size() && buffer_[index] == '\n')
            {
                ++index;
            }

            lineStarts_.push_back(static_cast<std::uint32_t>(index));
            continue;
        }

        if (buffer_[index] == '\n')
        {
            ++index;
            lineStarts_.push_back(static_cast<std::uint32_t>(index));
            continue;
        }

        ++index;
    }
}
}
