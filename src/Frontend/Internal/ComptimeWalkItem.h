#pragma once

#include <string>

namespace nativesharp::ast { class AstNode; }

namespace nativesharp::frontend::internal
{
// One pending syntax node and the declaration context reached on the walk.
struct ComptimeWalkItem final
{
    const ast::AstNode* node;
    std::string nameSpace;
    std::string ownerClass;
    bool namespaceMember;
    bool classMember;
    bool ownerHasState;
};
}
