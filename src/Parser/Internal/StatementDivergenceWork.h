#pragma once

#include "StatementDivergenceWorkKind.h"

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::parser::statement_divergence
{
struct Work final
{
    WorkKind kind = WorkKind::Evaluate;
    const ast::AstNode* node = nullptr;
};
}
