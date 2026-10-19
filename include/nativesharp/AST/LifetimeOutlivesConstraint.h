#pragma once

#include "nativesharp/Basic/SourceRange.h"

#include <string>

namespace nativesharp::ast
{
struct LifetimeOutlivesConstraint
{
    std::string longer;
    std::string shorter;
    SourceRange range;
};
}
