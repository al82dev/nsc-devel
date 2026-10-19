#pragma once

namespace nativesharp::sema
{
enum class ConstantKind
{
    None,
    SignedInteger,
    UnsignedInteger,
    Floating,
    Boolean,
    Null,
    String
};
}
