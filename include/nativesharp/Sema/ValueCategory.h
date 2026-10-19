#pragma once

namespace nativesharp::sema
{
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
}
