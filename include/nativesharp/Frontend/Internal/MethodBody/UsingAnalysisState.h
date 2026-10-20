#pragma once

struct UsingAnalysisState final
{
    const ast::AstNode* body = nullptr;
    std::size_t usingScope = 0;
    bool cleanupRegionActive = false;
};
