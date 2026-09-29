#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

enum class StageInformation : std::uint32_t
{
    None = 0,
    Syntax = 1U << 0U,
    Symbols = 1U << 1U,
    Types = 1U << 2U,
    Constants = 1U << 3U,
    ControlFlow = 1U << 4U,
    Specializations = 1U << 5U,
    MachineCode = 1U << 6U
};

constexpr StageInformation operator|(StageInformation left, StageInformation right) noexcept
{
    return static_cast<StageInformation>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr bool containsStageInformation(
    StageInformation available,
    StageInformation requested) noexcept
{
    return (static_cast<std::uint32_t>(available) & static_cast<std::uint32_t>(requested)) ==
        static_cast<std::uint32_t>(requested);
}

struct CompilationStageContract final
{
    CompilationStage stage;
    std::string_view name;
    StageInformation requiredInformation;
    StageInformation produces;
    bool diagnosticsRequireBackend;
};

std::span<const CompilationStageContract> compilationStageContracts() noexcept;
const CompilationStageContract& compilationStageContract(CompilationStage stage) noexcept;
StageInformation informationAvailableAfter(CompilationStage stage) noexcept;

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
