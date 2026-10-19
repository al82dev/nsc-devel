#pragma once

namespace nativesharp::language
{
struct ThreadSafetyFacts final
{
    bool fieldsSend = true;
    bool fieldsSync = true;
    bool containsRawPointer = false;
    bool negativeSendImpl = false;
    bool negativeSyncImpl = false;
};
}
