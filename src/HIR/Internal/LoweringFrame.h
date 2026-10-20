#pragma once

#include "LoweringFrameKind.h"

#include <cstddef>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::sema
{
struct ExpressionInfo;
class Type;
}

namespace nativesharp::hir
{
class HirNode;
}

namespace nativesharp::hir::internal
{
struct LoweringFrame final
{
    LoweringFrameKind kind;
    ast::AstNode* node;
    const ast::AstNode* owner = nullptr;
    int operandIndex = -1;
    bool loadLValue = true;
    std::size_t state = 0;
    std::size_t index = 0;
    HirNode* result = nullptr;
    HirNode* first = nullptr;
    HirNode* second = nullptr;
    const sema::ExpressionInfo* info = nullptr;
    sema::Type* dimensionType = nullptr;
};
}
