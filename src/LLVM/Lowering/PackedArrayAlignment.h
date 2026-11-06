#pragma once

#include "nativesharp/Frontend/ProjectOopMirNode.h"

#include <cstdint>
#include <numeric>
#include <optional>

namespace nativesharp::llvm_backend
{
// Only fixed-array element addresses rooted directly in a field carry this proof.
inline std::optional<std::uint64_t> fieldArrayElementAlignment(
    const frontend::ProjectOopMirNode* node) noexcept
{
    std::uint64_t strideGcd = 0;
    while (node && node->kind == frontend::ProjectOopMirNodeKind::RawElementAddress)
    {
        if (node->children.size() != 2 || !node->children.front() || node->immediate == 0)
            return std::nullopt;
        strideGcd = std::gcd(strideGcd, node->immediate);
        node = node->children.front().get();
    }
    if (strideGcd == 0 || !node || node->kind != frontend::ProjectOopMirNodeKind::FieldAddress ||
        node->alignment == 0) return std::nullopt;
    return std::gcd(strideGcd, node->alignment);
}
}
