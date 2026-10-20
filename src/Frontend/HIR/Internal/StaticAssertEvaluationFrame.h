#pragma once

#include "StaticAssertConstant.h"

#include "nativesharp/Frontend/ProjectOopHirNode.h"

#include <cstdint>
#include <optional>

namespace nativesharp::frontend::internal
{
struct StaticAssertEvaluationFrame final
{
    const ProjectOopHirNode* expression = nullptr;
    std::uint8_t stage = 0;
    std::optional<StaticAssertConstant> first;
};
}
