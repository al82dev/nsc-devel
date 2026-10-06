#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Sema/ExpressionInfo.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace nativesharp::frontend
{
// Immutable semantic facts for one expression occurrence in one callable. The callable identity
// is part of the key because a generic syntax node can be analyzed under more than one closed
// callable. Downstream models consume this record instead of reconstructing types or ownership.
struct MethodExpressionTypeBinding final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t methodMemberOrdinal = 0;
    std::size_t scopeOrdinal = 0;
    std::string typeKey;
    // NativeSharp 3.0 T00-F03. This is the canonical value-category view consumed by
    // lowering/tooling. Ownership remains orthogonal: it explains who destroys a value, while
    // this field explains whether the expression denotes storage, a value, or a temporary.
    sema::ValueCategory valueCategory = sema::ValueCategory::Value;
    std::optional<ProjectIntegerConstant> integerConstant;
    bool pointerPointeeReadonly = false;
    // NSC-LIMIT-010. Constructor overload resolution consumes the same ownership decision as an
    // ordinary call. Keeping it here distinguishes move(owner) from owner without re-analysis.
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    bool ownershipTransfer = false;
    bool ownershipFreshOwner = false;
    std::optional<std::size_t> ownershipSourceLocalOrdinal;
    std::optional<std::size_t> ownershipSourceParameterOrdinal;
    // The assignment expression targets storage that owns and must replace its current value.
    bool ownsStorageDestination = false;
    // Item 320. Logical object lifetime and backing-storage lifetime are distinct contracts.
    sema::OwnershipKind objectLifetimeOwnership = sema::OwnershipKind::None;
    sema::OwnershipKind storageOwnership = sema::OwnershipKind::None;
    // NativeSharp 3.0 T03-F05. Flow facts belong to this exact expression occurrence.
    bool flowProvenNonNull = false;
    bool flowIntegerExcludesZero = false;
    std::optional<std::int64_t> flowIntegerMinimum;
    std::optional<std::int64_t> flowIntegerMaximum;
    std::optional<std::size_t> flowActiveVariantCase;
    bool flowRuntimeCheckElided = false;
};
}
