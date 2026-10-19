#pragma once

namespace nativesharp::language
{
struct AwaitContext final
{
    bool insideAsyncFunction = false;
    bool insideComptime = false;
    bool insideNoSuspendCleanup = false;
};
}
