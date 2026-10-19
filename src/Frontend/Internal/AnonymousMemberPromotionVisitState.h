#pragma once

#include <cstdint>

namespace nativesharp::frontend
{
enum class AnonymousMemberPromotionVisitState : std::uint8_t
{
    Unvisited,
    Visiting,
    Complete
};
}
