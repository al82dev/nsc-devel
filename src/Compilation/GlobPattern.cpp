#include "nativesharp/Compilation/GlobPattern.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <vector>

namespace nativesharp::compilation
{
GlobPattern::GlobPattern(std::string pattern)
    : pattern_(std::move(pattern))
{
    std::replace(pattern_.begin(), pattern_.end(), '\\', '/');
    segments_ = split(pattern_);
    validate();
}

bool GlobPattern::isValid() const noexcept
{
    return validationError_.empty();
}

const std::string& GlobPattern::validationError() const noexcept
{
    return validationError_;
}

const std::string& GlobPattern::pattern() const noexcept
{
    return pattern_;
}

bool GlobPattern::matches(const std::filesystem::path& projectRelativePath) const
{
    if (!isValid())
    {
        return false;
    }
    const std::vector<std::string> pathSegments = split(projectRelativePath.generic_string());
    std::vector<std::vector<int>> memo(
        segments_.size() + 1,
        std::vector<int>(pathSegments.size() + 1, -1));

    std::function<bool(std::size_t, std::size_t)> match =
        [&](std::size_t patternIndex, std::size_t pathIndex)
        {
            int& cached = memo[patternIndex][pathIndex];
            if (cached != -1)
            {
                return cached != 0;
            }
            bool result = false;
            if (patternIndex == segments_.size())
            {
                result = pathIndex == pathSegments.size();
            }
            else if (segments_[patternIndex] == "**")
            {
                result = match(patternIndex + 1, pathIndex) ||
                    (pathIndex < pathSegments.size() && match(patternIndex, pathIndex + 1));
            }
            else if (pathIndex < pathSegments.size() &&
                matchSegment(segments_[patternIndex], pathSegments[pathIndex]))
            {
                result = match(patternIndex + 1, pathIndex + 1);
            }
            cached = result ? 1 : 0;
            return result;
        };

    return match(0, 0);
}

std::vector<std::string> GlobPattern::split(std::string_view value)
{
    std::vector<std::string> result;
    std::size_t begin = 0;
    while (begin <= value.size())
    {
        const std::size_t end = value.find('/', begin);
        const std::size_t count = end == std::string_view::npos ? value.size() - begin : end - begin;
        if (count != 0)
        {
            result.emplace_back(value.substr(begin, count));
        }
        if (end == std::string_view::npos)
        {
            break;
        }
        begin = end + 1;
    }
    return result;
}

bool GlobPattern::matchSegment(std::string_view pattern, std::string_view value)
{
    std::vector<std::vector<int>> memo(
        pattern.size() + 1,
        std::vector<int>(value.size() + 1, -1));
    std::function<bool(std::size_t, std::size_t)> match =
        [&](std::size_t patternIndex, std::size_t valueIndex)
        {
            int& cached = memo[patternIndex][valueIndex];
            if (cached != -1)
            {
                return cached != 0;
            }
            bool result = false;
            if (patternIndex == pattern.size())
            {
                result = valueIndex == value.size();
            }
            else if (pattern[patternIndex] == '*')
            {
                result = match(patternIndex + 1, valueIndex) ||
                    (valueIndex < value.size() && match(patternIndex, valueIndex + 1));
            }
            else if (valueIndex < value.size() &&
                (pattern[patternIndex] == '?' || fold(pattern[patternIndex]) == fold(value[valueIndex])))
            {
                result = match(patternIndex + 1, valueIndex + 1);
            }
            cached = result ? 1 : 0;
            return result;
        };
    return match(0, 0);
}

char GlobPattern::fold(char value) noexcept
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}

void GlobPattern::validate()
{
    if (pattern_.empty())
    {
        validationError_ = "glob pattern cannot be empty";
        return;
    }
    if (pattern_.front() == '/' || (pattern_.size() >= 2 && pattern_[1] == ':'))
    {
        validationError_ = "glob pattern must be project-relative";
        return;
    }
    for (const std::string& segment : segments_)
    {
        if (segment == "." || segment == "..")
        {
            validationError_ = "glob pattern cannot contain '.' or '..' segments";
            return;
        }
        if (segment.find("**") != std::string::npos && segment != "**")
        {
            validationError_ = "'**' must occupy an entire glob path segment";
            return;
        }
    }
}
}
