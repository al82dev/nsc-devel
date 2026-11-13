#pragma once

namespace nativesharp::frontend
{
struct ProjectMethodBodyModel::ForFrame final
{
    const ast::AstNode* statement = nullptr;
    std::size_t parentScopeOrdinal = 0;
    ForStage stage = ForStage::Start;
    std::size_t forScope = 0;
    std::size_t breakCleanupDepth = 0;
    bool conditionAlwaysTrue = false;
    OwnershipFlowSnapshot loopEntry;
    std::size_t localCountAtBodyEntry = 0;
    std::size_t callCountAtEntry = 0;
    std::size_t namespaceCallCountAtEntry = 0;
    std::size_t delegateCountAtEntry = 0;
    std::size_t eventCountAtEntry = 0;
    const ClassMemberSymbol* loopMethod = nullptr;
    std::optional<OwnershipFlowSnapshot> restoreAfterExit;
};
}
