#pragma once

#include "nativesharp/AST/AstNode.h"

#include <cstdint>
#include <string>

namespace nativesharp::frontend
{
struct HirTuplePatternFrame final
{
    const ast::AstNode* syntax = nullptr;
    std::string typeKey;
    std::uint64_t byteOffset = 0;
};
}
