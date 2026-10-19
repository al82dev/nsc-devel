#pragma once

#include <cstdint>
namespace nativesharp::frontend
{
enum class CompilationStage : std::uint8_t
{
    Parse,
    Binding,
    TypeChecking,
    Comptime,
    Mir,
    Monomorphization,
    Codegen
};
}

#include "nativesharp/Frontend/CompilationStageContract.h"
#include "nativesharp/Frontend/CompilationStageTracker.h"
#include "nativesharp/Frontend/StageInformation.h"

namespace nativesharp::frontend
{

std::span<const CompilationStageContract> compilationStageContracts() noexcept;
const CompilationStageContract& compilationStageContract(CompilationStage stage) noexcept;
StageInformation informationAvailableAfter(CompilationStage stage) noexcept;
}
