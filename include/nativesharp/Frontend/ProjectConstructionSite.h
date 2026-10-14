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
    std::vector<MethodExpressionTypeBinding> argumentTypes;
    bool argumentsTyped = false;
};
}
