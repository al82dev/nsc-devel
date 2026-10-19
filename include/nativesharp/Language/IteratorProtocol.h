#pragma once

#include <string>

#include "nativesharp/Language/IterationMode.h"

namespace nativesharp::language
{
struct IteratorProtocol final
{
    std::string itemType;
    IterationMode mode = IterationMode::Value;
    bool hasMoveNext = true;
    bool hasCurrent = true;
    bool hasCleanup = true;
    bool lendsFromSource = false;
};
}
