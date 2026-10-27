#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace nativesharp::basic
{
// Heterogeneous hash for unordered containers keyed by std::string (used with
// std::equal_to<>): a lookup with a std::string_view or a C string does not build a temporary
// std::string. Every overload hashes the characters with std::hash<std::string_view>, which the
// standard requires to equal std::hash<std::string> for the same characters, so a container
// that switches to this hash keeps its buckets and its iteration order.
struct TransparentStringHash final
{
    using is_transparent = void;

    std::size_t operator()(std::string_view value) const noexcept
    {
        return std::hash<std::string_view>{}(value);
    }

    std::size_t operator()(const std::string& value) const noexcept
    {
        return (*this)(std::string_view(value));
    }

    std::size_t operator()(const char* value) const noexcept
    {
        return (*this)(std::string_view(value));
    }
};
}
