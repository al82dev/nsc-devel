#pragma once

struct TryAnalysisState final
{
    const ast::AstNode* statement = nullptr;
    std::size_t scopeOrdinal = 0;
    std::optional<std::size_t> cleanupRegionOrdinal;
    OwnershipFlowSnapshot tryEntryState;
    OwnershipFlowSnapshot tryExitState;
    OwnershipFlowSnapshot catchEntryState;
    OwnershipFlowSnapshot joinedExitState;
    OwnershipFlowSnapshot postTryState;
    std::size_t throwStateBegin = 0;
    std::size_t nextCatchChild = 0;
    std::optional<std::size_t> exceptionRoot;
    bool sawCatchAll = false;
    std::vector<std::size_t> previousTypedCatchOrdinals;
    std::size_t currentCatchScope = 0;
    bool catchContextActive = false;
    bool finallyContextActive = false;
};
