#pragma once

#include "WriterWin32Handle.h"
#include "WriterWin32Path.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace nsc::suite::writer
{
class CaseTree
{
public:
    // Creates a unique fixture root and records every leaf for explicit non-recursive cleanup.
    explicit CaseTree(std::string_view caseId)
        : root_(std::filesystem::current_path() /
                (".nsc-suite-" + std::string(caseId) + "-" +
                 std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64())))
    {
        if (!std::filesystem::create_directory(root_))
        {
            throw std::runtime_error("cannot create transactional-writer fixture root");
        }
        tracked_.push_back(extendedPath(root_));
    }

    CaseTree(const CaseTree&) = delete;
    CaseTree& operator=(const CaseTree&) = delete;

    // Removes tracked files before their parent directory without allocating or throwing.
    ~CaseTree() noexcept
    {
        std::sort(tracked_.begin(), tracked_.end(), longerPathFirst);
        for (const auto& extended : tracked_)
        {
            const DWORD attributes = GetFileAttributesW(extended.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES)
            {
                continue;
            }
            if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                RemoveDirectoryW(extended.c_str());
            }
            else
            {
                DeleteFileW(extended.c_str());
            }
        }
    }

    // Returns and tracks one direct child path without creating it.
    [[nodiscard]] std::filesystem::path path(std::string_view name)
    {
        const std::filesystem::path value = root_ / std::string(name);
        tracked_.push_back(extendedPath(value));
        return value;
    }

private:
    std::filesystem::path root_;
    std::vector<std::filesystem::path> tracked_;
};
}
