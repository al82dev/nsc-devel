#pragma once

#include "nativesharp/Basic/SourceRange.h"

#include <cstddef>
#include <string>

namespace nativesharp::frontend
{
struct PendingReexport final
{
    std::size_t ownerProjectOrdinal = 0;
    std::string publicName;
    std::string targetName;
    SourceRange range;
};
}
