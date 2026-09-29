#include "nativesharp/Frontend/CompilationStage.h"

#include <array>

namespace nativesharp::frontend
{
namespace
{
constexpr std::array contracts = {
    CompilationStageContract{CompilationStage::Parse, "parse", StageInformation::None,
        StageInformation::Syntax, false},
    CompilationStageContract{CompilationStage::Binding, "binding", StageInformation::Syntax,
        StageInformation::Symbols, false},
    CompilationStageContract{CompilationStage::TypeChecking, "type-checking",
        StageInformation::Syntax | StageInformation::Symbols, StageInformation::Types, false},
    CompilationStageContract{CompilationStage::Comptime, "comptime",
        StageInformation::Types, StageInformation::Constants, false},
    CompilationStageContract{CompilationStage::Mir, "mir", StageInformation::Types,
        StageInformation::ControlFlow, false},
    CompilationStageContract{CompilationStage::Monomorphization, "monomorphization",
        StageInformation::Types | StageInformation::ControlFlow,
        StageInformation::Specializations, false},
    CompilationStageContract{CompilationStage::Codegen, "codegen",
        StageInformation::ControlFlow | StageInformation::Specializations,
        StageInformation::MachineCode, true},
};
}

std::span<const CompilationStageContract> compilationStageContracts() noexcept
{
    return contracts;
}

const CompilationStageContract& compilationStageContract(CompilationStage stage) noexcept
{
    return contracts[static_cast<std::size_t>(stage)];
}

StageInformation informationAvailableAfter(CompilationStage stage) noexcept
{
    StageInformation result = StageInformation::None;
    for (const CompilationStageContract& contract : contracts)
    {
        result = result | contract.produces;
        if (contract.stage == stage) break;
    }
    return result;
}

bool CompilationStageTracker::enter(CompilationStage stage, std::string& error)
{
    error.clear();
    const std::size_t expected = completed_.size();
    const std::size_t requested = static_cast<std::size_t>(stage);
    if (requested >= contracts.size())
    {
        error = "NS3010: unknown compilation stage";
        return false;
    }
    if (requested != expected)
    {
        const std::string_view expectedName =
            expected < contracts.size() ? contracts[expected].name : "complete";
        error = "NS3010: compilation stage '" + std::string(contracts[requested].name) +
            "' cannot run now; expected '" + std::string(expectedName) + "'";
        return false;
    }
    const CompilationStageContract& contract = contracts[requested];
    if (!containsStageInformation(available_, contract.requiredInformation))
    {
        error = "NS3010: compilation stage '" + std::string(contract.name) +
            "' is missing required producer information";
        return false;
    }
    completed_.push_back(stage);
    available_ = available_ | contract.produces;
    return true;
}

std::span<const CompilationStage> CompilationStageTracker::completed() const noexcept
{
    return completed_;
}

StageInformation CompilationStageTracker::availableInformation() const noexcept
{
    return available_;
}
}
