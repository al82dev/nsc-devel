#pragma once

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Source/SourcePosition.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::source
{
class SourceFile final
{
public:
    SourceFile(FileId id, std::filesystem::path path, std::string buffer);

    FileId id() const noexcept;
    const std::filesystem::path& path() const noexcept;
    std::string_view displayPath() const noexcept;
    std::string_view buffer() const noexcept;
    std::uint32_t size() const noexcept;
    std::uint32_t lineCount() const noexcept;

    SourcePosition positionForOffset(std::uint32_t offset) const noexcept;
    std::string_view lineText(std::uint32_t oneBasedLine) const noexcept;

private:
    void buildLineStarts();

    FileId id_;
    std::filesystem::path path_;
    std::string displayPath_;
    std::string buffer_;
    std::vector<std::uint32_t> lineStarts_;
};
}
