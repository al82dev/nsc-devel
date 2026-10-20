#pragma once

#include "BinaryRightHandSide.h"
#include "nativesharp/AST/AstNode.h"

#include <cstdint>

namespace nativesharp::parser
{
struct BinaryOperatorInfo final
{
    std::uint8_t leftBindingPower;
    std::uint8_t rightBindingPower;
    BinaryRightHandSide rightHandSide;
    ast::AstNodeKind resultKind;
};
}
