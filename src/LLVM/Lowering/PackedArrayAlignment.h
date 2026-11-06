#pragma once

#include "nativesharp/Frontend/ProjectOopMirNode.h"

#include <cstdint>
#include <numeric>
#include <optional>

namespace nativesharp::llvm_backend
{
inline std::optional<std::uint64_t> fieldOrArrayAddressAlignment(
    const frontend::ProjectOopMirNode* node, unsigned depth = 0) noexcept
{
    if (!node) return std::nullopt;
    if (depth > 128) return 1;
    if (node->kind == frontend::ProjectOopMirNodeKind::FieldAddress ||
        node->kind == frontend::ProjectOopMirNodeKind::FieldLoad)
    {
        if (node->alignment == 0 || node->children.size() != 1 || !node->children.front()) return std::nullopt;
        const auto* receiver = node->children.front().get();
        // A FieldLoad receiver is a loaded value, potentially a pointer; it is not its slot address.
        if (receiver->kind != frontend::ProjectOopMirNodeKind::FieldAddress &&
            receiver->kind != frontend::ProjectOopMirNodeKind::RawElementAddress) return node->alignment;
        const auto parent = fieldOrArrayAddressAlignment(receiver, depth + 1);
        return parent ? std::gcd(node->alignment, std::gcd(*parent, node->immediate)) : 1;
    }
    if (node->kind != frontend::ProjectOopMirNodeKind::RawElementAddress || node->children.size() != 2 ||
        !node->children.front() || node->immediate == 0) return std::nullopt;
    const auto parent = fieldOrArrayAddressAlignment(node->children.front().get(), depth + 1);
    return parent ? std::optional<std::uint64_t>{std::gcd(*parent, node->immediate)} : std::nullopt;
}

// Pointer-based indexing without a field-address root has no field alignment proof.
inline std::optional<std::uint64_t> fieldArrayElementAlignment(
    const frontend::ProjectOopMirNode* node) noexcept
{
    if (!node || node->kind != frontend::ProjectOopMirNodeKind::RawElementAddress) return std::nullopt;
    return fieldOrArrayAddressAlignment(node);
}
}
