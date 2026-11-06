#pragma once

#include "nativesharp/Frontend/ProjectOopMirNode.h"
#include "nativesharp/Language/CallableSemantics.h"

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

inline std::optional<std::uint64_t> clangMsVolatileEnumElementAlignment(
    const frontend::ProjectOopMirNode* node) noexcept
{
    if (!node || node->kind != frontend::ProjectOopMirNodeKind::RawElementAddress ||
        !node->volatileAccess || !node->cppMicrosoftVolatileSemantics.value_or(false) ||
        !node->cppAtomicAlignmentBytes) return std::nullopt;
    const auto width = *node->cppAtomicAlignmentBytes;
    if (width != 1 && width != 2 && width != 4 && width != 8) return std::nullopt;
    const auto guaranteed = fieldArrayElementAlignment(node);
    if (!guaranteed || *guaranteed < width || *guaranteed % width != 0) return std::nullopt;
    return width;
}

// The root marker is set only for a C global int array with proven alignment.
// Each raw index must preserve that alignment, including multidimensional strides.
inline std::optional<std::uint64_t> clangMsVolatileGlobalIntArrayAlignment(
    const frontend::ProjectOopMirNode* node) noexcept
{
    if (!node || node->kind != frontend::ProjectOopMirNodeKind::RawElementAddress ||
        node->typeKey != "builtin:int" || !node->volatileAccess) return std::nullopt;
    unsigned depth = 0;
    while (node && node->kind == frontend::ProjectOopMirNodeKind::RawElementAddress)
    {
        if (++depth > 128 || node->children.size() != 2 || !node->children.front() ||
            node->immediate == 0 || node->immediate % 4 != 0) return std::nullopt;
        node = node->children.front().get();
    }
    if (!node || node->kind != frontend::ProjectOopMirNodeKind::NamespaceStateAddress ||
        !node->volatileAccess || !node->cppMicrosoftVolatileSemantics.value_or(false) ||
        node->cppAtomicAlignmentBytes != std::optional<std::uint64_t>{4} ||
        !node->typeKey.starts_with("builtin:int[")) return std::nullopt;
    return 4;
}

inline bool clangMsVolatileDirectCIntPointee(const frontend::ProjectOopMirNode* node) noexcept
{
    return node && (node->kind == frontend::ProjectOopMirNodeKind::AddressValue ||
                    node->kind == frontend::ProjectOopMirNodeKind::PointerLoad) &&
        node->typeKey == "builtin:int" && node->volatileAccess &&
        node->cppMicrosoftVolatileSemantics == std::optional<bool>{true} &&
        node->cppAtomicAlignmentBytes == std::optional<std::uint64_t>{4};
}

inline std::optional<std::uint64_t> clangMsVolatileFunctionFieldAlignment(
    const frontend::ProjectOopMirNode* node) noexcept
{
    if (!node || (node->kind != frontend::ProjectOopMirNodeKind::FieldAddress &&
                  node->kind != frontend::ProjectOopMirNodeKind::FieldLoad) ||
        !node->volatileAccess || !node->cppMicrosoftVolatileSemantics.value_or(false)) return std::nullopt;
    const auto function = language::parseFunctionTypeIdentity(node->typeKey);
    if (!function || function->carrier != language::FunctionCarrier::Pointer) return std::nullopt;
    const auto alignment = fieldOrArrayAddressAlignment(node);
    if (!alignment || *alignment < 8 || *alignment % 8 != 0) return std::nullopt;
    return 8;
}
}
