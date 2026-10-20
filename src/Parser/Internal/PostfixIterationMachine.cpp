#include "PostfixIterationMachine.h"

#include "BinaryOperatorInfo.h"
#include "BinaryRightHandSide.h"
#include "ExpressionParsingSupport.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Language/ExpressionErgonomics.h"
#include "nativesharp/Language/IterationSemantics.h"
#include "nativesharp/Language/Language3Surface.h"
#include "nativesharp/Lexer/Lexer.h"

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
Parser::PostfixIterationMachine::PostfixIterationMachine(
    Parser& parser,
    bool resumable,
    ContinuationRequestKind rootKind) noexcept
    : parser_(parser), resumable_(resumable), rootKind_(rootKind)
{
}

std::size_t Parser::PostfixIterationMachine::makeSlot()
{
    const std::size_t slot = results_.size();
    results_.push_back({});
#if defined(NSC_PARSER_SCHEDULER_TEST_HOOKS)
    ++outstandingResults_;
#endif
    return slot;
}

void Parser::PostfixIterationMachine::complete(std::size_t slot, ast::AstNode* value)
{
#if defined(NSC_PARSER_SCHEDULER_TEST_HOOKS)
    if (!results_[slot].ready && outstandingResults_ != 0U) --outstandingResults_;
#endif
    results_[slot] = {value, true};
}

void Parser::PostfixIterationMachine::scheduleChild(std::size_t slot)
{
    Work expression{WorkKind::Expression, slot};
    expression.precedence = 1;
    work_.push_back(std::move(expression));
}

ast::AstNode* Parser::PostfixIterationMachine::parse()
{
    return parseRoot(WorkKind::Postfix, 0);
}

ast::AstNode* Parser::PostfixIterationMachine::parseUnary()
{
    return parseRoot(WorkKind::Unary, 0);
}

ast::AstNode* Parser::PostfixIterationMachine::parseBinary()
{
    return parseRoot(WorkKind::Expression, 5);
}

ast::AstNode* Parser::PostfixIterationMachine::parseRoot(
    WorkKind rootKind,
    std::uint8_t precedence)
{
#if defined(NSC_PARSER_SCHEDULER_TEST_HOOKS)
    ++scheduler_test_hooks::activeDepth;
    scheduler_test_hooks::maximumDepth = std::max(
        scheduler_test_hooks::maximumDepth, scheduler_test_hooks::activeDepth);
#endif
    initializeRoot(rootKind, precedence);
    while (!results_[rootSlot_].ready)
    {
        assert(!awaitingBlock_);
        Work work = std::move(work_.back());
        work_.pop_back();
        execute(std::move(work));
    }
    ast::AstNode* parsed = results_[rootSlot_].value;
#if defined(NSC_PARSER_SCHEDULER_TEST_HOOKS)
    scheduler_test_hooks::unfinishedResults = outstandingResults_;
    --scheduler_test_hooks::activeDepth;
#endif
    return parsed;
}

void Parser::PostfixIterationMachine::initializeRoot(
    WorkKind rootKind,
    std::uint8_t precedence)
{
    assert(!started_);
    rootSlot_ = makeSlot();
    Work rootWork{rootKind, rootSlot_};
    rootWork.precedence = precedence;
    work_.push_back(std::move(rootWork));
    started_ = true;
}

void Parser::PostfixIterationMachine::execute(Work work)
{
    switch (work.kind)
    {
    case WorkKind::Expression: startExpression(std::move(work)); break;
    case WorkKind::ExpressionTail: continueExpression(std::move(work)); break;
    case WorkKind::Unary: startUnary(std::move(work)); break;
    case WorkKind::FinishUnary: finishUnary(std::move(work)); break;
    case WorkKind::FinishBinary: finishBinary(std::move(work)); break;
    case WorkKind::FinishAssignment: finishAssignment(std::move(work)); break;
    case WorkKind::FinishConditionalTrue: finishConditionalTrue(std::move(work)); break;
    case WorkKind::FinishConditionalFalse: finishConditionalFalse(std::move(work)); break;
    case WorkKind::FinishTypeRelation: finishTypeRelation(std::move(work)); break;
    case WorkKind::FinishRangeEnd: finishRangeEnd(std::move(work)); break;
    case WorkKind::FinishRangeStep: finishRangeStep(std::move(work)); break;
    case WorkKind::Postfix: startPostfix(std::move(work)); break;
    case WorkKind::Primary: startPrimary(std::move(work)); break;
    case WorkKind::Loop: startLoop(std::move(work)); break;
    case WorkKind::Comprehension: startComprehension(std::move(work)); break;
    case WorkKind::FinishComprehensionBuilder:
        finishComprehensionBuilder(std::move(work)); break;
    case WorkKind::Grouping: continueGrouping(std::move(work)); break;
    case WorkKind::Collection: continueCollection(std::move(work)); break;
    case WorkKind::FinishNameOf: finishNameOf(std::move(work)); break;
    case WorkKind::FinishOverflowPolicy: finishOverflowPolicy(std::move(work)); break;
    case WorkKind::ContinueInterpolation: finishInterpolationHole(std::move(work)); break;
    case WorkKind::Call: continueCall(std::move(work)); break;
    case WorkKind::Index: continueIndex(std::move(work)); break;
    case WorkKind::With: continueWith(std::move(work)); break;
    case WorkKind::NewArguments: continueNewArguments(std::move(work)); break;
    case WorkKind::PlacementNew: continuePlacementNew(std::move(work)); break;
    case WorkKind::ArrayNew: continueArrayNew(std::move(work)); break;
    case WorkKind::ArrayInitializer: continueArrayInitializer(std::move(work)); break;
    }
}

Parser::ExpressionContinuationStatus Parser::PostfixIterationMachine::advance(
    ast::AstNode* completedBlock)
{
#if defined(NSC_PARSER_SCHEDULER_TEST_HOOKS)
    ++scheduler_test_hooks::activeDepth;
    scheduler_test_hooks::maximumDepth = std::max(
        scheduler_test_hooks::maximumDepth, scheduler_test_hooks::activeDepth);
#endif
    if (!started_)
    {
        WorkKind root = WorkKind::Expression;
        if (rootKind_ == ContinuationRequestKind::LoopExpression) root = WorkKind::Loop;
        else if (rootKind_ == ContinuationRequestKind::ComprehensionExpression)
            root = WorkKind::Comprehension;
        initializeRoot(root, 1);
    }
    else
    {
        assert(awaitingBlock_);
        awaitingBlock_ = false;
        if (awaitingComprehensionBody_)
        {
            awaitingComprehensionBody_ = false;
            finishComprehensionBody(std::move(pendingComprehension_), completedBlock);
        }
        else
        {
            finishLoopBody(std::move(pendingLoop_), completedBlock);
        }
    }
    while (!results_[rootSlot_].ready && !awaitingBlock_)
    {
        assert(!work_.empty());
        Work work = std::move(work_.back());
        work_.pop_back();
        execute(std::move(work));
    }
    const ExpressionContinuationStatus status = results_[rootSlot_].ready
        ? ExpressionContinuationStatus::Complete
        : ExpressionContinuationStatus::NeedsBlock;
#if defined(NSC_PARSER_SCHEDULER_TEST_HOOKS)
    scheduler_test_hooks::unfinishedResults = outstandingResults_;
    --scheduler_test_hooks::activeDepth;
#endif
    return status;
}

ast::AstNode* Parser::PostfixIterationMachine::result() const noexcept
{
    return rootSlot_ == noSlot ? nullptr : results_[rootSlot_].value;
}

void Parser::PostfixIterationMachine::startExpression(Work work)
{
    if (work.precedence <= 1 && parser_.looksLikeLambdaExpression())
    {
        ast::AstNode* lambda = parser_.parseLambdaExpressionHead();
        if (parser_.at(TokenKind::LeftBrace))
        {
            parser_.finishLambdaExpression(lambda, parser_.parseBlock(), true);
            complete(work.resultSlot, lambda);
            return;
        }
        const std::size_t child = makeSlot();
        Work finish{WorkKind::FinishAssignment, work.resultSlot, child, lambda};
        finish.text = "lambda";
        work_.push_back(std::move(finish));
        scheduleChild(child);
        return;
    }
    if (work.precedence <= 4 &&
        (parser_.at(TokenKind::DotDot) || parser_.at(TokenKind::DotDotEqual)))
    {
        Work range{WorkKind::FinishRangeEnd, work.resultSlot};
        range.range = parser_.consume().range();
        range.tokenKind = parser_.tokens_[parser_.index_ - 1].kind();
        range.precedence = work.precedence;
        if (rangeOperandEnds(parser_.current().kind())) { finishRangeEnd(std::move(range)); return; }
        range.childSlot = makeSlot();
        work_.push_back(range);
        Work rhs{WorkKind::Expression, range.childSlot};
        rhs.precedence = 5;
        work_.push_back(rhs);
        return;
    }
    const std::size_t child = makeSlot();
    Work tail{WorkKind::ExpressionTail, work.resultSlot, child};
    tail.precedence = work.precedence;
    work_.push_back(std::move(tail));
    work_.push_back({WorkKind::Unary, child});
}

