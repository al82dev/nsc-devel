#pragma once

struct ForeachAnalysisState final
{
    const ast::AstNode* statement = nullptr;
    const ast::AstNode* chosenCandidate = nullptr;
    std::size_t foreachScope = 0;
    std::size_t cleanupDepth = 0;
    bool iterationBorrowActive = false;
};
