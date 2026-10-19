#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/SourceRange.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::frontend
{
// NSC305-G09 (§6.4). The demand-owned instantiation context: what a specialization needs to know
// about its request site, carried by the demand itself instead of the shared model state, so a
// private worker can execute it without the C++ call stack.
struct InstantiationContext final
{
    std::size_t projectOrdinal = 0;
    std::string namespaceName;
    SourceRange range{};
    std::size_t overloadGroup = 0;
    std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>> packSubstitutions;
    std::unordered_set<std::string> associatedProjections;
};
}
