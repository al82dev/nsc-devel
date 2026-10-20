#pragma once

namespace nativesharp::frontend
{
struct ProjectMethodBodyModel::IfFrame final
{
    const ast::AstNode* statement = nullptr;
    std::size_t scopeOrdinal = 0;
    IfStage stage = IfStage::Start;
    OwnershipFlowSnapshot thenEntry;
    OwnershipFlowSnapshot elseEntry;
    OwnershipFlowSnapshot thenState;
    std::optional<OwnershipFlowSnapshot> restoreAfterExit;
};
}
