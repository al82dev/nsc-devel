#pragma once

#include "nativesharp/Language/Language3Disposition.h"
#include "nativesharp/Language/Language3Feature.h"

#include <string_view>

namespace nativesharp::language
{
struct Language3SurfaceEntry final
{
    Language3Feature feature;
    std::string_view name;
    Language3Disposition disposition;
    std::string_view rationale;
};
}
