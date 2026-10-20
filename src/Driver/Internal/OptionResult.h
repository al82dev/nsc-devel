#pragma once

#include "nativesharp/Basic/ExitCode.h"

namespace nativesharp::driver
{
namespace
{
struct OptionResult final
{
    bool matched = false;
    ExitCode code = ExitCode::Success;
};
} // namespace
} // namespace nativesharp::driver
