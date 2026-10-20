#pragma once

struct BreakValue final
{
    const ast::AstNode* syntax = nullptr;
    ExpressionResult value;
};
