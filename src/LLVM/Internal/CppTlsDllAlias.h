#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace nativesharp::interop
{
// Identity is the UTF-8 file name followed by ':' and its SHA-256 digest.
inline bool cppTlsSameDllName(std::string_view left, std::string_view right)
{
    const auto leftEnd = left.rfind(':');
    const auto rightEnd = right.rfind(':');
    if (leftEnd == std::string_view::npos || rightEnd == std::string_view::npos || leftEnd != rightEnd)
        return false;
    return std::equal(left.begin(), left.begin() + leftEnd, right.begin(),
        [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
}

inline std::string cppTlsDllAlias(std::string_view identity)
{
    const auto separator = identity.rfind(':');
    if (separator == std::string_view::npos || identity.size() - separator - 1 != 64) return {};
    const auto digest = identity.substr(separator + 1);
    if (!std::all_of(digest.begin(), digest.end(), [](unsigned char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        })) return {};
    return "__nsc_tls_" + std::string(digest) + ".dll";
}
}
