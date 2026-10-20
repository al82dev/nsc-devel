#pragma once

#include "nativesharp/Target/TargetDataLayout.h"

#include <string_view>

namespace nativesharp::driver
{
namespace
{
using target::BuiltinLayoutKind;

struct LayoutRow
{
    std::string_view name;
    BuiltinLayoutKind kind;
};
} // namespace
} // namespace nativesharp::driver
