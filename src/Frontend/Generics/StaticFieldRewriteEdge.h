#pragma once

#include <cstddef>

namespace nativesharp::ast { class AstNode; }

namespace nativesharp::frontend::internal
{
// slot 0, 1 and 2 name first, second and third; slot 3 uses child index.
struct StaticFieldRewriteEdge final
{
    ast::AstNode* parent;
    ast::AstNode* child;
    std::size_t slot;
    std::size_t index;
};
}
