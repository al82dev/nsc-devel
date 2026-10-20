#pragma once

#include "nativesharp/Frontend/ProjectOopMirDispatchTable.h"

namespace nativesharp::frontend
{
struct ProjectOopMirDispatchTable::OwnedField final
{
    std::size_t memberOrdinal = 0;
    std::uint64_t offset = 0;
    // A native string owns a flat allocation released directly; a class instance is released
    // through its runtime class descriptor's destruction entry.
    bool isNativeString = false;
};
}
