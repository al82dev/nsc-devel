#pragma once

#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstdint>

namespace nativesharp::frontend
{
enum class ProjectNameLookup::Stage : std::uint8_t
{
    Enter,
    AfterArrayCounts
};
}
