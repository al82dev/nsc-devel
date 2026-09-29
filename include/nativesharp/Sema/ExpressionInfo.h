#pragma once

#include "nativesharp/Sema/ConstantValue.h"

#include <string_view>

namespace nativesharp::sema
{
class Symbol;
class Type;

enum class ValueCategory
{
    // Preserve the numeric identities used by the 2.0 frontend.
    Value = 0,
    Place = 1,
    Temporary,
    BorrowedValue,
    MovedFrom,

    // Compatibility spellings for the 2.0 frontend. New 3.0 code uses Place/Value.
    LValue = Place,
    RValue = Value
};

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
