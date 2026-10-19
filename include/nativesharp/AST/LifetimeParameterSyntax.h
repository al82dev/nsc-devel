#pragma once

#include "nativesharp/Basic/SourceRange.h"

#include <string>

namespace nativesharp::ast
{
// NativeSharp 3.0 T06-F04. Lifetime parameters are erased semantic parameters: unlike
// type/const generics they never contribute to ABI identity or request monomorphization.
struct LifetimeParameterSyntax
{
    std::string name;
    SourceRange range;
};
}
