#pragma once

#include <cstdint>

namespace nativesharp::frontend::internal
{
enum class WorkKind : std::uint8_t
{
    Visit,
    CompleteThisMember
};
}
