#pragma once

namespace nativesharp::language
{
enum class PatternKind
{
    Wildcard,
    Literal,
    Binding,
    Tuple,
    Struct,
    Variant,
    Range,
    Or
};
}
