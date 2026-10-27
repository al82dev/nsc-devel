#include "nativesharp/Compilation/ImplicitProjectRegistry.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace nativesharp::compilation
{
namespace
{
std::mutex& registryMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<std::string, std::string>& registryTexts()
{
    static std::unordered_map<std::string, std::string> texts;
    return texts;
}

// EN: Windows paths compare without case; both sides use the same canonical spelling.
// ES: Las rutas de Windows se comparan sin mayusculas; ambos lados usan la misma forma canonica.
std::string registryKey(const std::filesystem::path& projectFile)
{
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(projectFile, error);
    if (error) canonical = projectFile.lexically_normal();
    std::string key = canonical.generic_string();
    std::ranges::transform(key, key.begin(), [](unsigned char value)
    {
        return static_cast<char>(std::tolower(value));
    });
    return key;
}
}

void ImplicitProjectRegistry::add(const std::filesystem::path& projectFile, std::string text)
{
    const std::string key = registryKey(projectFile);
    const std::lock_guard<std::mutex> lock(registryMutex());
    registryTexts().insert_or_assign(key, std::move(text));
}

std::optional<std::string> ImplicitProjectRegistry::find(const std::filesystem::path& projectFile)
{
    const std::string key = registryKey(projectFile);
    const std::lock_guard<std::mutex> lock(registryMutex());
    const auto found = registryTexts().find(key);
    if (found == registryTexts().end()) return std::nullopt;
    return found->second;
}
}
