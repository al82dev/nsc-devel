#pragma once

#include "nativesharp/Language/ThreadSafetyMemberFacts.h"

#include <string>
#include <vector>

namespace nativesharp::language
{
struct ThreadSafetyTypeFacts final
{
    std::string typeName;
    std::vector<ThreadSafetyMemberFacts> members;
    bool negativeSendImpl = false;
    bool negativeSyncImpl = false;
    bool unsafeSendImpl = false;
    bool unsafeSyncImpl = false;
    bool unsafeProofReviewed = false;
    bool asyncClosure = false;
};
}
