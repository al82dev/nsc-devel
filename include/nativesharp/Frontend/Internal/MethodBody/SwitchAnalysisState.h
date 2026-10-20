#pragma once

struct SwitchAnalysisState final
{
    const ast::AstNode* statement = nullptr;
    std::size_t scopeOrdinal = 0;
    MethodSwitchBinding binding;
    OwnershipFlowSnapshot switchEntry;
    std::vector<OwnershipFlowSnapshot> sectionExitStates;
    std::size_t nextChild = 0;
    bool hasDefault = false;
};
