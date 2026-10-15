#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/MethodExpressionTypeBinding.h"
#include "nativesharp/Frontend/ProjectConstructionSiteKind.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
// NS304-T13-F08. Explicit allocation-failure profile for `new (allocator, failure: ...) T(...)`.
enum class ProjectAllocatorFailureProfile
{
    None,    // no `failure:` — preserves T13-F02/T13-F07 behaviour.
    Null,    // `failure: null`    — Allocate null -> construction result is null.
    Result,  // `failure: result`  — Result<owned T, AllocationFailure>.
    Throws,  // `failure: throws`  — Allocate null -> AllocationFailureException.
    Panic    // `failure: panic`   — Allocate null -> project panic profile.
};

// Typed hand-off from method-body analysis to constructor overload resolution. Sites are ordered
// by callableOrdinal/sequenceWithinCallable, never by discovery timing, so future parallel body
// analysis cannot perturb dumps, HIR ordinals, or layout consumers.
struct ProjectConstructionSite final
{
    const ast::AstNode* syntax = nullptr;
    std::size_t callableOrdinal = 0;
    std::size_t sequenceWithinCallable = 0;
    ProjectConstructionSiteKind kind = ProjectConstructionSiteKind::Object;
    std::string targetTypeKey;
    std::optional<std::size_t> targetTypeOrdinal;
    // NS304-T13-F02: the allocator operand type for an `Allocator` site (`new (allocator) T(...)`).
    std::string allocatorTypeKey;
    std::optional<std::size_t> allocatorAllocateMemberOrdinal;
    // NS304-T13-F07: the allocator's Deallocate member (the provenance pair of Allocate).
    std::optional<std::size_t> allocatorDeallocateMemberOrdinal;
    // NS304-T13-F08: the explicit failure profile (None when `failure:` is absent).
    ProjectAllocatorFailureProfile allocatorFailureProfile = ProjectAllocatorFailureProfile::None;
    std::vector<MethodExpressionTypeBinding> argumentTypes;
    bool argumentsTyped = false;
};
}
