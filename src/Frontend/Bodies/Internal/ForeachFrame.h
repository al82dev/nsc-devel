#pragma once

namespace nativesharp::frontend
{
struct ProjectMethodBodyModel::ForeachFrame final
{
    const ast::AstNode* statement = nullptr;
    std::size_t parentScopeOrdinal = 0;
    ForeachStage stage = ForeachStage::Start;
    std::optional<ForeachAnalysisState> state;
    std::optional<OwnershipFlowSnapshot> restoreAfterExit;
};
}
