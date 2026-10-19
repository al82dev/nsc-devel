#pragma once

#include <string>

namespace nativesharp::language
{
struct ThreadSafetyDecision final
{
    bool send = false;
    bool sync = false;
    bool usedUnsafeSend = false;
    bool usedUnsafeSync = false;
    std::string reason;
};
}
