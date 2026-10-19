#pragma once

#include "nativesharp/Frontend/StageInformation.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
enum class CompilationStage : std::uint8_t;

class CompilationStageTracker final
{
public:
    bool enter(CompilationStage stage, std::string& error);
    std::span<const CompilationStage> completed() const noexcept;
    StageInformation availableInformation() const noexcept;

private:
    std::vector<CompilationStage> completed_;
    StageInformation available_ = StageInformation::None;
};
}
