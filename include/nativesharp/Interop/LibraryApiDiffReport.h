#pragma once

#include "nativesharp/Interop/LibraryApiChange.h"

#include <string>
#include <vector>

namespace nativesharp::interop
{
struct LibraryApiDiffReport final
{
    bool sourceCompatible = true;
    bool abiCompatible = true;
    std::string oldSourceFingerprint;
    std::string newSourceFingerprint;
    std::string oldAbiSurfaceFingerprint;
    std::string newAbiSurfaceFingerprint;
    std::vector<LibraryApiChange> changes;
};
}
