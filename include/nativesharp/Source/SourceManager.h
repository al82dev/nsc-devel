#pragma once

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceLocation.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Source/SourceFile.h"
#include "nativesharp/Source/SourcePosition.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::source
{
class SourceManager final
{
public:
    static std::optional<std::string> readFileBuffer(
        const std::filesystem::path& path,
        std::string& errorMessage);
    std::optional<FileId> loadFile(const std::filesystem::path& path, std::string& errorMessage);
    FileId addBuffer(std::filesystem::path path, std::string buffer);

    const SourceFile* getFile(FileId id) const noexcept;
    SourcePosition getPosition(SourceLocation location) const noexcept;
    std::string_view getText(SourceRange range) const noexcept;
    std::string_view getLineText(SourceLocation location) const noexcept;
    std::string_view getPath(SourceLocation location) const noexcept;

private:
    std::vector<SourceFile> files_;
};
}
