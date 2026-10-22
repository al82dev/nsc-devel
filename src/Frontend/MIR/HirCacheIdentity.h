#pragma once

#include <string>
#include <vector>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::frontend
{
struct ProjectOopHirFunction;

std::string serializeHirCacheIdentity(const ProjectOopHirFunction& function);
std::vector<const ast::AstNode*> collectHirSyntaxTable(
    const ProjectOopHirFunction& function);
} // namespace nativesharp::frontend