void Parser::PostfixIterationMachine::continueExpression(Work work)
{
    ast::AstNode* left = work.node != nullptr ? work.node : results_[work.childSlot].value;
    const TokenKind operation = parser_.current().kind();
    const std::optional<BinaryOperatorInfo> binary = binaryOperatorInfo(operation);
    const std::uint8_t precedence = expressionBindingPower(operation);
    if (precedence == 0 || precedence < work.precedence ||
        (operation == TokenKind::Question && !parser_.questionBeginsConditional()))
    {
        complete(work.resultSlot, left);
        return;
    }
    if (operation == TokenKind::Question)
    {
        parser_.consume();
        const std::size_t child = makeSlot();
        work_.push_back({WorkKind::FinishConditionalTrue, work.resultSlot, child, left});
        scheduleChild(child);
        return;
    }
    if (binary.has_value() && binary->rightHandSide == BinaryRightHandSide::Type)
    {
        parser_.consume();
        Work finish{WorkKind::FinishTypeRelation, work.resultSlot, noSlot, left};
        finish.tokenKind = operation;
        finish.precedence = work.precedence;
        work_.push_back(std::move(finish));
        return;
    }
    if (operation == TokenKind::DotDot || operation == TokenKind::DotDotEqual)
    {
        Work range{WorkKind::FinishRangeEnd, work.resultSlot, noSlot, nullptr, left};
        range.range = parser_.consume().range();
        range.tokenKind = operation;
        range.precedence = work.precedence;
        if (rangeOperandEnds(parser_.current().kind())) { finishRangeEnd(std::move(range)); return; }
        range.childSlot = makeSlot();
        work_.push_back(range);
        Work rhs{WorkKind::Expression, range.childSlot};
        rhs.precedence = 5;
        work_.push_back(rhs);
        return;
    }
    parser_.consume();
    const bool reference = lexer::isAssignmentOperator(operation) && operation == TokenKind::Equal &&
        parser_.atContextualKeyword("ref");
    if (reference) parser_.consume();
    const std::size_t child = makeSlot();
    const WorkKind finishKind = lexer::isAssignmentOperator(operation)
        ? WorkKind::FinishAssignment : WorkKind::FinishBinary;
    Work finish{finishKind, work.resultSlot, child, left};
    finish.tokenKind = operation;
    finish.flag = reference;
    finish.precedence = work.precedence;
    work_.push_back(std::move(finish));
    Work rhs{WorkKind::Expression, child};
    rhs.precedence = binary.has_value()
        ? binary->rightBindingPower
        : (postfixBridgeRightAssociative(operation) ? precedence : precedence + 1);
    work_.push_back(std::move(rhs));
}

bool Parser::PostfixIterationMachine::startSimpleUnary(Work& work)
{
    const TokenKind kind = parser_.current().kind();
    const bool ordinary = kind == TokenKind::Plus || kind == TokenKind::Minus ||
        kind == TokenKind::Bang || kind == TokenKind::Tilde || kind == TokenKind::Amp ||
        kind == TokenKind::Star || kind == TokenKind::PlusPlus || kind == TokenKind::MinusMinus;
    const bool awaiting = parser_.atContextualKeyword("await") && parser_.peek().kind() != TokenKind::Equal &&
        parser_.peek().kind() != TokenKind::LeftParen;
    if (ordinary || awaiting || kind == TokenKind::Caret)
    {
        const lexer::Token& token = parser_.consume();
        const std::size_t child = makeSlot();
        Work finish{WorkKind::FinishUnary, work.resultSlot, child};
        finish.tokenKind = awaiting ? TokenKind::Invalid : kind;
        finish.range = token.range();
        finish.text = awaiting ? "await" : (kind == TokenKind::Caret ? "from-end" : "ordinary");
        work_.push_back(std::move(finish));
        work_.push_back({WorkKind::Unary, child});
        return true;
    }
    if (parser_.at(TokenKind::LeftParen) && parser_.looksLikeParenthesizedType())
    {
        Work finish{WorkKind::FinishUnary, work.resultSlot, makeSlot()};
        finish.range = parser_.consume().range();
        finish.typeSyntax = parser_.parseType();
        finish.text = "parenthesized-cast";
        parser_.expect(TokenKind::RightParen, "')'");
        work_.push_back(finish);
        work_.push_back({WorkKind::Unary, finish.childSlot});
        return true;
    }
    if (parser_.at(TokenKind::KwDelete) || parser_.at(TokenKind::KwDestroy))
    {
        const bool deleting = parser_.at(TokenKind::KwDelete);
        Work finish{WorkKind::FinishUnary, work.resultSlot, makeSlot()};
        finish.range = parser_.consume().range();
        finish.text = deleting ? "delete" : "destroy";
        if (deleting && parser_.consumeIf(TokenKind::LeftBracket))
        {
            finish.flag = true;
            parser_.expect(TokenKind::RightBracket, "']'");
        }
        work_.push_back(finish);
        work_.push_back({WorkKind::Unary, finish.childSlot});
        return true;
    }
    return false;
}

bool Parser::PostfixIterationMachine::startDelimitedUnary(Work& work)
{
    if (parser_.at(TokenKind::KwMove))
    {
        Work finish{WorkKind::FinishUnary, work.resultSlot, makeSlot()};
        finish.range = parser_.consume().range();
        finish.text = "move";
        parser_.expect(TokenKind::LeftParen, "'(' after move");
        work_.push_back(finish);
        scheduleChild(finish.childSlot);
        return true;
    }
    if (parser_.at(TokenKind::Identifier) && parser_.peek().kind() == TokenKind::Less &&
        (parser_.current().lexeme() == "static_cast" ||
         parser_.current().lexeme() == "reinterpret_cast" ||
         parser_.current().lexeme() == "const_cast"))
    {
        const lexer::Token& name = parser_.consume();
        Work finish{WorkKind::FinishUnary, work.resultSlot, makeSlot()};
        finish.range = name.range();
        finish.auxiliaryText = "cast:" + std::string(name.lexeme());
        finish.text = "named-cast";
        parser_.expect(TokenKind::Less, "'<' after named cast");
        finish.typeSyntax = parser_.parseType();
        parser_.expect(TokenKind::Greater, "'>' after named cast target type");
        parser_.expect(TokenKind::LeftParen, "'(' after named cast type");
        work_.push_back(finish);
        scheduleChild(finish.childSlot);
        return true;
    }
    if (parser_.at(TokenKind::KwStackalloc))
    {
        Work finish{WorkKind::FinishUnary, work.resultSlot, makeSlot()};
        finish.range = parser_.consume().range();
        finish.text = "stackalloc";
        finish.typeSyntax = parser_.parseAllocationElementType();
        parser_.expect(TokenKind::LeftBracket, "'['");
        work_.push_back(finish);
        scheduleChild(finish.childSlot);
        return true;
    }
    return false;
}

bool Parser::PostfixIterationMachine::startAtomicUnary(Work& work)
{
    if (parser_.at(TokenKind::KwOffsetof))
        { complete(work.resultSlot, parser_.parseOffsetofExpression()); return true; }
    if (parser_.at(TokenKind::KwDefault))
        { complete(work.resultSlot, parser_.parseDefaultValueExpression()); return true; }
    if (parser_.at(TokenKind::KwSizeof) || parser_.at(TokenKind::KwAlignof))
    {
        const bool align = parser_.at(TokenKind::KwAlignof);
        Work finish{WorkKind::FinishUnary, work.resultSlot};
        finish.range = parser_.consume().range();
        finish.text = align ? "alignof" : "sizeof";
        parser_.expect(TokenKind::LeftParen, "'('");
        const std::size_t checkpoint = parser_.index_;
        if (parser_.canStartTypeAt(parser_.index_) && parser_.looksLikeSizeofTypeForm(parser_.index_))
        {
            finish.typeSyntax = parser_.parseType();
            parser_.expect(TokenKind::RightParen, "')'");
            ast::AstNode* node = parser_.arena_.make<ast::AstNode>(align
                    ? ast::AstNodeKind::AlignofTypeExpression : ast::AstNodeKind::SizeofTypeExpression,
                parser_.tokenRangeFrom(checkpoint));
            node->setTypeSyntax(finish.typeSyntax);
            node->setRange(parser_.join(finish.range, parser_.tokens_[parser_.index_ - 1].range()));
            complete(work.resultSlot, node);
            return true;
        }
        finish.childSlot = makeSlot();
        work_.push_back(finish);
        scheduleChild(finish.childSlot);
        return true;
    }
    return false;
}

