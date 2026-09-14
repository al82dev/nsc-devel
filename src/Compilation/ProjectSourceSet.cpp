#include "nativesharp/Compilation/ProjectSourceSet.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace nativesharp::compilation
{
ProjectSourceSet::ProjectSourceSet(std::filesystem::path projectDirectory)
    : projectDirectory_(std::filesystem::absolute(std::move(projectDirectory)).lexically_normal())
{
}

bool ProjectSourceSet::add(const std::filesystem::path& sourcePath, std::string& error)
{
    if (finalized_)
    {
        error = "cannot add a source after the project source set was finalized";
        return false;
    }

    std::error_code filesystemError;
    const std::filesystem::path canonicalProject =
        std::filesystem::weakly_canonical(projectDirectory_, filesystemError);
    if (filesystemError || !std::filesystem::is_directory(canonicalProject))
    {
        error = "project directory does not exist: '" + projectDirectory_.string() + "'";
        return false;
    }

    const std::filesystem::path unresolved = sourcePath.is_absolute()
        ? sourcePath
        : canonicalProject / sourcePath;
    const std::filesystem::path canonicalSource =
        std::filesystem::weakly_canonical(unresolved, filesystemError);
    if (filesystemError || !std::filesystem::is_regular_file(canonicalSource))
    {
        error = "project source does not exist or is not a regular file: '" + unresolved.string() + "'";
        return false;
    }

    const std::filesystem::path relative =
        std::filesystem::relative(canonicalSource, canonicalProject, filesystemError).lexically_normal();
    if (filesystemError || relative.empty() || escapesProject(relative))
    {
        error = "project source escapes the project directory: '" + canonicalSource.string() + "'";
        return false;
    }

    if (relative.extension() != ".ns")
    {
        error = "project source must use the canonical .ns extension: '" + relative.generic_string() + "'";
        return false;
    }

    const std::string candidateKey = comparisonKey(relative);
    const auto duplicate = std::find_if(
        sources_.begin(),
        sources_.end(),
        [&candidateKey](const ProjectSource& source)
        {
            return comparisonKey(source.projectRelativePath) == candidateKey;
        });
    if (duplicate != sources_.end())
    {
        error = "duplicate project source: '" + relative.generic_string() + "'";
        return false;
    }

    sources_.push_back(ProjectSource{canonicalSource, relative, 0});
    return true;
}

bool ProjectSourceSet::finalize(std::string& error, bool allowEmpty)
{
    if (finalized_)
    {
        return true;
    }
    if (sources_.empty() && !allowEmpty)
    {
        error = "a NativeSharp project must contain at least one .ns source";
        return false;
    }

    std::sort(
        sources_.begin(),
        sources_.end(),
        [](const ProjectSource& left, const ProjectSource& right)
        {
            const std::string leftKey = comparisonKey(left.projectRelativePath);
            const std::string rightKey = comparisonKey(right.projectRelativePath);
            if (leftKey != rightKey)
            {
                return leftKey < rightKey;
            }
            return left.projectRelativePath.generic_string() < right.projectRelativePath.generic_string();
        });

    for (std::size_t index = 0; index < sources_.size(); ++index)
    {
        sources_[index].ordinal = index;
    }
    finalized_ = true;
    return true;
}

const std::filesystem::path& ProjectSourceSet::projectDirectory() const noexcept
{
    return projectDirectory_;
}

std::span<const ProjectSource> ProjectSourceSet::sources() const noexcept
{
    return sources_;
}

bool ProjectSourceSet::isFinalized() const noexcept
{
    return finalized_;
}

std::string ProjectSourceSet::comparisonKey(const std::filesystem::path& path)
{
    std::string key = path.generic_string();
    std::transform(
        key.begin(),
        key.end(),
        key.begin(),
        [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        });
    return key;
}

bool ProjectSourceSet::escapesProject(const std::filesystem::path& relativePath) noexcept
{
    const auto first = relativePath.begin();
    return first != relativePath.end() && *first == "..";
}
}
