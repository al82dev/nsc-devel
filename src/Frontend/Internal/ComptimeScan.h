#pragma once

#include "nativesharp/Frontend/ComptimeEngine.h"

namespace nativesharp::frontend
{
enum class ComptimeEngine::Scan
{
    Code,
    LineComment,
    BlockComment,
    String,
    Character
};
}
