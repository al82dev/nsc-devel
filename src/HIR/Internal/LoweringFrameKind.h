#pragma once

namespace nativesharp::hir::internal
{
enum class LoweringFrameKind
{
    Global,
    Struct,
    Enum,
    Function,
    GlobalVariable,
    Block,
    Statement,
    LocalVariable,
    Expression,
    Address,
    ConvertedOperand
};
}
