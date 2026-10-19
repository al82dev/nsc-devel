#include "ExpressionIterationMachine.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/Language3Surface.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nativesharp::parser
{
using lexer::TokenKind;
Parser::ExpressionIterationMachine::ExpressionIterationMachine(
    Parser& parser,
    bool conditionalOnly) noexcept
    : parser_(parser), work_(conditionalOnly ? WorkKind::Conditional : WorkKind::Assignment)
{
}

ast::AstNode* Parser::ExpressionIterationMachine::parse()
{
    while (work_ != WorkKind::Deliver || !continuations_.empty())
    {
        if (work_ == WorkKind::Assignment) parseAssignment();
        else if (work_ == WorkKind::Conditional) parseConditional();
        else deliver();
    }
    return value_;
}

void Parser::ExpressionIterationMachine::parseAssignment()
{
    if (!parser_.looksLikeLambdaExpression())
    {
        continuations_.push_back({ContinuationKind::AssignmentTail});
        work_ = WorkKind::Conditional;
        return;
    }

    ast::AstNode* lambda = parser_.parseLambdaExpressionHead();
    if (parser_.at(TokenKind::LeftBrace))
    {
        parser_.finishLambdaExpression(lambda, parser_.parseBlock(), true);
        value_ = lambda;
        work_ = WorkKind::Deliver;
        return;
    }
    continuations_.push_back({ContinuationKind::ExpressionLambda, lambda});
}

void Parser::ExpressionIterationMachine::parseConditional()
{
    ast::AstNode* condition = parser_.parseNullCoalescingExpression();
    if (!parser_.at(TokenKind::Question))
    {
        value_ = condition;
        work_ = WorkKind::Deliver;
        return;
    }

    parser_.consume();
    continuations_.push_back({ContinuationKind::ConditionalTrue, condition});
    work_ = WorkKind::Assignment;
}

void Parser::ExpressionIterationMachine::deliver()
{
    const Continuation continuation = continuations_.back();
    continuations_.pop_back();
    if (continuation.kind == ContinuationKind::AssignmentTail)
    {
        deliverAssignmentTail(continuation);
        return;
    }
    deliverContinuation(continuation);
}

void Parser::ExpressionIterationMachine::deliverAssignmentTail(const Continuation&)
{
    if (!lexer::isAssignmentOperator(parser_.current().kind())) return;

    const TokenKind operation = parser_.consume().kind();
    const bool referenceAssignment = operation == TokenKind::Equal &&
        parser_.atContextualKeyword("ref");
    if (referenceAssignment) parser_.consume();
    continuations_.push_back(
        {ContinuationKind::FinishAssignment, value_, nullptr, operation, referenceAssignment});
    work_ = WorkKind::Assignment;
}

void Parser::ExpressionIterationMachine::deliverContinuation(
    const Continuation& continuation)
{
    switch (continuation.kind)
    {
    case ContinuationKind::FinishAssignment:
        buildAssignment(continuation);
        return;
    case ContinuationKind::ExpressionLambda:
        parser_.finishLambdaExpression(continuation.first, value_, false);
        value_ = continuation.first;
        return;
    case ContinuationKind::ConditionalTrue:
        parser_.expect(TokenKind::Colon, "':' in a conditional expression");
        continuations_.push_back(
            {ContinuationKind::ConditionalFalse, continuation.first, value_});
        work_ = WorkKind::Assignment;
        return;
    case ContinuationKind::ConditionalFalse:
        buildConditional(continuation);
        return;
    case ContinuationKind::AssignmentTail:
        return;
    }
}

void Parser::ExpressionIterationMachine::buildAssignment(const Continuation& continuation)
{
    ast::AstNode* assignment = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::AssignmentExpression,
        parser_.join(continuation.first->range(), value_->range()));
    assignment->setTokenKind(continuation.operation);
    assignment->setFirst(continuation.first);
    assignment->setSecond(value_);
    assignment->setReference(continuation.reference);
    value_ = assignment;
}

void Parser::ExpressionIterationMachine::buildConditional(const Continuation& continuation)
{
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::ConditionalExpression,
        parser_.join(continuation.first->range(),
            value_ != nullptr ? value_->range() : continuation.first->range()));
    node->setFirst(continuation.first);
    node->setSecond(continuation.second);
    node->setThird(value_);
    value_ = node;
}
}
