#pragma once

#include "nativesharp/Language/Representation.h"

#include <cstddef>
#include <optional>

namespace nativesharp::language
{
struct RepresentationContract final
{
    Representation representation = Representation::Native;
    std::optional<std::size_t> packing;
    std::optional<std::size_t> alignment;
    std::size_t nonZeroFields = 0;
};
}
