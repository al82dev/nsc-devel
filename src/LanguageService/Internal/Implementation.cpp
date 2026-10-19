#include "Implementation.h"

#include <algorithm>
#include <cctype>

namespace nativesharp::language_service
{
std::optional<std::pair<std::string, std::filesystem::path>>
LanguageService::Implementation::documentKey(const std::filesystem::path& path)
{
    if (path.empty()) return std::nullopt;
    std::error_code error;
    std::filesystem::path absolute = std::filesystem::absolute(path, error);
    if (error) return std::nullopt;
    absolute = absolute.lexically_normal();
    std::string key = absolute.generic_string();
    std::ranges::transform(key, key.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return std::pair{std::move(key), std::move(absolute)};
}
}
