#pragma once

#include "nativesharp/Frontend/ProjectGenericModel.h"

namespace nativesharp::frontend
{
struct ProjectGenericModel::CollectFrame final
{
    ast::AstNode* node = nullptr;
    ast::AstNode* parent = nullptr;
    std::string namespaceName;
    std::string ownerName;
    bool insideGenericType = false;
};
}
