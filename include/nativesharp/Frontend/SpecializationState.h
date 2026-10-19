#pragma once

#include <cstdint>

namespace nativesharp::frontend
{
// NSC305-G07. Lifecycle of a specialization slot. The sequential pass only uses Unseen (no slot),
// Building (record pushed, materializing) and Ready (product committed). Queued and Failed are the
// states the deterministic parallel planner (G09) will add.
enum class SpecializationState : std::uint8_t
{
    Unseen = 0,
    Queued = 1,
    Building = 2,
    Ready = 3,
    Failed = 4,
};
}
