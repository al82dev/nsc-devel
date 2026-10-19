#pragma once

#include "nativesharp/Language/ThreadSafetyMemberKind.h"

#include <string>

namespace nativesharp::language
{
struct ThreadSafetyMemberFacts final
{
    std::string name;
    ThreadSafetyMemberKind kind = ThreadSafetyMemberKind::Value;
    bool send = true;
    bool sync = true;
    bool dynamicTraitSend = false;
    bool dynamicTraitSync = false;
};
}
