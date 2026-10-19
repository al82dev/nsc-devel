#pragma once

#include "nativesharp/Frontend/ProjectGenericModel.h"

#include <string>

namespace nativesharp::frontend
{
struct ProjectGenericModel::AliasFrame final
{
    ast::AstNode* node;
    std::string namespaceName;
    GenericLookupScope scope;
};
}
