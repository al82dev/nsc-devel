#pragma once

#include <string>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::frontend
{
class ProjectSyntaxUnit;

struct LocatedNode final
{
    ProjectSyntaxUnit* unit = nullptr;
    ast::AstNode* syntax = nullptr;
    std::string namespaceName;
    std::string ownerName;
};
}
