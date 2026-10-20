#pragma once

namespace nativesharp::hir::internal
{
enum class VerificationTaskKind
{
    Node,
    VariableInitializerType,
    IfElseBranch,
    ReturnType,
    CallArgument
};
}
