#pragma once

struct ExpressionAnalysisCacheKeyHash final
{
    std::size_t operator()(const ExpressionAnalysisCacheKey& key) const noexcept;
};
