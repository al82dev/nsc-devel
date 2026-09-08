#pragma once

#include "nativesharp/Basic/ExitCode.h"

#include <exception>
#include <new>
#include <ostream>
#include <utility>

namespace nativesharp::driver
{
// Last-resort boundary for defects in the compiler itself. User diagnostics must normally be
// produced inside Driver; this boundary exists so an unexpected C++ exception cannot terminate
// nsc without an exit code and a stable explanation.
template <typename Action> int runWithCrashBoundary(Action&& action, std::ostream& error) noexcept
{
    try
    {
        return std::forward<Action>(action)();
    }
    catch (const std::bad_alloc&)
    {
        error << "nsc: internal error: out of memory\n";
    }
    catch (const std::exception& exception)
    {
        error << "nsc: internal error: unhandled exception: " << exception.what() << '\n';
    }
    catch (...)
    {
        error << "nsc: internal error: unknown unhandled exception\n";
    }

    return static_cast<int>(ExitCode::InternalError);
}
} // namespace nativesharp::driver
