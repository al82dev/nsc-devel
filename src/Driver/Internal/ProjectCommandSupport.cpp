#include "ProjectCommandSupport.h"

#include <cstdlib>

namespace nativesharp::driver
{
std::string jsonEscape(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    constexpr char digits[] = "0123456789abcdef";
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (character < 0x20U)
            {
                result += "\\u00";
                result.push_back(digits[(character >> 4U) & 0x0fU]);
                result.push_back(digits[character & 0x0fU]);
            }
            else
                result.push_back(static_cast<char>(character));
            break;
        }
    }
    return result;
}

std::string pathText(const std::filesystem::path &path)
{
    return path.generic_string();
}

std::optional<std::string> environmentValue(const char *name)
{
#if defined(_WIN32)
    char *value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || value == nullptr)
        return std::nullopt;
    std::string result(value);
    std::free(value);
    return result;
#else
    if (const char *value = std::getenv(name))
        return std::string(value);
    return std::nullopt;
#endif
}
} // namespace nativesharp::driver
