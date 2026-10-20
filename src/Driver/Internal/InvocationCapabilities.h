#pragma once

namespace nativesharp::driver
{
namespace
{
struct InvocationCapabilities final
{
    bool executable = false;
    bool debug = false;
    bool artifactCache = false;
};
} // namespace
} // namespace nativesharp::driver