void Parser::PostfixIterationMachine::startUnary(Work work)
{
    if (startSimpleUnary(work) || startDelimitedUnary(work) || startAtomicUnary(work)) return;
    if (parser_.at(TokenKind::KwNew)) { startNewExpression(std::move(work)); return; }
    work_.push_back({WorkKind::Postfix, work.resultSlot});
}

ast::AstNode* Parser::PostfixIterationMachine::buildUnaryNode(
    Work& work,
    ast::AstNode* operand)
{
    ast::AstNodeKind kind = ast::AstNodeKind::UnaryExpression;
    if (work.text == "await") kind = ast::AstNodeKind::AwaitExpression;
    else if (work.text == "from-end") kind = ast::AstNodeKind::FromEndExpression;
    else if (work.text == "parenthesized-cast") kind = ast::AstNodeKind::CastExpression;
    else if (work.text == "delete") kind = ast::AstNodeKind::DeleteExpression;
    else if (work.text == "destroy") kind = ast::AstNodeKind::DestroyExpression;
    const SourceRange range = work.text == "await" ? work.range :
        parser_.join(work.range, operand != nullptr ? operand->range() : work.range);
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(kind, range);
    if (work.text == "ordinary" || work.text == "from-end") node->setTokenKind(work.tokenKind);
    if (work.text == "parenthesized-cast") node->setTypeSyntax(work.typeSyntax);
    if (work.text == "delete") node->setFlag(work.flag);
    node->setFirst(operand);
    return node;
}

void Parser::PostfixIterationMachine::finishDelimitedUnary(Work work)
{
    ast::AstNode* operand = results_[work.childSlot].value;
    ast::AstNodeKind kind = ast::AstNodeKind::MoveExpression;
    std::string_view closing = "')' after move operand";
    if (work.text == "named-cast")
        { kind = ast::AstNodeKind::CastExpression; closing = "')' after named cast operand"; }
    else if (work.text == "sizeof")
        { kind = ast::AstNodeKind::SizeofExpression; closing = "')'"; }
    else if (work.text == "alignof")
        { kind = ast::AstNodeKind::AlignofExpression; closing = "')'"; }
    else if (work.text == "stackalloc")
        { kind = ast::AstNodeKind::StackallocExpression; closing = "']'"; }
    parser_.expect(work.text == "stackalloc" ? TokenKind::RightBracket : TokenKind::RightParen,
        std::string(closing));
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(kind,
        parser_.join(work.range, parser_.tokens_[parser_.index_ - 1].range()));
    node->setFirst(operand);
    if (work.typeSyntax != nullptr) node->setTypeSyntax(work.typeSyntax);
    if (!work.auxiliaryText.empty()) node->setAuxiliaryText(work.auxiliaryText);
    complete(work.resultSlot, node);
}

void Parser::PostfixIterationMachine::finishUnary(Work work)
{
    if (work.text == "move" || work.text == "named-cast" || work.text == "sizeof" ||
        work.text == "alignof" || work.text == "stackalloc")
    {
        finishDelimitedUnary(std::move(work));
        return;
    }
    complete(work.resultSlot, buildUnaryNode(work, results_[work.childSlot].value));
}

void Parser::PostfixIterationMachine::finishBinary(Work work)
{
    ast::AstNode* right = results_[work.childSlot].value;
    const std::optional<BinaryOperatorInfo> binary = binaryOperatorInfo(work.tokenKind);
    const ast::AstNodeKind kind = work.tokenKind == TokenKind::QuestionQuestion
        ? ast::AstNodeKind::NullCoalescingExpression
        : (binary.has_value() ? binary->resultKind : ast::AstNodeKind::BinaryExpression);
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        kind, parser_.join(work.node->range(), right->range()));
    node->setTokenKind(work.tokenKind);
    node->setFirst(work.node);
    node->setSecond(right);
    Work tail{WorkKind::ExpressionTail, work.resultSlot, noSlot, node};
    tail.precedence = work.precedence;
    work_.push_back(std::move(tail));
}

void Parser::PostfixIterationMachine::finishAssignment(Work work)
{
    ast::AstNode* value = results_[work.childSlot].value;
    if (work.text == "lambda")
    {
        parser_.finishLambdaExpression(work.node, value, false);
        complete(work.resultSlot, work.node);
        return;
    }
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::AssignmentExpression, parser_.join(work.node->range(), value->range()));
    node->setTokenKind(work.tokenKind);
    node->setFirst(work.node);
    node->setSecond(value);
    node->setReference(work.flag);
    complete(work.resultSlot, node);
}

void Parser::PostfixIterationMachine::finishConditionalTrue(Work work)
{
    parser_.expect(TokenKind::Colon, "':' in a conditional expression");
    Work finish{WorkKind::FinishConditionalFalse, work.resultSlot, makeSlot(), work.node,
        results_[work.childSlot].value};
    work_.push_back(finish);
    scheduleChild(finish.childSlot);
}

void Parser::PostfixIterationMachine::finishConditionalFalse(Work work)
{
    ast::AstNode* value = results_[work.childSlot].value;
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(ast::AstNodeKind::ConditionalExpression,
        parser_.join(work.node->range(), value != nullptr ? value->range() : work.node->range()));
    node->setFirst(work.node);
    node->setSecond(work.first);
    node->setThird(value);
    complete(work.resultSlot, node);
}

void Parser::PostfixIterationMachine::finishTypeRelation(Work work)
{
    ast::TypeSyntax* type = parser_.parseType();
    if (type == nullptr) { complete(work.resultSlot, work.node); return; }
    const std::optional<BinaryOperatorInfo> binary = binaryOperatorInfo(work.tokenKind);
    const ast::AstNodeKind kind = binary.has_value()
        ? binary->resultKind : ast::AstNodeKind::BinaryExpression;
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        kind, parser_.join(work.node->range(), type->range()));
    node->setTokenKind(work.tokenKind);
    node->setTypeSyntax(type);
    node->setFirst(work.node);
    Work tail{WorkKind::ExpressionTail, work.resultSlot, noSlot, node};
    tail.precedence = work.precedence;
    work_.push_back(std::move(tail));
}

void Parser::PostfixIterationMachine::finishRangeEnd(Work work)
{
    ast::AstNode* end = work.childSlot == noSlot ? nullptr : results_[work.childSlot].value;
    if (parser_.atContextualKeyword("step"))
    {
        const SourceRange stepKeyword = parser_.consume().range();
        if (rangeOperandEnds(parser_.current().kind()))
            parser_.diagnostics_.report(diagnostics::DiagnosticId::ExpectedExpression,
                diagnostics::DiagnosticSeverity::Error, stepKeyword,
                "expected an integral expression after range step");
        else
        {
            Work finish{WorkKind::FinishRangeStep, work.resultSlot, makeSlot(), nullptr, work.first};
            finish.node = end;
            finish.range = work.range;
            finish.tokenKind = work.tokenKind;
            finish.precedence = work.precedence;
            work_.push_back(finish);
            Work step{WorkKind::Expression, finish.childSlot};
            step.precedence = 5;
            work_.push_back(step);
            return;
        }
    }
    Work finish{WorkKind::FinishRangeStep, work.resultSlot, noSlot, end, work.first};
    finish.range = work.range;
    finish.tokenKind = work.tokenKind;
    finish.precedence = work.precedence;
    finishRangeStep(std::move(finish));
}

