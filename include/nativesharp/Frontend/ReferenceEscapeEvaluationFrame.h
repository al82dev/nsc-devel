#pragma once

#include "nativesharp/Frontend/MethodEscapeOrigin.h"
#include "nativesharp/Frontend/ReferenceEscapeContinuationKind.h"

#include <cstddef>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::frontend
{
struct ReferenceEscapeEvaluationFrame final
{
    ReferenceEscapeContinuationKind kind = ReferenceEscapeContinuationKind::MatchArms;
    const ast::AstNode* syntax = nullptr;
    std::size_t nextIndex = 0;
    MethodEscapeOrigin accumulated;
    bool hasAccumulated = false;
};
}
