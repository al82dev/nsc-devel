#pragma once

#include "nativesharp/Frontend/ProjectOopHirModel.h"

namespace nativesharp::frontend
{
struct ProjectOopHirModel::BoundCallTarget final
{
    std::size_t memberOrdinal = 0;
    const ast::AstNode* receiverSyntax = nullptr;
    std::string_view receiverTypeKey;
    bool interfaceDispatch = false;
};
}