void Parser::PostfixIterationMachine::finishRangeStep(Work work)
{
    ast::AstNode* step = work.childSlot == noSlot ? nullptr : results_[work.childSlot].value;
    const bool inclusive = work.tokenKind == TokenKind::DotDotEqual;
    if (inclusive && work.node == nullptr)
        parser_.diagnostics_.report(diagnostics::DiagnosticId::InvalidIndex,
            diagnostics::DiagnosticSeverity::Error, work.range,
            "an inclusive range requires an end endpoint");
    ast::AstNode* range = parser_.arena_.make<ast::AstNode>(ast::AstNodeKind::RangeExpression,
        SourceRange(work.first != nullptr ? work.first->range().begin() : work.range.begin(),
            step != nullptr ? step->range().end() :
            (work.node != nullptr ? work.node->range().end() : work.range.end())));
    range->setTokenKind(work.tokenKind);
    range->setFirst(work.first);
    range->setSecond(work.node);
    range->setThird(step);
    range->setFlag(inclusive);
    validateConstantRange(range);
    Work tail{WorkKind::ExpressionTail, work.resultSlot, noSlot, range};
    tail.precedence = work.precedence;
    work_.push_back(std::move(tail));
}

void Parser::PostfixIterationMachine::validateConstantRange(ast::AstNode* range) const
{
    const std::optional<std::int64_t> begin = signedIntegerLiteral(range->first());
    const std::optional<std::int64_t> end = signedIntegerLiteral(range->second());
    const std::optional<std::int64_t> step = range->third() != nullptr
        ? signedIntegerLiteral(range->third()) : std::optional<std::int64_t>(1);
    if (!begin.has_value() || !end.has_value() || !step.has_value()) return;

    std::string error;
    if (language::lazyRangeCount({*begin, *end, *step, range->flag()}, error).has_value()) return;
    parser_.diagnostics_.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        range->range(),
        error);
}

void Parser::PostfixIterationMachine::schedulePrimaryArgument(
    Work work,
    bool tupleElement)
{
    work.childSlot = makeSlot();
    work.flag = false;
    work.text.clear();
    work.auxiliaryText.clear();
    work.tokenKind = TokenKind::Invalid;
    if (tupleElement && parser_.at(TokenKind::DotDot))
    {
        work.tokenKind = TokenKind::DotDot;
        work.range = parser_.consume().range();
    }
    else
    {
        if (parser_.at(TokenKind::Identifier) && parser_.peek().kind() == TokenKind::Colon)
        {
            work.auxiliaryText = std::string(parser_.consume().lexeme());
            parser_.consume();
        }
        const bool reference = (parser_.atContextualKeyword("ref") ||
            parser_.atContextualKeyword("out") || parser_.at(TokenKind::KwIn)) &&
            parser_.peek().kind() != TokenKind::Comma &&
            parser_.peek().kind() != TokenKind::RightParen;
        if (reference)
        {
            const lexer::Token& keyword = parser_.consume();
            work.range = keyword.range();
            work.text = keyword.kind() == TokenKind::KwIn
                ? "in" : std::string(keyword.lexeme());
            work.flag = true;
        }
    }
    const std::size_t childSlot = work.childSlot;
    work_.push_back(std::move(work));
    scheduleChild(childSlot);
}

ast::AstNode* Parser::PostfixIterationMachine::finishPrimaryArgument(Work& work)
{
    ast::AstNode* argument = finishArgument(work);
    if (work.tokenKind != TokenKind::DotDot) return argument;
    ast::AstNode* spread = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::SpreadElementExpression,
        parser_.join(work.range, argument != nullptr ? argument->range() : work.range));
    spread->setTokenKind(TokenKind::DotDot);
    spread->setFirst(argument);
    return spread;
}

