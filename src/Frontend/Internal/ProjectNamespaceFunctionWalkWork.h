#pragma once

#include "ProjectNamespaceFunctionWalkKind.h"

#include <cstddef>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::frontend::internal
{
struct WalkWork
{
    WalkKind kind;
    const ast::AstNode* node;
    std::size_t namespaceIndex;
    std::size_t environmentIndex;
};
}
