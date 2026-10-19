#pragma once

#include "nativesharp/Frontend/ProjectGenericModel.h"

#include <cstdint>

namespace nativesharp::frontend
{
struct ProjectGenericModel::ChildWork final
{
    const ast::AstNode* source;
    ast::AstNode* parent;
    std::uint8_t slot;  // 1=first, 2=second, 3=third, 4=child
};
}
