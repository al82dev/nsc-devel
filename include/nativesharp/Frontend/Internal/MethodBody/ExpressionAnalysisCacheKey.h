#pragma once

struct ExpressionAnalysisCacheKey final
{
    const ast::AstNode* syntax = nullptr;
    std::uint8_t optionMask = 0;

    bool operator==(const ExpressionAnalysisCacheKey&) const noexcept = default;
};
