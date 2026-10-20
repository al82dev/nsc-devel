#pragma once

#include "nativesharp/Parser/Parser.h"

#include <vector>

namespace nativesharp::parser
{
class Parser::ExpressionIterationMachine final
{
public:
    ExpressionIterationMachine(Parser& parser, bool conditionalOnly) noexcept;

    ast::AstNode* parse();

private:
    enum class WorkKind { Assignment, Conditional, Deliver };
    enum class ContinuationKind {
        AssignmentTail,
        FinishAssignment,
        ExpressionLambda,
        ConditionalTrue,
        ConditionalFalse
    };

    struct Continuation final
    {
        ContinuationKind kind;
        ast::AstNode* first = nullptr;
        ast::AstNode* second = nullptr;
        lexer::TokenKind operation = lexer::TokenKind::Invalid;
        bool reference = false;
    };

    void parseAssignment();
    void parseConditional();
    void deliver();
    void deliverAssignmentTail(const Continuation& continuation);
    void deliverContinuation(const Continuation& continuation);
    void buildAssignment(const Continuation& continuation);
    void buildConditional(const Continuation& continuation);

    Parser& parser_;
    WorkKind work_;
    std::vector<Continuation> continuations_;
    ast::AstNode* value_ = nullptr;
};
}
