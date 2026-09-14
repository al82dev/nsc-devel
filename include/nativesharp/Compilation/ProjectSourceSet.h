#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::compilation
{
struct ProjectSource final
{
    std::filesystem::path absolutePath;
    std::filesystem::path projectRelativePath;
    std::size_t ordinal = 0;
};

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
    std::vector<ProjectSource> sources_;
    bool finalized_ = false;
};
}
