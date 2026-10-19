#pragma once

#include "nativesharp/Interop/LibraryApiChangeImpact.h"

#include <string>

namespace nativesharp::interop
{
struct LibraryApiChange final
{
    LibraryApiChangeImpact impact = LibraryApiChangeImpact::Additive;
    std::string domain;
    std::string identity;
    std::string detail;
};
}
