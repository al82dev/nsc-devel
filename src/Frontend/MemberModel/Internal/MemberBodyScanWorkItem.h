#pragma once

#include "MemberBodyScanWorkKind.h"

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::frontend::internal
{
struct WorkItem final
{
    WorkKind kind = WorkKind::Visit;
    const ast::AstNode* node = nullptr;
};
}
