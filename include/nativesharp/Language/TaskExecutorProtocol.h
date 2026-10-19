#pragma once

#include <string>

namespace nativesharp::language
{
struct TaskExecutorProtocol final
{
    std::string typeName;
    bool hasSpawn = true;
    bool hasPollJoin = true;
    bool hasCancel = true;
    bool staticDispatch = true;
    bool supportsNoHeap = false;
};
}
