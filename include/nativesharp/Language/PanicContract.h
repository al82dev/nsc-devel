#pragma once

namespace nativesharp::language
{
struct PanicContract final
{
    bool runsCleanup = false;
    bool mayCrossFfi = false;
};
}
