#pragma once

#include "nativesharp/Frontend/StageInformation.h"

#include <cstdint>
#include <string_view>

namespace nativesharp::frontend
{
enum class CompilationStage : std::uint8_t;

struct CompilationStageContract final
{
    CompilationStage stage;
    std::string_view name;
    StageInformation requiredInformation;
    StageInformation produces;
    bool diagnosticsRequireBackend;
};
}
