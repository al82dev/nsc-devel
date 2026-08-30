#pragma once

#include <cstdint>

namespace nativesharp::passes
{
enum class AnalysisKind : std::uint8_t
{
    Cfg,
    Dominance
};
}