void Parser::PostfixIterationMachine::continueGrouping(Work work)
{
    ast::AstNode* argument = finishPrimaryArgument(work);
    if (work.node == nullptr)
    {
        if (!parser_.consumeIf(TokenKind::Comma))
        {
            parser_.expect(TokenKind::RightParen, "')'");
            if (argument != nullptr)
                argument->setRange(SourceRange(
                    work.outerRange.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
            complete(work.resultSlot, argument);
            return;
        }
        work.node = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::TupleExpression, work.outerRange);
        if (argument != nullptr) work.node->addChild(argument);
        if (parser_.at(TokenKind::RightParen) || parser_.at(TokenKind::EndOfFile))
        {
            parser_.expect(TokenKind::RightParen, "')' after tuple");
            work.node->setRange(SourceRange(
                work.outerRange.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
            complete(work.resultSlot, work.node);
            return;
        }
        schedulePrimaryArgument(std::move(work), true);
        return;
    }
    if (argument != nullptr) work.node->addChild(argument);
    const bool comma = parser_.consumeIf(TokenKind::Comma);
    if (!comma || parser_.at(TokenKind::RightParen) || parser_.at(TokenKind::EndOfFile))
    {
        parser_.expect(TokenKind::RightParen, "')' after tuple");
        work.node->setRange(SourceRange(
            work.outerRange.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
        complete(work.resultSlot, work.node);
        return;
    }
    schedulePrimaryArgument(std::move(work), true);
}

void Parser::PostfixIterationMachine::continueCollection(Work work)
{
    ast::AstNode* element = results_[work.childSlot].value;
    if (work.tokenKind == TokenKind::DotDot)
    {
        ast::AstNode* spread = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::SpreadElementExpression,
            parser_.join(work.range, element != nullptr ? element->range() : work.range));
        spread->setTokenKind(TokenKind::DotDot);
        spread->setFirst(element);
        element = spread;
    }
    if (element != nullptr) work.node->addChild(element);
    if (!parser_.consumeIf(TokenKind::Comma) || parser_.at(TokenKind::RightBracket) ||
        parser_.at(TokenKind::EndOfFile))
    {
        parser_.expect(TokenKind::RightBracket, "']' after collection expression");
        work.node->setRange(SourceRange(
            work.outerRange.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
        complete(work.resultSlot, work.node);
        return;
    }
    work.tokenKind = TokenKind::Invalid;
    if (parser_.at(TokenKind::DotDot))
    {
        work.tokenKind = TokenKind::DotDot;
        work.range = parser_.consume().range();
    }
    work.childSlot = makeSlot();
    const std::size_t childSlot = work.childSlot;
    work_.push_back(std::move(work));
    scheduleChild(childSlot);
}

void Parser::PostfixIterationMachine::finishNameOf(Work work)
{
    ast::AstNode* operand = results_[work.childSlot].value;
    const SourceRange end = parser_.current().range();
    parser_.expect(TokenKind::RightParen, "')' after 'nameof' argument");
    ast::AstNode* expression = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::NameOfExpression, parser_.join(work.range, end));
    expression->setFirst(operand);
    const ast::AstNode* named = operand;
    while (named != nullptr && (named->kind() == ast::AstNodeKind::CallExpression ||
        named->kind() == ast::AstNodeKind::IndexExpression)) named = named->first();
    if (named != nullptr && (named->kind() == ast::AstNodeKind::IdentifierExpression ||
        named->kind() == ast::AstNodeKind::MemberExpression ||
        named->kind() == ast::AstNodeKind::PointerMemberExpression ||
        named->kind() == ast::AstNodeKind::NullConditionalMemberExpression))
    {
        expression->setText(named->text());
    }
    complete(work.resultSlot, expression);
}

void Parser::PostfixIterationMachine::finishOverflowPolicy(Work work)
{
    ast::AstNode* operand = results_[work.childSlot].value;
    parser_.expect(TokenKind::RightParen, "')' after '" + work.text + "' operand");
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::OverflowPolicyExpression,
        parser_.join(work.range, parser_.tokens_[parser_.index_ - 1U].range()));
    node->setFirst(operand);
    node->setAuxiliaryText(std::move(work.text));
    complete(work.resultSlot, node);
}

void Parser::PostfixIterationMachine::enterInterpolationFragment(InterpolationFrame& frame)
{
    const language::InterpolationPart& part = frame.parts[frame.partIndex];
    const lexer::Token& token = *frame.token;
    const std::uint32_t tokenOffset = token.range().begin().offset();
    const std::uint64_t requestedOffset =
        static_cast<std::uint64_t>(tokenOffset) + 2U + part.expressionOffset;
    const std::size_t prefix = requestedOffset <=
            static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())
        ? static_cast<std::size_t>(requestedOffset) : static_cast<std::size_t>(tokenOffset);
    std::string fragment(prefix, ' ');
    fragment += part.text;
    frame.fragmentSource = std::make_unique<source::SourceFile>(
        token.range().begin().fileId(),
        frame.runtime ? "<runtime-interpolation>" : "<interpolation-hole>",
        std::move(fragment));
    lexer::Lexer lexer(*frame.fragmentSource, parser_.diagnostics_);
    frame.fragmentTokens = lexer.lexAll();
    frame.outerTokens = parser_.tokens_;
    frame.outerIndex = parser_.index_;
    frame.outerQuestionIndex = std::move(parser_.conditionalQuestionIndex_);
    frame.outerComprehensionDelimiterIndex =
        std::move(parser_.comprehensionDelimiterIndex_);
    frame.outerPendingGenericGreater = parser_.pendingGenericGreater_;
    parser_.tokens_ = std::span<const lexer::Token>(frame.fragmentTokens);
    parser_.index_ = 0;
    parser_.pendingGenericGreater_ = 0;
    parser_.buildConditionalQuestionIndex();
    parser_.buildComprehensionDelimiterIndex();
}

void Parser::PostfixIterationMachine::restoreInterpolationParser(InterpolationFrame& frame)
{
    parser_.tokens_ = frame.outerTokens;
    parser_.index_ = frame.outerIndex;
    parser_.conditionalQuestionIndex_ = std::move(frame.outerQuestionIndex);
    parser_.comprehensionDelimiterIndex_ =
        std::move(frame.outerComprehensionDelimiterIndex);
    parser_.pendingGenericGreater_ = frame.outerPendingGenericGreater;
    frame.fragmentTokens.clear();
    frame.fragmentSource.reset();
}

void Parser::PostfixIterationMachine::scheduleInterpolation(
    std::size_t frameIndex,
    std::size_t resultSlot)
{
    InterpolationFrame& frame = interpolationFrames_[frameIndex];
    while (frame.partIndex < frame.parts.size())
    {
        const language::InterpolationPart& part = frame.parts[frame.partIndex];
        if (part.kind == language::InterpolationPartKind::Hole)
        {
            Work finish{WorkKind::ContinueInterpolation, resultSlot, makeSlot()};
            finish.frameIndex = frameIndex;
            const std::size_t childSlot = finish.childSlot;
            enterInterpolationFragment(frame);
            work_.push_back(std::move(finish));
            scheduleChild(childSlot);
            return;
        }
        if (frame.runtime && !part.text.empty())
        {
            ast::AstNode* piece = parser_.makeInterpolationStringLiteral(*frame.token, part.text);
            frame.runtimeResult = parser_.makeBinaryExpression(
                TokenKind::Plus, frame.runtimeResult, piece);
            frame.runtimeResult->setRange(frame.token->range());
        }
        ++frame.partIndex;
    }
    complete(resultSlot, frame.runtime ? frame.runtimeResult : frame.staticNode);
}

void Parser::PostfixIterationMachine::finishInterpolationHole(Work work)
{
    InterpolationFrame& frame = interpolationFrames_[work.frameIndex];
    ast::AstNode* hole = results_[work.childSlot].value;
    const bool hasTrailingTokens = !parser_.at(TokenKind::EndOfFile);
    restoreInterpolationParser(frame);
    if (frame.runtime && hasTrailingTokens)
        parser_.diagnostics_.report(diagnostics::DiagnosticId::ExpectedExpression,
            diagnostics::DiagnosticSeverity::Error, frame.token->range(),
            "interpolation hole must contain exactly one expression");
    if (hole == nullptr) hole = parser_.makeErrorExpression();
    if (frame.runtime)
    {
        ast::AstNode* piece = parser_.formatRuntimeInterpolationHole(
            *frame.token, frame.parts[frame.partIndex], hole);
        frame.runtimeResult = parser_.makeBinaryExpression(
            TokenKind::Plus, frame.runtimeResult, piece);
        frame.runtimeResult->setRange(frame.token->range());
    }
    else frame.staticNode->addChild(hole);
    ++frame.partIndex;
    scheduleInterpolation(work.frameIndex, work.resultSlot);
}

void Parser::PostfixIterationMachine::startInterpolationToken(
    Work work,
    const lexer::Token& token)
{
    std::string value;
    if (const auto* decoded = std::get_if<std::string>(&token.value())) value = *decoded;
    parser_.consume();
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::InterpolatedStringExpression, token.range());
    node->setTokenKind(token.kind());
    node->setText(value);
    interpolationFrames_.emplace_back();
    const std::size_t frameIndex = interpolationFrames_.size() - 1U;
    InterpolationFrame& frame = interpolationFrames_.back();
    frame.token = token;
    frame.staticNode = node;
    std::string error;
    if (!language::parseStaticInterpolation(value, frame.parts, error))
    {
        parser_.diagnostics_.report(diagnostics::DiagnosticId::InvalidStringLiteral,
            diagnostics::DiagnosticSeverity::Error, token.range(), std::move(error));
        node->setIntegerValue(0U);
        complete(work.resultSlot, node);
        return;
    }
    node->setIntegerValue(frame.parts.size());
    frame.runtime = !std::ranges::all_of(
        frame.parts, language::builtinStaticInterpolationHoleSupported);
    if (frame.runtime)
        frame.runtimeResult = parser_.makeInterpolationStringLiteral(token, "");
    scheduleInterpolation(frameIndex, work.resultSlot);
}

void Parser::PostfixIterationMachine::startLoop(Work work)
{
    work.beginIndex = parser_.index_;
    parser_.consume();
    if (parser_.at(TokenKind::Identifier) && parser_.peek().kind() == TokenKind::LeftBrace)
        work.text = std::string(parser_.consume().lexeme());
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "expected '{' after loop expression" +
                (work.text.empty() ? std::string{} : " label '" + work.text + "'"));
    }
    if (resumable_)
    {
        pendingLoop_ = std::move(work);
        awaitingBlock_ = true;
        return;
    }
    finishLoopBody(std::move(work), parser_.parseBlock());
}

void Parser::PostfixIterationMachine::finishLoopBody(
    Work work,
    ast::AstNode* body)
{
    ast::AstNode* loop = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::LoopExpression, parser_.tokenRangeFrom(work.beginIndex));
    loop->setFirst(body);
    loop->setText(std::move(work.text));
    complete(work.resultSlot, loop);
}

void Parser::PostfixIterationMachine::startComprehension(Work work)
{
    work.beginIndex = parser_.index_;
    work.range = parser_.consume().range();
    parser_.expect(TokenKind::LeftParen, "'(' after comprehend");
    work.kind = WorkKind::FinishComprehensionBuilder;
    work.childSlot = makeSlot();
    const std::size_t builderSlot = work.childSlot;
    work_.push_back(std::move(work));
    scheduleChild(builderSlot);
}

void Parser::PostfixIterationMachine::finishComprehensionBuilder(Work work)
{
    work.node = results_[work.childSlot].value;
    parser_.expect(TokenKind::RightParen, "')' after comprehension builder");
    work.ordinal = parser_.nextComprehensionTemporary_++;
    work.text = "$__comprehension_builder_" + std::to_string(work.ordinal);
    work.label = "$__comprehension_loop_" + std::to_string(work.ordinal);
    parser_.comprehensionContexts_.push_back({work.text, work.label});

    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "expected '{' after comprehension builder");
        ast::AstNode* body = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement, work.range);
        finishComprehensionBody(std::move(work), body);
        return;
    }
    if (resumable_)
    {
        pendingComprehension_ = std::move(work);
        awaitingComprehensionBody_ = true;
        awaitingBlock_ = true;
        return;
    }
    finishComprehensionBody(std::move(work), parser_.parseBlock());
}

void Parser::PostfixIterationMachine::finishComprehensionBody(
    Work work,
    ast::AstNode* body)
{
    assert(!parser_.comprehensionContexts_.empty());
    assert(parser_.comprehensionContexts_.back().builderName == work.text);
    assert(parser_.comprehensionContexts_.back().loopLabel == work.label);
    parser_.comprehensionContexts_.pop_back();

    const SourceRange range = parser_.tokenRangeFrom(work.beginIndex);
    ast::AstNode* loopBody = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, range);
    ast::TypeSyntax* inferred = parser_.arena_.make<ast::TypeSyntax>(
        range, TokenKind::KwVar, std::string("var"));
    ast::AstNode* builderDeclaration = parser_.makeVariableDeclaration(
        inferred, work.text, work.node, false);
    builderDeclaration->setAuxiliaryText("local:comprehension-builder");
    loopBody->addChild(builderDeclaration);
    loopBody->addChild(body);

    ast::AstNode* finish = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BreakStatement, range);
    finish->setText(work.label);
    ast::AstNode* result = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::MoveExpression, range);
    result->setFirst(parser_.makeIdentifierExpression(range, work.text));
    result->setAuxiliaryText("comprehension-result");
    finish->setFirst(result);
    loopBody->addChild(finish);

    ast::AstNode* loop = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::LoopExpression, range);
    loop->setText(work.label);
    loop->setAuxiliaryText("comprehension:explicit-builder");
    loop->setFirst(loopBody);
    complete(work.resultSlot, loop);
}

