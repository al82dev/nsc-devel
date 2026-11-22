#include "Internal/DriverInternal.h"

#include <algorithm>
#include <array>
#include <fstream>

// EN: UTF-8 path conversion and content comparison shared by the driver.
// ES: Conversion UTF-8 de rutas y comparacion de contenido compartidas por el driver.
namespace nativesharp::driver
{
std::string pathToUtf8(const std::filesystem::path &value)
{
    const auto encoded = value.generic_u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

std::filesystem::path pathFromUtf8(std::string_view value)
{
#if defined(_WIN32)
    if (value.empty())
        return {};
    const int required =
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0)
        return std::filesystem::path(std::string(value));
    std::wstring converted(static_cast<std::size_t>(required), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                              converted.data(), required) != required)
        return std::filesystem::path(std::string(value));
    return std::filesystem::path(converted);
#else
    const std::u8string encoded(reinterpret_cast<const char8_t *>(value.data()), value.size());
    return std::filesystem::path(encoded);
#endif
}

bool filesHaveEqualContents(const std::filesystem::path &left, const std::filesystem::path &right)
{
    std::error_code sizeError;
    const std::uintmax_t leftSize = std::filesystem::file_size(left, sizeError);
    if (sizeError)
        return false;
    const std::uintmax_t rightSize = std::filesystem::file_size(right, sizeError);
    if (sizeError || leftSize != rightSize)
        return false;

    std::ifstream leftStream(left, std::ios::binary);
    std::ifstream rightStream(right, std::ios::binary);
    std::array<char, 64 * 1024> leftBuffer{};
    std::array<char, 64 * 1024> rightBuffer{};
    while (leftStream && rightStream)
    {
        leftStream.read(leftBuffer.data(), static_cast<std::streamsize>(leftBuffer.size()));
        rightStream.read(rightBuffer.data(), static_cast<std::streamsize>(rightBuffer.size()));
        const std::streamsize count = leftStream.gcount();
        if (count != rightStream.gcount() ||
            !std::equal(leftBuffer.begin(), leftBuffer.begin() + count, rightBuffer.begin()))
        {
            return false;
        }
    }
    return leftStream.eof() && rightStream.eof();
}
} // namespace nativesharp::driver
