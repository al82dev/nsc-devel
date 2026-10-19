#pragma once

#include "nativesharp/Frontend/ProjectSyntaxUnit.h"

#include "nativesharp/AST/AstNode.h"

#include <string>

namespace nativesharp::frontend::internal
{
using ast::AstNode;

struct LocatedDeclaration final
{
    ProjectSyntaxUnit* unit = nullptr;
    AstNode* parent = nullptr;
    AstNode* syntax = nullptr;
    std::string namespaceName;
    std::string ownerName;
};
}
