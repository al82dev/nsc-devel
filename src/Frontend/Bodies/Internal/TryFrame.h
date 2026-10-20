#pragma once

namespace nativesharp::frontend
{
struct ProjectMethodBodyModel::TryFrame final
{
    const ast::AstNode* statement = nullptr;
    std::size_t scopeOrdinal = 0;
    TryStage stage = TryStage::Start;
    std::optional<TryAnalysisState> state;
    std::optional<OwnershipFlowSnapshot> restoreAfterExit;
};
}
