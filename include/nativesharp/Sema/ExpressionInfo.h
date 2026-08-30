#pragma once

#include "nativesharp/Sema/ConstantValue.h"

namespace nativesharp::sema
{
class Symbol;
class Type;

enum class ValueCategory
{
    RValue,
    LValue
};

struct ExpressionInfo
{
    Type* type = nullptr;
    ValueCategory category = ValueCategory::RValue;
    bool modifiable = false;
    bool addressable = false;
    bool isConstant = false;
    bool requiresBoundsCheck = false;
    ConstantValue constantValue;
    Symbol* symbol = nullptr;
};
}
