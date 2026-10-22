#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace nativesharp::parser
{
struct KnownTypeNameHash final
{
    using is_transparent = void;

    std::size_t operator()(std::string_view value) const noexcept;
    std::size_t operator()(const std::string& value) const noexcept;
};

using KnownTypeNameSet = std::unordered_set<
    std::string,
    KnownTypeNameHash,
    std::equal_to<>>;
}