bool Parser::PostfixIterationMachine::startReservedPrimary(Work& work)
{
    if (parser_.startsParenthesizedBlock("match"))
        { complete(work.resultSlot, parser_.parseMatchExpression()); return true; }
    if (parser_.atContextualKeyword("loop") &&
        (parser_.peek().kind() == TokenKind::LeftBrace ||
         (parser_.peek().kind() == TokenKind::Identifier &&
          parser_.peek(2).kind() == TokenKind::LeftBrace)))
        { startLoop(std::move(work)); return true; }
    if (parser_.comprehensionExpressionStartsHere())
        { startComprehension(std::move(work)); return true; }
    return false;
}

bool Parser::PostfixIterationMachine::startKeywordPrimary(Work& work)
{
    if (parser_.atContextualKeyword("nameof") && parser_.peek().kind() == TokenKind::LeftParen)
    {
        if (parser_.nameOfHasQualifiedGenericOperand())
            { complete(work.resultSlot, parser_.parseNameOfExpression()); return true; }
        work.range = parser_.consume().range();
        parser_.expect(TokenKind::LeftParen, "'(' after 'nameof'");
        work.kind = WorkKind::FinishNameOf;
        work.childSlot = makeSlot();
        const std::size_t childSlot = work.childSlot;
        work_.push_back(std::move(work)); scheduleChild(childSlot); return true;
    }
    if ((parser_.atContextualKeyword("checked") || parser_.atContextualKeyword("saturating") ||
         parser_.atContextualKeyword("unchecked") || parser_.atContextualKeyword("wrapping")) &&
        parser_.peek().kind() == TokenKind::LeftParen)
    {
        const lexer::Token& policy = parser_.consume();
        work.kind = WorkKind::FinishOverflowPolicy;
        work.range = policy.range(); work.text = std::string(policy.lexeme());
        parser_.expect(TokenKind::LeftParen, "'(' after '" + work.text + "'");
        work.childSlot = makeSlot(); const std::size_t childSlot = work.childSlot;
        work_.push_back(std::move(work)); scheduleChild(childSlot); return true;
    }
    return false;
}

bool Parser::PostfixIterationMachine::startAggregatePrimary(Work& work)
{
    if (parser_.at(TokenKind::LeftBracket))
    {
        work.kind = WorkKind::Collection; work.outerRange = parser_.consume().range();
        work.node = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::CollectionExpression, work.outerRange);
        work.node->setAuxiliaryText("inferred-array-initializer");
        if (parser_.at(TokenKind::RightBracket))
        {
            parser_.consume(); work.node->setRange(parser_.join(
                work.outerRange, parser_.tokens_[parser_.index_ - 1U].range()));
            complete(work.resultSlot, work.node); return true;
        }
        work.tokenKind = parser_.at(TokenKind::DotDot) ? parser_.consume().kind() : TokenKind::Invalid;
        if (work.tokenKind == TokenKind::DotDot)
            work.range = parser_.tokens_[parser_.index_ - 1U].range();
        work.childSlot = makeSlot(); const std::size_t childSlot = work.childSlot;
        work_.push_back(std::move(work)); scheduleChild(childSlot); return true;
    }
    if (parser_.at(TokenKind::LeftParen))
    {
        work.kind = WorkKind::Grouping; work.outerRange = parser_.consume().range();
        if (parser_.at(TokenKind::RightParen))
        {
            ast::AstNode* tuple = parser_.arena_.make<ast::AstNode>(
                ast::AstNodeKind::TupleExpression,
                parser_.join(work.outerRange, parser_.consume().range()));
            complete(work.resultSlot, tuple); return true;
        }
        schedulePrimaryArgument(std::move(work), false); return true;
    }
    return false;
}

void Parser::PostfixIterationMachine::startPrimary(Work work)
{
    if (startReservedPrimary(work) || startKeywordPrimary(work) || startAggregatePrimary(work))
        return;
    const lexer::Token& token = parser_.current();
    const auto kind = parser_.classifyPrimaryToken(token.kind());
    if (kind == ast::AstNodeKind::InterpolatedStringExpression)
        { startInterpolationToken(std::move(work), token); return; }
    if (kind.has_value())
        { complete(work.resultSlot, parser_.makeTokenPrimaryExpression(token, *kind)); return; }
    parser_.diagnostics_.report(diagnostics::DiagnosticId::ExpectedExpression,
        diagnostics::DiagnosticSeverity::Error, token.range(), "expected expression");
    if (!parser_.at(TokenKind::EndOfFile)) parser_.consume();
    complete(work.resultSlot, parser_.makeErrorExpression());
}

void Parser::PostfixIterationMachine::startPostfix(Work work)
{
    if (work.node == nullptr && work.childSlot == noSlot &&
        !parser_.looksLikeClosedGenericTypeReference())
    {
        work.childSlot = makeSlot();
        const std::size_t primarySlot = work.childSlot;
        work_.push_back(std::move(work));
        work_.push_back({WorkKind::Primary, primarySlot});
        return;
    }
    ast::AstNode* expression = work.node;
    if (expression == nullptr && work.childSlot != noSlot)
        expression = results_[work.childSlot].value;
    if (expression == nullptr) expression = parser_.parsePostfixBaseExpression();
    if (expression == nullptr)
    {
        work.childSlot = makeSlot();
        const std::size_t primarySlot = work.childSlot;
        work_.push_back(std::move(work));
        work_.push_back({WorkKind::Primary, primarySlot});
        return;
    }
    std::vector<ast::TypeSyntax*> genericArguments;
    const bool explicitGenerics = parser_.looksLikeGenericCallTypeArguments();
    if (explicitGenerics) parser_.parseTypeArgumentList(genericArguments);
    if (parser_.consumeIf(TokenKind::LeftParen))
    {
        ast::AstNode* call = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::CallExpression, expression->range());
        call->setFirst(expression);
        call->setFlag(explicitGenerics);
        for (ast::TypeSyntax* type : genericArguments) call->addTypeSyntax(type);
        work_.push_back({WorkKind::Call, work.resultSlot, noSlot, call});
        return;
    }
    if (!genericArguments.empty())
    {
        parser_.diagnostics_.report(diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error, parser_.current().range(),
            "generic type arguments in an expression must be followed by a call in Stage 44");
        complete(work.resultSlot, expression);
        return;
    }
    continuePostfix(std::move(work), expression);
}

void Parser::PostfixIterationMachine::continuePostfix(
    Work work,
    ast::AstNode* expression)
{
    work.node = expression;
    if (parser_.consumeIf(TokenKind::LeftBracket))
        work_.push_back(Work{WorkKind::Index, work.resultSlot, noSlot, expression});
    else if (parser_.at(TokenKind::QuestionBracket))
    {
        parser_.consume();
        Work index{WorkKind::Index, work.resultSlot, noSlot, expression};
        index.flag = true;
        work_.push_back(std::move(index));
    }
    else if (parser_.at(TokenKind::QuestionDot))
        work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot,
            parser_.parseNullConditionalMemberSuffix(expression)});
    else if (parser_.at(TokenKind::Dot) || parser_.at(TokenKind::Arrow))
        work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot,
            parser_.parseMemberSuffix(expression)});
    else if (parser_.at(TokenKind::PlusPlus) || parser_.at(TokenKind::MinusMinus))
        work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot,
            parser_.parsePostfixOperatorSuffix(expression)});
    else if (parser_.at(TokenKind::Question) && !parser_.questionBeginsConditional())
        work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot,
            parser_.parseResultPropagationSuffix(expression)});
    else if (parser_.atContextualKeyword("with"))
    {
        const SourceRange keyword = parser_.consume().range();
        ast::AstNode* with = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::WithExpression, parser_.join(expression->range(), keyword));
        with->setFirst(expression);
        parser_.expect(TokenKind::LeftBrace, "'{' after with");
        work_.push_back({WorkKind::With, work.resultSlot, noSlot, with});
    }
    else complete(work.resultSlot, expression);
}

