#pragma once

namespace nativesharp::frontend
{
struct ProjectMethodBodyModel::SwitchFrame final
{
    const ast::AstNode* statement = nullptr;
    std::size_t scopeOrdinal = 0;
    SwitchStage stage = SwitchStage::Start;
    std::optional<SwitchAnalysisState> state;
    std::optional<OwnershipFlowSnapshot> restoreAfterExit;
};
}
