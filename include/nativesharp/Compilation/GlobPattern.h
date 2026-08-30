#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::compilation
{
class GlobPattern final
{
public:
    explicit GlobPattern(std::string pattern);

    bool isValid() const noexcept;
    const std::string& validationError() const noexcept;
    const std::string& pattern() const noexcept;
    bool matches(const std::filesystem::path& projectRelativePath) const;

private:
    static std::vector<std::string> split(std::string_view value);
    static bool matchSegment(std::string_view pattern, std::string_view value);
    static char fold(char value) noexcept;
    void validate();

    std::string pattern_;
    std::vector<std::string> segments_;
    std::string validationError_;
};
}