ast::AstNode* Parser::PostfixIterationMachine::finishArgument(Work& work)
{
    ast::AstNode* argument = results_[work.childSlot].value;
    if (work.flag)
    {
        ast::AstNode* wrapper = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression, work.range);
        wrapper->setTokenKind(TokenKind::Amp);
        wrapper->setFirst(argument);
        wrapper->setAuxiliaryText("pass:" + work.text);
        if (argument != nullptr) wrapper->setRange(parser_.join(work.range, argument->range()));
        argument = wrapper;
    }
    if (argument != nullptr && !work.auxiliaryText.empty())
        argument->setArgumentName(work.auxiliaryText);
    return argument;
}

void Parser::PostfixIterationMachine::finishCall(Work& work)
{
    parser_.expect(TokenKind::RightParen, "')'");
    work.node->setRange(SourceRange(work.node->first()->range().begin(),
        parser_.tokens_[parser_.index_ - 1].range().end()));
    ast::AstNode* callee = work.node->first();
    if (callee->kind() == ast::AstNodeKind::IdentifierExpression && callee->typeSyntax() == nullptr)
    {
        const std::string& name = callee->text();
        if (name == "assert" || name == "unreachable" || name == "assume" || name == "panic")
            work.node->setAuxiliaryText("builtin-" + name);
    }
    work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot, work.node});
}

void Parser::PostfixIterationMachine::continueCall(Work work)
{
    if (work.childSlot != noSlot)
    {
        ast::AstNode* argument = finishArgument(work);
        if (argument != nullptr) work.node->addChild(argument);
        work.childSlot = noSlot;
        work.text.clear();
        work.auxiliaryText.clear();
        work.flag = false;
        if (!parser_.consumeIf(TokenKind::Comma)) { finishCall(work); return; }
    }
    if (parser_.at(TokenKind::RightParen) || parser_.at(TokenKind::EndOfFile))
    { finishCall(work); return; }
    if (parser_.at(TokenKind::Identifier) && parser_.peek().kind() == TokenKind::Colon)
    {
        work.auxiliaryText = std::string(parser_.consume().lexeme());
        parser_.consume();
    }
    const bool reference = (parser_.atContextualKeyword("ref") || parser_.atContextualKeyword("out") ||
        parser_.at(TokenKind::KwIn)) && parser_.peek().kind() != TokenKind::Comma &&
        parser_.peek().kind() != TokenKind::RightParen;
    if (reference)
    {
        const lexer::Token& keyword = parser_.consume();
        work.range = keyword.range();
        work.text = keyword.kind() == TokenKind::KwIn ? "in" : std::string(keyword.lexeme());
        work.flag = true;
    }
    work.childSlot = makeSlot();
    work_.push_back(work);
    scheduleChild(work.childSlot);
}

void Parser::PostfixIterationMachine::appendIndex(Work& work, ast::AstNode* value)
{
    if (value != nullptr) work.children.push_back(value);
    work.childSlot = noSlot;
    work.first = nullptr;
}

void Parser::PostfixIterationMachine::finishIndex(Work& work)
{
    parser_.expect(TokenKind::RightBracket, "']'");
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        work.flag ? ast::AstNodeKind::NullConditionalIndexExpression
                  : ast::AstNodeKind::IndexExpression,
        SourceRange(work.node->range().begin(), parser_.tokens_[parser_.index_ - 1].range().end()));
    if (work.flag) node->setTokenKind(TokenKind::QuestionBracket);
    node->setFirst(work.node);
    if (!work.children.empty()) node->setSecond(work.children.front());
    for (ast::AstNode* index : work.children) node->addChild(index);
    work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot, node});
}

void Parser::PostfixIterationMachine::continueIndex(Work work)
{
    if (work.childSlot != noSlot)
    {
        ast::AstNode* value = results_[work.childSlot].value;
        if (work.first == nullptr && parser_.at(TokenKind::DotDot))
        {
            work.first = value;
            work.range = parser_.consume().range();
            work.text = "range";
            if (!parser_.at(TokenKind::Comma) && !parser_.at(TokenKind::RightBracket))
            {
                work.childSlot = makeSlot();
                work_.push_back(work);
                scheduleChild(work.childSlot);
                return;
            }
            value = nullptr;
        }
        if (work.text == "range")
        {
            ast::AstNode* range = parser_.arena_.make<ast::AstNode>(ast::AstNodeKind::RangeExpression,
                SourceRange(work.first != nullptr ? work.first->range().begin() : work.range.begin(),
                    value != nullptr ? value->range().end() : work.range.end()));
            range->setTokenKind(TokenKind::DotDot);
            range->setFirst(work.first);
            range->setSecond(value);
            value = range;
            work.range = {};
            work.text.clear();
        }
        appendIndex(work, value);
        if (!parser_.consumeIf(TokenKind::Comma)) { finishIndex(work); return; }
    }
    if (parser_.at(TokenKind::RightBracket) || parser_.at(TokenKind::EndOfFile))
    { finishIndex(work); return; }
    if (parser_.at(TokenKind::DotDot))
    {
        work.range = parser_.consume().range();
        work.text = "range";
        if (parser_.at(TokenKind::Comma) || parser_.at(TokenKind::RightBracket))
        {
            work.childSlot = makeSlot();
            complete(work.childSlot, nullptr);
            work_.push_back(work);
            return;
        }
    }
    work.childSlot = makeSlot();
    work_.push_back(work);
    scheduleChild(work.childSlot);
}

void Parser::PostfixIterationMachine::continueWith(Work work)
{
    if (work.childSlot != noSlot)
    {
        ast::AstNode* value = results_[work.childSlot].value;
        if (value != nullptr)
        {
            value->setArgumentName(work.text);
            work.node->addChild(value);
        }
        work.childSlot = noSlot;
        if (!parser_.consumeIf(TokenKind::Comma))
        {
            parser_.expect(TokenKind::RightBrace, "'}' after with expression");
            work.node->setRange(SourceRange(work.node->first()->range().begin(),
                parser_.tokens_[parser_.index_ - 1].range().end()));
            work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot, work.node});
            return;
        }
    }
    if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
    {
        parser_.expect(TokenKind::RightBrace, "'}' after with expression");
        work.node->setRange(SourceRange(work.node->first()->range().begin(),
            parser_.tokens_[parser_.index_ - 1].range().end()));
        work_.push_back({WorkKind::Postfix, work.resultSlot, noSlot, work.node});
        return;
    }
    if (!parser_.at(TokenKind::Identifier))
    {
        parser_.expect(TokenKind::Identifier, "member name in with expression");
        complete(work.resultSlot, work.node);
        return;
    }
    work.text = std::string(parser_.consume().lexeme());
    parser_.expect(TokenKind::Equal, "'=' after with member name");
    work.childSlot = makeSlot();
    work_.push_back(work);
    scheduleChild(work.childSlot);
}

