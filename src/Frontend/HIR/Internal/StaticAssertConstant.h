#pragma once

#include "StaticAssertConstantKind.h"

#include <cstdint>
#include <string>

namespace nativesharp::frontend::internal
{
struct StaticAssertConstant final
{
    StaticAssertConstantKind kind = StaticAssertConstantKind::Integer;
    std::string typeKey;
    std::uint64_t integerValue = 0;
    double floatingValue = 0.0;
    bool booleanValue = false;
};
}
