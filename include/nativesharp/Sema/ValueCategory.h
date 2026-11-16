#pragma once

namespace nativesharp::sema
{
enum class ValueCategory
{
    // Preserve the numeric identities used by existing frontend data.
    Value = 0,
    Place = 1,
    Temporary,
    BorrowedValue,
    MovedFrom,

    // Compatibility spellings for earlier frontend data. New code uses Place/Value.
    LValue = Place,
    RValue = Value
};
}