void Parser::PostfixIterationMachine::continueNewArguments(Work work)
{
    if (work.childSlot != noSlot)
    {
        ast::AstNode* argument = finishArgument(work);
        if (argument != nullptr) work.node->addChild(argument);
        work.childSlot = noSlot;
        work.text.clear();
        work.auxiliaryText.clear();
        work.flag = false;
        if (!parser_.consumeIf(TokenKind::Comma))
        {
            parser_.expect(TokenKind::RightParen, work.label);
            work.node->setRange(SourceRange(
                work.outerRange.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
            if (work.first != nullptr) work.node->addChild(work.first);
            complete(work.resultSlot, work.node);
            return;
        }
    }
    if (parser_.at(TokenKind::RightParen) || parser_.at(TokenKind::EndOfFile))
    {
        parser_.expect(TokenKind::RightParen, work.label);
        work.node->setRange(SourceRange(
            work.outerRange.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
        if (work.first != nullptr) work.node->addChild(work.first);
        complete(work.resultSlot, work.node);
        return;
    }
    schedulePrimaryArgument(std::move(work), false);
}

void Parser::PostfixIterationMachine::continuePlacementNew(Work work)
{
    if (work.precedence == 0)
    {
        work.first = results_[work.childSlot].value;
        work.childSlot = noSlot;
        work.precedence = 1;
    }
    else if (work.precedence == 2)
    {
        work.node = results_[work.childSlot].value;
        work.childSlot = noSlot;
        work.precedence = 1;
    }
    while (parser_.consumeIf(TokenKind::Comma))
    {
        if (parser_.atContextualKeyword("trailing") && parser_.peek().kind() == TokenKind::Colon)
        {
            parser_.consume(); parser_.consume();
            work.precedence = 2; work.childSlot = makeSlot();
            const std::size_t child = work.childSlot;
            work_.push_back(std::move(work)); scheduleChild(child); return;
        }
        if (parser_.atContextualKeyword("failure") && parser_.peek().kind() == TokenKind::Colon)
        {
            parser_.consume(); parser_.consume();
            if (parser_.atContextualKeyword("panic") || parser_.atContextualKeyword("result") ||
                parser_.atContextualKeyword("throws") || parser_.at(TokenKind::KwNull))
                work.auxiliaryText = std::string(parser_.consume().lexeme());
            else parser_.expect(TokenKind::Colon,
                "'panic', 'null', 'result' or 'throws' after 'failure:'");
            continue;
        }
        parser_.expect(TokenKind::Colon, "':' after placement option");
        break;
    }
    parser_.expect(TokenKind::RightParen, "')' after placement storage");
    work.typeSyntax = parser_.parseAllocationElementType();
    ast::AstNode* creation = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::PlacementConstructionExpression, work.range);
    creation->setFirst(work.first); creation->setTypeSyntax(work.typeSyntax);
    if (work.node != nullptr) creation->setAuxiliaryText("trailing");
    if (!work.auxiliaryText.empty()) creation->setAllocatorFailureProfile(work.auxiliaryText);
    if (work.auxiliaryText == "result") parser_.attachPlacementResultType(creation, work.typeSyntax);
    if (!parser_.expect(TokenKind::LeftParen, "'(' after placement construction type"))
    {
        if (work.node != nullptr) creation->addChild(work.node);
        creation->setRange(SourceRange(
            work.range.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
        complete(work.resultSlot, creation);
        return;
    }
    Work arguments{WorkKind::NewArguments, work.resultSlot, noSlot, creation, work.node};
    arguments.range = work.range;
    arguments.outerRange = work.range;
    arguments.label = "')' after placement constructor arguments";
    continueNewArguments(std::move(arguments));
}

void Parser::PostfixIterationMachine::finishArrayGroup(Work& work)
{
    work.ordinal += work.children.size();
    if (work.children.size() > 1U)
    {
        if (work.node->first() != nullptr || !work.ranges.empty())
            parser_.diagnostics_.report(diagnostics::DiagnosticId::UnexpectedToken,
                diagnostics::DiagnosticSeverity::Error, work.children.front()->range(),
                "a rectangular allocation uses one counted bracket group");
        work.node->setAuxiliaryText("range-array-allocation:" +
            std::to_string(work.children.size()));
        work.node->setFirst(work.children.front());
        for (std::size_t index = 1; index < work.children.size(); ++index)
            work.node->addChild(work.children[index]);
    }
    else if (work.node->first() == nullptr) work.node->setFirst(work.children.front());
    else parser_.diagnostics_.report(diagnostics::DiagnosticId::UnexpectedToken,
        diagnostics::DiagnosticSeverity::Error, work.children.front()->range(),
        "dynamic jagged allocation accepts exactly one counted dimension");
    work.children.clear();
}

void Parser::PostfixIterationMachine::finishArrayNew(Work work)
{
    const bool inferred = work.ordinal == 0 && !work.ranges.empty() &&
        parser_.at(TokenKind::LeftBrace);
    const std::size_t modifiers = inferred ? work.ranges.size() - 1U : work.ranges.size();
    for (std::size_t index = 0; index < modifiers; ++index)
    {
        const SourceRange& range = work.ranges[index];
        work.typeSyntax->addModifier(ast::TypeModifierSyntax{
            ast::TypeModifierSyntaxKind::DynamicArray, nullptr, range});
        work.typeSyntax->setRange(parser_.join(work.typeSyntax->range(), range));
    }
    if (inferred)
    {
        parser_.consume();
        work.node->setAuxiliaryText("inferred-array-initializer");
        work.kind = WorkKind::ArrayInitializer;
        continueArrayInitializer(std::move(work));
        return;
    }
    if (work.ordinal == 0)
        parser_.diagnostics_.report(diagnostics::DiagnosticId::ExpectedExpression,
            diagnostics::DiagnosticSeverity::Error, work.node->range(),
            "dynamic array allocation requires one count or an aggregate initializer");
    work.node->setRange(SourceRange(
        work.range.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
    complete(work.resultSlot, work.node);
}

void Parser::PostfixIterationMachine::continueArrayNew(Work work)
{
    if (work.childSlot != noSlot)
    {
        work.children.push_back(results_[work.childSlot].value);
        work.childSlot = noSlot;
        if (parser_.consumeIf(TokenKind::Comma))
        {
            work.childSlot = makeSlot(); const std::size_t child = work.childSlot;
            work_.push_back(std::move(work)); scheduleChild(child); return;
        }
        parser_.expect(TokenKind::RightBracket, "']'");
        finishArrayGroup(work);
    }
    while (parser_.consumeIf(TokenKind::LeftBracket))
    {
        const SourceRange left = parser_.tokens_[parser_.index_ - 1U].range();
        if (parser_.at(TokenKind::RightBracket))
        {
            work.ranges.push_back(parser_.join(left, parser_.consume().range()));
            continue;
        }
        work.childSlot = makeSlot(); const std::size_t child = work.childSlot;
        work_.push_back(std::move(work)); scheduleChild(child); return;
    }
    finishArrayNew(std::move(work));
}

void Parser::PostfixIterationMachine::continueArrayInitializer(Work work)
{
    if (work.childSlot != noSlot)
    {
        if (ast::AstNode* value = results_[work.childSlot].value) work.node->addChild(value);
        work.childSlot = noSlot;
        if (!parser_.consumeIf(TokenKind::Comma) || parser_.at(TokenKind::RightBrace))
        {
            parser_.expect(TokenKind::RightBrace, "'}'");
            work.node->setRange(SourceRange(
                work.range.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
            complete(work.resultSlot, work.node);
            return;
        }
    }
    if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
    {
        parser_.expect(TokenKind::RightBrace, "'}'");
        work.node->setRange(SourceRange(
            work.range.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
        complete(work.resultSlot, work.node);
        return;
    }
    work.childSlot = makeSlot(); const std::size_t child = work.childSlot;
    work_.push_back(std::move(work)); scheduleChild(child);
}

void Parser::PostfixIterationMachine::startNewExpression(Work work)
{
    const SourceRange start = parser_.consume().range();
    if (parser_.at(TokenKind::LeftParen) && parser_.peek().kind() == TokenKind::RightParen)
    {
        parser_.consume(); parser_.consume();
        ast::AstNode* creation = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::ObjectCreationExpression, start);
        creation->setAuxiliaryText("target-typed");
        creation->setRange(parser_.join(start, parser_.tokens_[parser_.index_ - 1U].range()));
        complete(work.resultSlot, creation); return;
    }
    if (parser_.consumeIf(TokenKind::LeftParen))
    {
        if (parser_.parenthesizedNewIsPlacement())
        {
            Work placement{WorkKind::PlacementNew, work.resultSlot, makeSlot()};
            placement.range = start; const std::size_t child = placement.childSlot;
            work_.push_back(std::move(placement)); scheduleChild(child); return;
        }
        ast::AstNode* creation = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::ObjectCreationExpression, start);
        creation->setAuxiliaryText("target-typed");
        Work arguments{WorkKind::NewArguments, work.resultSlot, noSlot, creation};
        arguments.range = start;
        arguments.outerRange = start;
        arguments.label = "')' after target-typed new arguments";
        continueNewArguments(std::move(arguments)); return;
    }
    ast::TypeSyntax* type = parser_.parseAllocationElementType();
    if (parser_.consumeIf(TokenKind::LeftParen))
    {
        ast::AstNode* creation = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::ObjectCreationExpression, start);
        creation->setTypeSyntax(type);
        Work arguments{WorkKind::NewArguments, work.resultSlot, noSlot, creation};
        arguments.range = start;
        arguments.outerRange = start;
        arguments.label = "')'";
        continueNewArguments(std::move(arguments)); return;
    }
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(ast::AstNodeKind::NewExpression, start);
    node->setTypeSyntax(type);
    if (!parser_.at(TokenKind::LeftBracket))
    {
        node->setRange(SourceRange(start.begin(), parser_.tokens_[parser_.index_ - 1U].range().end()));
        complete(work.resultSlot, node); return;
    }
    node->setFlag(true);
    Work array{WorkKind::ArrayNew, work.resultSlot, noSlot, node};
    array.range = start; array.typeSyntax = type;
    continueArrayNew(std::move(array));
}
}
