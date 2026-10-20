#pragma once

struct IterativeExpressionAnalysis final
{
    std::vector<ExpressionAnalysisCacheEntry> results;
    std::unordered_map<
        ExpressionAnalysisCacheKey,
        std::size_t,
        ExpressionAnalysisCacheKeyHash> resultIndex;
};
