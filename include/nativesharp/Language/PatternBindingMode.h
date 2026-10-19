#pragma once

namespace nativesharp::language
{
enum class PatternBindingMode
{
    Value,
    Reference,
    Move,
    Copy,
    ReferenceMutable
};
}
