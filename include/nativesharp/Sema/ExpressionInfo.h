#pragma once

#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/ValueCategory.h"

#include <string_view>

namespace nativesharp::sema
{
class Symbol;
class Type;

constexpr bool isPlaceCategory(ValueCategory category) noexcept
{
    return category == ValueCategory::Place || category == ValueCategory::BorrowedValue;
}

constexpr bool isUsableValueCategory(ValueCategory category) noexcept
{
    return category != ValueCategory::MovedFrom;
}

constexpr bool requiresMaterialization(ValueCategory category) noexcept
{
    return category == ValueCategory::Temporary;
}

constexpr std::string_view valueCategoryName(ValueCategory category) noexcept
{
    switch (category)
    {
        case ValueCategory::Place: return "place";
        case ValueCategory::Value: return "value";
        case ValueCategory::Temporary: return "temporary";
        case ValueCategory::BorrowedValue: return "borrowed-value";
        case ValueCategory::MovedFrom: return "moved-from";
    }
    return "value";
}

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
