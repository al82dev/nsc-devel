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

// The root marker is set only for supported C global scalar arrays with proven alignment.
// Each raw index must preserve that alignment, including multidimensional strides.
inline std::optional<std::uint64_t> clangMsVolatileGlobalScalarArrayAlignment(
    const frontend::ProjectOopMirNode* node) noexcept
{
    if (!node || node->kind != frontend::ProjectOopMirNodeKind::RawElementAddress ||
        !node->volatileAccess) return std::nullopt;
    const auto& elementType = node->typeKey;
    std::uint64_t width = 0;
    if (elementType == "builtin:sbyte" || elementType == "builtin:byte") width = 1;
    else if (elementType == "builtin:short" || elementType == "builtin:ushort") width = 2;
    else if (elementType == "builtin:int" || elementType == "builtin:uint" ||
             elementType == "builtin:float") width = 4;
    else if (elementType == "builtin:long" || elementType == "builtin:ulong" ||
             elementType == "builtin:double") width = 8;
    if (width == 0) return std::nullopt;
    unsigned depth = 0;
    while (node && node->kind == frontend::ProjectOopMirNodeKind::RawElementAddress)
    {
        if (++depth > 128 || node->children.size() != 2 || !node->children.front() ||
            node->immediate == 0 || node->immediate % width != 0) return std::nullopt;
        node = node->children.front().get();
    }
    if (!node || node->kind != frontend::ProjectOopMirNodeKind::NamespaceStateAddress ||
        !node->volatileAccess || !node->cppMicrosoftVolatileSemantics.value_or(false) ||
        node->cppAtomicAlignmentBytes != std::optional<std::uint64_t>{width} ||
        !node->typeKey.starts_with(elementType) || node->typeKey.size() <= elementType.size() ||
        node->typeKey[elementType.size()] != '[') return std::nullopt;
    return width;
}

inline std::optional<std::uint64_t> clangMsVolatileDirectCPointeeAlignment(const frontend::ProjectOopMirNode* node) noexcept
{
    if (!node || !(node->kind == frontend::ProjectOopMirNodeKind::AddressValue ||
                    node->kind == frontend::ProjectOopMirNodeKind::PointerLoad ||
                    node->kind == frontend::ProjectOopMirNodeKind::RawElementAddress) || !node->volatileAccess ||
        node->cppMicrosoftVolatileSemantics != std::optional<bool>{true}) return std::nullopt;
    auto width = node->typeKey == "builtin:bool" ? 1U : node->typeKey == "builtin:short" ? 2U :
        (node->typeKey == "builtin:int" || node->typeKey == "builtin:uint") ? 4U :
        (node->typeKey == "builtin:long" || node->typeKey == "builtin:double") ? 8U : 0U;
    if (node->typeKey == "builtin:float")
    {
        const auto* base = node;
        for (unsigned depth = 0; depth < 3 && base; ++depth)
        {
            if (base->kind == frontend::ProjectOopMirNodeKind::NamespaceStateLoad &&
                base->cppAtomicAccess && base->typeKey == "builtin:float*")
            { width = 4; break; }
            base = base->children.empty() ? nullptr : base->children.front().get();
        }
    }
    if (!width || node->cppAtomicAlignmentBytes != std::optional<std::uint64_t>{width}) return std::nullopt;
    return width;
}

// A C _Bool occupies one byte even though the NS expression value is i1.
inline bool cBoolGlobalArrayElement(const frontend::ProjectOopMirNode* node) noexcept
{
    if (!node || node->kind != frontend::ProjectOopMirNodeKind::RawElementAddress ||
        node->typeKey != "builtin:bool") return false;
    unsigned depth = 0;
    while (node && node->kind == frontend::ProjectOopMirNodeKind::RawElementAddress)
    {
        if (++depth > 128 || node->children.size() != 2 || !node->children.front() ||
            node->immediate == 0) return false;
        node = node->children.front().get();
    }
    return node && node->kind == frontend::ProjectOopMirNodeKind::NamespaceStateAddress &&
        node->cppCBoolStorage && node->typeKey.starts_with("builtin:bool[");
}

inline bool clangMsVolatileGlobalBoolArray(const frontend::ProjectOopMirNode* node) noexcept
{
    if (!node || node->kind != frontend::ProjectOopMirNodeKind::RawElementAddress ||
        node->typeKey != "builtin:bool" || !node->volatileAccess) return false;
    unsigned depth = 0;
    while (node && node->kind == frontend::ProjectOopMirNodeKind::RawElementAddress)
    {
        if (++depth > 128 || node->children.size() != 2 || !node->children.front() ||
            node->immediate == 0) return false;
        node = node->children.front().get();
    }
    return node && node->kind == frontend::ProjectOopMirNodeKind::NamespaceStateAddress &&
        node->volatileAccess && node->cppMicrosoftVolatileSemantics == std::optional<bool>{true} &&
        node->cppAtomicAlignmentBytes == std::optional<std::uint64_t>{1} &&
        node->typeKey.starts_with("builtin:bool[");
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
