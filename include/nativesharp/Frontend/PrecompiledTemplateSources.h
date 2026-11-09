#pragma once

#include <charconv>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend
{
// A NUL-prefixed frame cannot be mistaken for valid NativeSharp source. Each entry is a
// complete source unit so file-level imports remain independent between declarations.
inline constexpr std::string_view kPrecompiledTemplateFrame{"\0NSC-TEMPLATE-UNITS-1\n", 22U};

inline bool isPrecompiledTemplateFrame(std::string_view text) noexcept
{
    return text.starts_with(kPrecompiledTemplateFrame);
}

inline std::string encodePrecompiledTemplateSources(const std::vector<std::string>& sources)
{
    std::string result(kPrecompiledTemplateFrame);
    for (const std::string& source : sources)
    {
        result += std::to_string(source.size());
        result.push_back('\n');
        result += source;
    }
    return result;
}

inline bool decodePrecompiledTemplateSources(
    std::string_view frame, std::vector<std::string>& sources, std::string& error)
{
    sources.clear();
    if (!isPrecompiledTemplateFrame(frame))
    {
        error = "invalid precompiled template source frame";
        return false;
    }
    frame.remove_prefix(kPrecompiledTemplateFrame.size());
    while (!frame.empty())
    {
        if (sources.size() >= 1'000'000U)
        {
            error = "too many precompiled template source units";
            return false;
        }
        const std::size_t newline = frame.find('\n');
        std::size_t length = 0;
        if (newline == std::string_view::npos || newline == 0U || newline > 20U)
        {
            error = "invalid precompiled template source length";
            return false;
        }
        const auto converted = std::from_chars(frame.data(), frame.data() + newline, length);
        if (converted.ec != std::errc{} || converted.ptr != frame.data() + newline || length == 0U ||
            length > frame.size() - newline - 1U)
        {
            error = "truncated or invalid precompiled template source";
            return false;
        }
        frame.remove_prefix(newline + 1U);
        if (isPrecompiledTemplateFrame(frame.substr(0, length)))
        {
            error = "nested precompiled template source frame";
            return false;
        }
        sources.emplace_back(frame.substr(0, length));
        frame.remove_prefix(length);
    }
    return true;
}
}
