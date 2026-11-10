#pragma once

#include <string>

namespace nativesharp::ast { class AstNode; }

namespace nativesharp::frontend::internal
{
// A free function or static method with its namespace and optional declaring type.
struct ComptimeDeclaration final
{
    const ast::AstNode* node;
    std::string nameSpace;
    std::string ownerClass;
    bool pendingDirective;
    bool ownerHasState;
};
}
