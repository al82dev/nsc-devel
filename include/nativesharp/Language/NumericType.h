#pragma once

#include "nativesharp/Language/NumericKind.h"

namespace nativesharp::language
{
struct NumericType final { NumericKind kind; unsigned bits; bool isSigned; };
}
