#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
// The visibility predicate belongs to the caller: name lookup and the pre-parser do not
// necessarily have the same indexes, but they must probe namespace spellings in the same order.
template<typename IsVisible>
std::optional<std::string> resolveQualifiedNamespaceTarget(
    std::span<const std::string> namespaceSegments, std::string_view target, IsVisible&& isVisible)
{
    for (std::size_t depth = namespaceSegments.size() + 1; depth > 0; --depth)
    {
        std::string candidate;
        for (std::size_t index = 0; index < depth - 1; ++index)
        {
            if (!candidate.empty()) candidate.push_back('.');
            candidate += namespaceSegments[index];
        }
        if (!candidate.empty() && !target.empty()) candidate.push_back('.');
        candidate.append(target);
        if (isVisible(candidate)) return candidate;
    }
    return std::nullopt;
}
}
