#pragma once

struct ExpressionAnalysisRequest final
{
    const ast::AstNode* syntax = nullptr;
    bool allowMethodGroup = false;
    bool allowEvent = false;
    bool allowMove = false;
    bool allowConstructorPropertyWrite = false;

    std::uint8_t optionMask() const noexcept;
};
