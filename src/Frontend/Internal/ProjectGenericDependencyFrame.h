#pragma once

#include "nativesharp/Frontend/ProjectGenericModel.h"

#include <string>

namespace nativesharp::frontend
{
struct ProjectGenericModel::DependencyFrame final
{
    ast::AstNode* node;
    std::string namespaceName;
    std::string ownerName;
    GenericLookupScope scope;
};
}
