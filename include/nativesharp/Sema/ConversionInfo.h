#pragma once

#include "nativesharp/Sema/ConversionKind.h"

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::sema
{
class Type;

enum class ConversionRole
{
    Contextual,
    Operand,
    Result
};

struct ConversionInfo
{
    ConversionKind kind = ConversionKind::Identity;
    ConversionRole role = ConversionRole::Contextual;
    const ast::AstNode* operand = nullptr;
    Type* sourceType = nullptr;
    Type* destinationType = nullptr;
    int operandIndex = -1;
};
}
