#pragma once

namespace nativesharp::frontend
{
struct ProjectMethodBodyModel::UsingFrame final
{
    const ast::AstNode* statement = nullptr;
    std::size_t parentScopeOrdinal = 0;
    UsingStage stage = UsingStage::Start;
    std::optional<UsingAnalysisState> state;
    std::optional<OwnershipFlowSnapshot> restoreAfterExit;
};
}
