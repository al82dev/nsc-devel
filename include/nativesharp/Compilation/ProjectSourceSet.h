#pragma once

#include "nativesharp/Compilation/ProjectSource.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace nativesharp::compilation
{
class ProjectSourceSet final
{
public:
    explicit ProjectSourceSet(std::filesystem::path projectDirectory);

    bool add(const std::filesystem::path& sourcePath, std::string& error);
    bool finalize(std::string& error, bool allowEmpty = false);

    const std::filesystem::path& projectDirectory() const noexcept;
    std::span<const ProjectSource> sources() const noexcept;
    bool isFinalized() const noexcept;

private:
    static std::string comparisonKey(const std::filesystem::path& path);
    static bool escapesProject(const std::filesystem::path& relativePath) noexcept;

    std::filesystem::path projectDirectory_;
    // Canonical form of projectDirectory_, resolved once by the first add that finds the
    // directory; later adds reuse it instead of canonicalizing the directory again.
    std::optional<std::filesystem::path> canonicalProjectDirectory_;
    std::vector<ProjectSource> sources_;
    // comparisonKey of every accepted source, for the duplicate check.
    std::unordered_set<std::string> sourceKeys_;
    bool finalized_ = false;
};
}
