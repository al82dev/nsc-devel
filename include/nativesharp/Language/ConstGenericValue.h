#pragma once

#include "nativesharp/Language/ConstGenericKind.h"

#include <string>

namespace nativesharp::language
{
struct ConstGenericValue final
{
    ConstGenericKind kind = ConstGenericKind::Integer;
    std::string typeKey;
    std::string normalizedValue;
};
}
