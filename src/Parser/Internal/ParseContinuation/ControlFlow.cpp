#include "../ParseContinuationCoordinator.h"

#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/Language3Surface.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace nativesharp::parser
{
using lexer::TokenKind;
ast::AstNode* Parser::ParseContinuationCoordinator::conditionFromDeclaration(
    ast::AstNode* declaration)
{
    if (declaration == nullptr) return nullptr;
    const ast::AstNode* binding = declaration;
    if (declaration->kind() == ast::AstNodeKind::BlockStatement &&
        declaration->auxiliaryText() == "declaration-list")
    {
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidCondition,
            diagnostics::DiagnosticSeverity::Error,
            declaration->range(),
            "a condition declaration must declare exactly one local");
        if (declaration->children().empty()) return nullptr;
        binding = declaration->children().front();
    }
    return parser_.makeIdentifierExpression(binding->range(), binding->text());
}

// Opens an ordinary if header and schedules its condition through one stable child slot.
void Parser::ParseContinuationCoordinator::startIf(const Work& work)
{
    const std::size_t frameIndex = ifFrames_.size();
    ifFrames_.push_back({{}, work.resultSlot});
    IfFrame& frame = ifFrames_.back();
    frame.state.beginIndex = parser_.index_;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'('");
    Work finish{WorkKind::FinishIfCondition, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.flag = parser_.looksLikeConditionDeclaration();
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    if (finish.flag)
        startVariableDeclaration(Work{WorkKind::Statement, childSlot}, false, false);
    else
        work_.push_back({WorkKind::Expression, childSlot});
}

// Completes the if header and schedules exactly one embedded then body.
void Parser::ParseContinuationCoordinator::finishIfCondition(const Work& work)
{
    IfFrame& frame = ifFrames_[work.frameIndex];
    if (work.flag)
    {
        frame.state.conditionDeclaration = childValue(work);
        frame.state.condition = conditionFromDeclaration(frame.state.conditionDeclaration);
    }
    else frame.state.condition = childValue(work);
    parser_.expect(TokenKind::RightParen, "')'");
    Work finish{WorkKind::FinishIfThen, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Embedded, childSlot});
}

// Attaches the then body and preserves nearest-if binding for a following else clause.
void Parser::ParseContinuationCoordinator::finishIfThen(const Work& work)
{
    IfFrame& frame = ifFrames_[work.frameIndex];
    frame.state.thenBlock = childValue(work);
    if (!parser_.consumeIf(TokenKind::KwElse))
    {
        complete(frame.resultSlot, parser_.lowerIfStatement(frame.state, nullptr));
        return;
    }
    Work finish{WorkKind::FinishIfElse, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({parser_.at(TokenKind::KwIf)
        ? WorkKind::Statement : WorkKind::Embedded, childSlot});
}

// Publishes the completed ordinary if after its optional else subtree is ready.
void Parser::ParseContinuationCoordinator::finishIfElse(const Work& work)
{
    IfFrame& frame = ifFrames_[work.frameIndex];
    complete(frame.resultSlot, parser_.lowerIfStatement(frame.state, childValue(work)));
}

// Opens an ordinary while header and schedules its condition without native reentry.
void Parser::ParseContinuationCoordinator::startWhile(const Work& work)
{
    const std::size_t frameIndex = whileFrames_.size();
    whileFrames_.push_back({{}, work.resultSlot});
    WhileFrame& frame = whileFrames_.back();
    frame.state.beginIndex = parser_.index_;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'('");
    Work finish{WorkKind::FinishWhileCondition, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.flag = parser_.looksLikeConditionDeclaration();
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    if (finish.flag)
        startVariableDeclaration(Work{WorkKind::Statement, childSlot}, false, false);
    else
        work_.push_back({WorkKind::Expression, childSlot});
}

// Completes the while header and requires the same explicit block body as the legacy path.
void Parser::ParseContinuationCoordinator::finishWhileCondition(const Work& work)
{
    WhileFrame& frame = whileFrames_[work.frameIndex];
    if (work.flag)
    {
        frame.state.conditionDeclaration = childValue(work);
        frame.state.condition = conditionFromDeclaration(frame.state.conditionDeclaration);
    }
    else frame.state.condition = childValue(work);
    parser_.expect(TokenKind::RightParen, "')'");
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after while condition");
        complete(frame.resultSlot, parser_.lowerWhileStatement(frame.state, nullptr));
        return;
    }
    Work finish{WorkKind::FinishWhileBody, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches the completed body to the canonical while lowering.
void Parser::ParseContinuationCoordinator::finishWhileBody(const Work& work)
{
    WhileFrame& frame = whileFrames_[work.frameIndex];
    complete(frame.resultSlot, parser_.lowerWhileStatement(frame.state, childValue(work)));
}

// Opens a do-while and schedules its required block body on the shared worklist.
void Parser::ParseContinuationCoordinator::startDoWhile(const Work& work)
{
    const std::size_t frameIndex = doWhileFrames_.size();
    doWhileFrames_.push_back({{parser_.index_}, work.resultSlot, nullptr});
    parser_.consume();
    Work finish{WorkKind::FinishDoBody, work.resultSlot};
    finish.frameIndex = frameIndex;
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after 'do'");
        work_.push_back(finish);
        return;
    }
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Consumes the trailing do-while prefix and schedules its condition expression.
void Parser::ParseContinuationCoordinator::finishDoBody(const Work& work)
{
    DoWhileFrame& frame = doWhileFrames_[work.frameIndex];
    frame.body = childValue(work);
    parser_.expect(TokenKind::KwWhile, "'while' after the do-loop body");
    parser_.expect(TokenKind::LeftParen, "'('");
    Work finish{WorkKind::FinishDoCondition, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Builds the do-while node after its condition and contractual trailing semicolon.
void Parser::ParseContinuationCoordinator::finishDoCondition(const Work& work)
{
    DoWhileFrame& frame = doWhileFrames_[work.frameIndex];
    parser_.expect(TokenKind::RightParen, "')'");
    parser_.expect(TokenKind::Semicolon, "';' after the do-while condition");
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::DoWhileStatement,
        parser_.tokenRangeFrom(frame.state.beginIndex));
    node->setFirst(childValue(work));
    node->setSecond(frame.body);
    complete(frame.resultSlot, node);
}

// Opens a for header and schedules only the first present clause.
void Parser::ParseContinuationCoordinator::startFor(const Work& work)
{
    const std::size_t frameIndex = forFrames_.size();
    forFrames_.push_back({{}, work.resultSlot});
    ForFrame& frame = forFrames_.back();
    frame.state.beginIndex = parser_.index_;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'('");
    if (parser_.at(TokenKind::Semicolon))
    {
        parser_.consume();
        scheduleForCondition(frameIndex);
        return;
    }
    Work finish{WorkKind::FinishForInitializer, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    if (parser_.at(TokenKind::KwRaii) || parser_.at(TokenKind::KwConst) ||
        parser_.at(TokenKind::KwVolatile) || parser_.at(TokenKind::KwVar) ||
        parser_.canStartTypeAt(parser_.index_))
        startVariableDeclaration(Work{WorkKind::Statement, childSlot}, false, false);
    else
        startForExpressionList(childSlot, frame.state.beginIndex);
}

// Stores the completed initializer, consumes its delimiter, and advances to the condition.
void Parser::ParseContinuationCoordinator::finishForInitializer(const Work& work)
{
    ForFrame& frame = forFrames_[work.frameIndex];
    frame.state.initializer = childValue(work);
    parser_.expect(TokenKind::Semicolon, "';'");
    scheduleForCondition(work.frameIndex);
}

// Schedules the optional for condition or advances directly across its delimiter.
void Parser::ParseContinuationCoordinator::scheduleForCondition(std::size_t frameIndex)
{
    ForFrame& frame = forFrames_[frameIndex];
    if (parser_.at(TokenKind::Semicolon))
    {
        parser_.consume();
        scheduleForIncrement(frameIndex);
        return;
    }
    Work finish{WorkKind::FinishForCondition, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Stores the completed for condition, consumes ';', and advances to the increment clause.
void Parser::ParseContinuationCoordinator::finishForCondition(const Work& work)
{
    forFrames_[work.frameIndex].state.condition = childValue(work);
    parser_.expect(TokenKind::Semicolon, "';'");
    scheduleForIncrement(work.frameIndex);
}

// Schedules the optional increment expression list or advances directly to the body.
void Parser::ParseContinuationCoordinator::scheduleForIncrement(std::size_t frameIndex)
{
    ForFrame& frame = forFrames_[frameIndex];
    if (parser_.at(TokenKind::RightParen))
    {
        parser_.consume();
        scheduleForBody(frameIndex);
        return;
    }
    Work finish{WorkKind::FinishForIncrement, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    startForExpressionList(childSlot, frame.state.beginIndex);
}

// Stores the completed increment list and closes the parenthesized for header.
void Parser::ParseContinuationCoordinator::finishForIncrement(const Work& work)
{
    forFrames_[work.frameIndex].state.increment = childValue(work);
    parser_.expect(TokenKind::RightParen, "')'");
    scheduleForBody(work.frameIndex);
}

// Requires the canonical block body and schedules it without nested parser invocation.
void Parser::ParseContinuationCoordinator::scheduleForBody(std::size_t frameIndex)
{
    ForFrame& frame = forFrames_[frameIndex];
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after for clause");
        complete(frame.resultSlot, parser_.lowerForStatement(frame.state, nullptr));
        return;
    }
    Work finish{WorkKind::FinishForBody, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches the completed block to the canonical for lowering.
void Parser::ParseContinuationCoordinator::finishForBody(const Work& work)
{
    ForFrame& frame = forFrames_[work.frameIndex];
    complete(frame.resultSlot, parser_.lowerForStatement(frame.state, childValue(work)));
}

// Creates one explicit frame for a comma-separated for-clause expression list.
void Parser::ParseContinuationCoordinator::startForExpressionList(
    std::size_t resultSlot,
    std::size_t beginIndex)
{
    const std::size_t frameIndex = forExpressionFrames_.size();
    forExpressionFrames_.push_back({beginIndex, resultSlot, nullptr, nullptr});
    Work next{WorkKind::ContinueForExpressionList, resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(next);
}

// Schedules exactly one expression from the current for-clause list.
void Parser::ParseContinuationCoordinator::continueForExpressionList(const Work& work)
{
    ForExpressionFrame& frame = forExpressionFrames_[work.frameIndex];
    Work finish{WorkKind::FinishForExpression, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Appends one expression and either publishes the list or schedules its next element.
void Parser::ParseContinuationCoordinator::finishForExpression(const Work& work)
{
    ForExpressionFrame& frame = forExpressionFrames_[work.frameIndex];
    ast::AstNode* expression = childValue(work);
    if (frame.first == nullptr) frame.first = expression;
    if (frame.sequence != nullptr && expression != nullptr)
        frame.sequence->addChild(parser_.makeExpressionStatement(expression));
    if (!parser_.at(TokenKind::Comma))
    {
        ast::AstNode* result = frame.sequence != nullptr ? frame.sequence : frame.first;
        if (frame.sequence != nullptr)
            frame.sequence->setRange(parser_.tokenRangeFrom(frame.beginIndex));
        complete(frame.resultSlot, result);
        return;
    }
    if (frame.sequence == nullptr)
    {
        frame.sequence = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::BlockStatement,
            frame.first != nullptr ? frame.first->range() : parser_.current().range());
        if (frame.first != nullptr)
            frame.sequence->addChild(parser_.makeExpressionStatement(frame.first));
    }
    parser_.consume();
    Work next{WorkKind::ContinueForExpressionList, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Opens the foreach clause and schedules its real collection expression.
void Parser::ParseContinuationCoordinator::startForeach(const Work& work)
{
    const std::size_t frameIndex = foreachFrames_.size();
    foreachFrames_.push_back({{}, work.resultSlot, 0, 0});
    ForeachFrame& frame = foreachFrames_.back();
    if (!parser_.parseForeachHeaderPrefix(frame.state))
    {
        complete(frame.resultSlot, nullptr);
        return;
    }
    frame.collectionStart = parser_.index_;
    Work finish{WorkKind::FinishForeachCollection, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Stores the executable collection, rewinds, and schedules the distinct semantic probe.
void Parser::ParseContinuationCoordinator::finishForeachCollection(const Work& work)
{
    ForeachFrame& frame = foreachFrames_[work.frameIndex];
    frame.state.collection = childValue(work);
    frame.collectionEnd = parser_.index_;
    parser_.index_ = frame.collectionStart;
    Work finish{WorkKind::FinishForeachProbe, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Restores the real cursor boundary and schedules the required block body.
void Parser::ParseContinuationCoordinator::finishForeachProbe(const Work& work)
{
    ForeachFrame& frame = foreachFrames_[work.frameIndex];
    frame.state.collectionProbe = childValue(work);
    parser_.index_ = frame.collectionEnd;
    parser_.expect(TokenKind::RightParen, "')'");
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after foreach clause");
        complete(frame.resultSlot, parser_.lowerForeachStatement(std::move(frame.state), nullptr));
        return;
    }
    Work finish{WorkKind::FinishForeachBody, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Publishes the canonical lowering after the shared worklist completes the body.
void Parser::ParseContinuationCoordinator::finishForeachBody(const Work& work)
{
    ForeachFrame& frame = foreachFrames_[work.frameIndex];
    complete(frame.resultSlot, parser_.lowerForeachStatement(
        std::move(frame.state), childValue(work)));
}

// Opens one conditional-pattern frame and schedules its assignment-expression source.
void Parser::ParseContinuationCoordinator::startConditionalPattern(const Work& work)
{
    const std::size_t frameIndex = conditionalPatternFrames_.size();
    conditionalPatternFrames_.emplace_back();
    ConditionalPatternFrame& frame = conditionalPatternFrames_.back();
    frame.resultSlot = work.resultSlot;
    parseConditionalPatternPrefix(frame);
    frame.pattern = parser_.parsePattern(false);
    parser_.expect(TokenKind::Equal, "'=' between conditional pattern and source expression");
    analyzeConditionalPattern(frame);

    Work finish{WorkKind::FinishConditionalPatternSource, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Consumes the three historical prefixes and preserves their exact diagnostics.
void Parser::ParseContinuationCoordinator::parseConditionalPatternPrefix(
    ConditionalPatternFrame& frame)
{
    frame.beginIndex = parser_.index_;
    frame.featureRange = parser_.current().range();
    if (parser_.at(TokenKind::KwIf) || parser_.at(TokenKind::KwWhile))
    {
        frame.form = parser_.at(TokenKind::KwIf)
            ? ConditionalPatternForm::If : ConditionalPatternForm::While;
        frame.featureRange = parser_.consume().range();
        parser_.expect(TokenKind::LeftParen, frame.form == ConditionalPatternForm::If
            ? "'(' after 'if'" : "'(' after 'while'");
        parser_.expectContextualKeyword("let", "'let' in conditional pattern");
        return;
    }
    frame.form = ConditionalPatternForm::LetElse;
    parser_.expectContextualKeyword("let", "'let'");
}

// Runs the shared iterative pattern analysis exactly once and records ownership requirements.
void Parser::ParseContinuationCoordinator::analyzeConditionalPattern(
    ConditionalPatternFrame& frame)
{
    const Parser::ConditionalPatternAnalysis analysis =
        parser_.analyzeConditionalPattern(frame.pattern);
    frame.hasReferenceBinding = analysis.hasReferenceBinding;
    frame.hasMoveBinding = analysis.hasMoveBinding;
}

// Completes the header and schedules only the body required by the selected form.
void Parser::ParseContinuationCoordinator::finishConditionalPatternSource(const Work& work)
{
    ConditionalPatternFrame& frame = conditionalPatternFrames_[work.frameIndex];
    frame.sourceExpression = childValue(work);
    if (frame.form != ConditionalPatternForm::LetElse)
        parser_.expect(TokenKind::RightParen, "')' after conditional pattern");
    if (frame.form == ConditionalPatternForm::If)
    {
        scheduleConditionalPatternBody(
            work.frameIndex, WorkKind::FinishConditionalPatternSuccess, WorkKind::Embedded);
        return;
    }
    if (frame.form == ConditionalPatternForm::While)
    {
        scheduleConditionalPatternBody(
            work.frameIndex, WorkKind::FinishConditionalPatternWhile, WorkKind::Embedded);
        return;
    }
    parser_.expect(TokenKind::KwElse, "'else' after let pattern initializer");
    if (parser_.at(TokenKind::LeftBrace))
        scheduleConditionalPatternBody(
            work.frameIndex, WorkKind::FinishConditionalPatternLetElse, WorkKind::Block);
    else
    {
        parser_.expect(TokenKind::LeftBrace, "'{' to open the divergent let-else body");
        Work finish{WorkKind::FinishConditionalPatternLetElse, frame.resultSlot};
        finish.frameIndex = work.frameIndex;
        work_.push_back(finish);
    }
}

// Schedules one child body and its stable frame continuation.
void Parser::ParseContinuationCoordinator::scheduleConditionalPatternBody(
    std::size_t frameIndex,
    WorkKind finishKind,
    WorkKind bodyKind)
{
    ConditionalPatternFrame& frame = conditionalPatternFrames_[frameIndex];
    Work finish{finishKind, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({bodyKind, childSlot});
}

// Stores an if-let success body and preserves nearest-if failure scheduling.
void Parser::ParseContinuationCoordinator::finishConditionalPatternSuccess(const Work& work)
{
    ConditionalPatternFrame& frame = conditionalPatternFrames_[work.frameIndex];
    frame.successBody = childValue(work);
    if (!parser_.consumeIf(TokenKind::KwElse))
    {
        complete(frame.resultSlot, lowerConditionalPattern(frame));
        return;
    }
    const WorkKind failureKind = parser_.at(TokenKind::KwIf)
        ? WorkKind::Statement : WorkKind::Embedded;
    scheduleConditionalPatternBody(
        work.frameIndex, WorkKind::FinishConditionalPatternFailure, failureKind);
}

// Stores the optional if-let failure and publishes the canonical lowering.
void Parser::ParseContinuationCoordinator::finishConditionalPatternFailure(const Work& work)
{
    ConditionalPatternFrame& frame = conditionalPatternFrames_[work.frameIndex];
    frame.failureBody = childValue(work);
    complete(frame.resultSlot, lowerConditionalPattern(frame));
}

// Stores the while-let body and publishes its per-iteration lowering.
void Parser::ParseContinuationCoordinator::finishConditionalPatternWhile(const Work& work)
{
    ConditionalPatternFrame& frame = conditionalPatternFrames_[work.frameIndex];
    frame.successBody = childValue(work);
    complete(frame.resultSlot, lowerConditionalPattern(frame));
}

// Validates divergence, consumes the trailing semicolon, and publishes let-else.
void Parser::ParseContinuationCoordinator::finishConditionalPatternLetElse(const Work& work)
{
    ConditionalPatternFrame& frame = conditionalPatternFrames_[work.frameIndex];
    frame.failureBody = childValue(work);
    if (!parser_.statementAlwaysDiverges(frame.failureBody))
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            frame.failureBody != nullptr ? frame.failureBody->range() : frame.featureRange,
            "the else body of a let-else statement must diverge with return, throw, break, "
            "continue, or an if whose branches both diverge");
    parser_.expect(TokenKind::Semicolon, "';' after let-else statement");
    complete(frame.resultSlot, lowerConditionalPattern(frame));
}

// Produces a fresh identifier expression for each iterative pattern-lowering request.
Parser::PatternExpressionFactory
Parser::ParseContinuationCoordinator::makeConditionalPatternRoot(
    const ConditionalPatternLoweringContext& context)
{
    return [this, range = context.statementRange, temporary = context.temporary]
    {
        return parser_.makeIdentifierExpression(range, temporary);
    };
}

// Emits source and matched locals in their historical order and ownership mode.
void Parser::ParseContinuationCoordinator::addConditionalPatternPrelude(
    const ConditionalPatternFrame& frame,
    const ConditionalPatternLoweringContext& context,
    ast::AstNode* destination)
{
    ast::TypeSyntax* inferred = parser_.arena_.make<ast::TypeSyntax>(
        frame.featureRange, TokenKind::KwVar, "var");
    ast::AstNode* declaration = parser_.makeVariableDeclaration(
        inferred, context.temporary, frame.sourceExpression, false,
        !frame.hasReferenceBinding && !frame.hasMoveBinding);
    if (frame.hasReferenceBinding) declaration->setAuxiliaryText("local:ref");
    destination->addChild(declaration);
    ast::TypeSyntax* boolType = parser_.arena_.make<ast::TypeSyntax>(
        frame.featureRange, TokenKind::KwBool, "bool");
    destination->addChild(parser_.makeVariableDeclaration(
        boolType, context.matched,
        parser_.makeBooleanLiteral(frame.featureRange, false), false, false));
}

// Builds the matched=true block and optionally appends the parsed success body.
ast::AstNode* Parser::ParseContinuationCoordinator::makeConditionalPatternSuccess(
    const ConditionalPatternFrame& frame,
    const ConditionalPatternLoweringContext& context,
    bool includeBody)
{
    ast::AstNode* body = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, context.statementRange);
    body->addChild(parser_.makeExpressionStatement(parser_.makeAssignmentExpression(
        parser_.makeIdentifierExpression(context.statementRange, context.matched),
        parser_.makeBooleanLiteral(context.statementRange, true))));
    if (includeBody && frame.successBody != nullptr)
        for (ast::AstNode* child : frame.successBody->children()) body->addChild(child);
    return body;
}

// Recreates while-let as while(true) with one source evaluation per iteration.
ast::AstNode* Parser::ParseContinuationCoordinator::lowerConditionalPatternWhile(
    ConditionalPatternFrame& frame,
    const ConditionalPatternLoweringContext& context)
{
    PatternExpressionFactory root = makeConditionalPatternRoot(context);
    ast::AstNode* iteration = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, context.statementRange);
    addConditionalPatternPrelude(frame, context, iteration);
    iteration->addChild(parser_.lowerPattern(
        frame.pattern, root, makeConditionalPatternSuccess(frame, context, true),
        context.temporary));
    ast::AstNode* stopBody = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, context.statementRange);
    stopBody->addChild(parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BreakStatement, context.statementRange));
    ast::AstNode* stop = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, context.statementRange);
    stop->setFirst(makeConditionalPatternNotMatched(context.statementRange, context.matched));
    stop->setSecond(stopBody);
    iteration->addChild(stop);
    ast::AstNode* loop = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::WhileStatement, context.statementRange);
    loop->setFirst(parser_.makeBooleanLiteral(frame.featureRange, true));
    loop->setSecond(iteration);
    return loop;
}

// Recreates if-let and let-else selection without reanalyzing the pattern.
ast::AstNode* Parser::ParseContinuationCoordinator::lowerConditionalPatternSelection(
    ConditionalPatternFrame& frame,
    const ConditionalPatternLoweringContext& context)
{
    PatternExpressionFactory root = makeConditionalPatternRoot(context);
    ast::AstNode* lowered = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, context.statementRange);
    addConditionalPatternPrelude(frame, context, lowered);
    lowered->addChild(parser_.lowerPattern(
        frame.pattern, root,
        makeConditionalPatternSuccess(
            frame, context, frame.form == ConditionalPatternForm::If),
        context.temporary, frame.form != ConditionalPatternForm::LetElse));
    finishConditionalPatternLowering(frame, root, lowered, context);
    return lowered;
}

// Allocates one monotonic temporary pair and selects the canonical lowering family.
ast::AstNode* Parser::ParseContinuationCoordinator::lowerConditionalPattern(
    ConditionalPatternFrame& frame)
{
    ConditionalPatternLoweringContext context;
    context.statementRange = parser_.tokenRangeFrom(frame.beginIndex);
    context.temporary =
        "$__nsc_conditional_pattern_" + std::to_string(parser_.nextMatchTemporary_++);
    context.matched = context.temporary + "_matched";
    if (frame.form == ConditionalPatternForm::While)
        return lowerConditionalPatternWhile(frame, context);
    return lowerConditionalPatternSelection(frame, context);
}

// Creates the canonical !matched condition used by failure and stop branches.
ast::AstNode* Parser::ParseContinuationCoordinator::makeConditionalPatternNotMatched(
    SourceRange range,
    const std::string& matched)
{
    ast::AstNode* result = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::UnaryExpression, range);
    result->setTokenKind(TokenKind::Bang);
    result->setFirst(parser_.makeIdentifierExpression(range, matched));
    return result;
}

// Appends failure and post-success let-else bindings in their observable order.
void Parser::ParseContinuationCoordinator::finishConditionalPatternLowering(
    ConditionalPatternFrame& frame,
    const PatternExpressionFactory& root,
    ast::AstNode* lowered,
    const ConditionalPatternLoweringContext& context)
{
    if (frame.failureBody != nullptr)
    {
        ast::AstNode* failure = parser_.arena_.make<ast::AstNode>(
            ast::AstNodeKind::IfStatement, context.statementRange);
        failure->setFirst(makeConditionalPatternNotMatched(
            context.statementRange, context.matched));
        failure->setSecond(frame.failureBody);
        lowered->addChild(failure);
    }
    if (frame.form != ConditionalPatternForm::LetElse) return;
    parser_.emitPatternBindings(frame.pattern, root, lowered, context.temporary);
    lowered->setAuxiliaryText("conditional-pattern-inline");
}

// Opens one switch frame and schedules its governing expression or declaration.
void Parser::ParseContinuationCoordinator::startSwitch(const Work& work)
{
    const std::size_t frameIndex = switchFrames_.size();
    switchFrames_.emplace_back();
    SwitchFrame& frame = switchFrames_.back();
    frame.beginIndex = parser_.index_;
    frame.resultSlot = work.resultSlot;
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'(' after 'switch'");
    Work finish{WorkKind::FinishSwitchGoverning, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.flag = parser_.looksLikeConditionDeclaration();
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    if (finish.flag)
        startVariableDeclaration(Work{WorkKind::Statement, childSlot}, false, false);
    else
        work_.push_back({WorkKind::Expression, childSlot});
}

// Completes the switch header and creates the canonical governing node shape.
void Parser::ParseContinuationCoordinator::finishSwitchGoverning(const Work& work)
{
    SwitchFrame& frame = switchFrames_[work.frameIndex];
    ast::AstNode* governing = childValue(work);
    if (work.flag)
    {
        frame.conditionDeclaration = governing;
        governing = conditionFromDeclaration(frame.conditionDeclaration);
    }
    parser_.expect(TokenKind::RightParen, "')' after the switch governing expression");
    parser_.expect(TokenKind::LeftBrace, "'{' to open the switch body");
    frame.node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::SwitchStatement,
        parser_.tokenRangeFrom(frame.beginIndex));
    frame.node->setFirst(governing);
    Work next{WorkKind::ContinueSwitch, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Selects the next section, performs one-token recovery, or closes the switch.
void Parser::ParseContinuationCoordinator::continueSwitch(const Work& work)
{
    SwitchFrame& frame = switchFrames_[work.frameIndex];
    if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
    {
        completeSwitch(frame);
        return;
    }
    if (!parser_.atSwitchLabel())
    {
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "expected 'case' or 'default' at the start of a switch section");
        parser_.consume();
        work_.push_back(work);
        return;
    }
    startSwitchSection(work.frameIndex);
}

// Creates one section and schedules its first label without a nested parser facade.
void Parser::ParseContinuationCoordinator::startSwitchSection(std::size_t frameIndex)
{
    SwitchFrame& frame = switchFrames_[frameIndex];
    frame.sectionBegin = parser_.index_;
    frame.section = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::SwitchSection,
        parser_.current().range());
    frame.labelCount = 0;
    Work next{WorkKind::ContinueSwitchLabels, frame.resultSlot};
    next.frameIndex = frameIndex;
    work_.push_back(next);
}

// Schedules one case value at a time and preserves grouped-label ordering.
void Parser::ParseContinuationCoordinator::continueSwitchLabels(const Work& work)
{
    SwitchFrame& frame = switchFrames_[work.frameIndex];
    if (!parser_.atSwitchLabel())
    {
        frame.section->setIntegerValue(frame.labelCount);
        Work next{WorkKind::ContinueSwitch, frame.resultSlot};
        next.frameIndex = work.frameIndex;
        Work section{WorkKind::ContinueSwitchSection, frame.resultSlot};
        section.frameIndex = work.frameIndex;
        work_.push_back(next);
        work_.push_back(section);
        return;
    }
    Work finish{WorkKind::FinishSwitchLabel, frame.resultSlot};
    finish.frameIndex = work.frameIndex;
    finish.beginIndex = parser_.index_;
    finish.flag = parser_.at(TokenKind::KwDefault);
    parser_.consume();
    // EN: Freeze 3.0 C-06: same deliberate rejection as StructuredControlParser::appendSwitchLabel.
    // ES: Congelamiento 3.0 C-06: el mismo rechazo deliberado que appendSwitchLabel.
    if (!finish.flag && parser_.at(TokenKind::LeftBracket))
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::UnexpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            std::string(language::kListPatternCaseLabelMessage));
    if (!finish.flag)
    {
        finish.childSlot = makeResultSlot();
        const std::size_t childSlot = finish.childSlot;
        work_.push_back(finish);
        work_.push_back({WorkKind::Expression, childSlot});
        return;
    }
    work_.push_back(finish);
}

// Materializes one label after its optional value and resumes the label run.
void Parser::ParseContinuationCoordinator::finishSwitchLabel(const Work& work)
{
    SwitchFrame& frame = switchFrames_[work.frameIndex];
    parser_.expect(
        TokenKind::Colon,
        work.flag ? "':' after 'default'" : "':' after the case value");
    ast::AstNode* label = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::SwitchLabel,
        parser_.tokenRangeFrom(work.beginIndex));
    label->setFlag(work.flag);
    label->setFirst(childValue(work));
    frame.section->addChild(label);
    ++frame.labelCount;
    Work next{WorkKind::ContinueSwitchLabels, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Schedules one section statement or publishes the completed labels-first section.
void Parser::ParseContinuationCoordinator::continueSwitchSection(const Work& work)
{
    SwitchFrame& frame = switchFrames_[work.frameIndex];
    if (parser_.atSwitchLabel() || parser_.at(TokenKind::RightBrace) ||
        parser_.at(TokenKind::EndOfFile))
    {
        frame.section->setRange(parser_.tokenRangeFrom(frame.sectionBegin));
        frame.node->addChild(frame.section);
        return;
    }
    frame.beforeStatement = parser_.index_;
    Work append{WorkKind::AppendSwitchStatement, frame.resultSlot};
    append.frameIndex = work.frameIndex;
    append.childSlot = makeResultSlot();
    const std::size_t childSlot = append.childSlot;
    work_.push_back(append);
    work_.push_back({WorkKind::Statement, childSlot});
}

// Preserves the historical conditional-pattern flattening and progress guard.
void Parser::ParseContinuationCoordinator::appendSwitchStatement(const Work& work)
{
    SwitchFrame& frame = switchFrames_[work.frameIndex];
    ast::AstNode* statement = childValue(work);
    if (statement != nullptr && statement->kind() == ast::AstNodeKind::BlockStatement &&
        statement->auxiliaryText() == "conditional-pattern-inline")
        for (ast::AstNode* child : statement->children()) frame.section->addChild(child);
    else if (statement != nullptr)
        frame.section->addChild(statement);
    if (parser_.index_ == frame.beforeStatement) parser_.consume();
    Work next{WorkKind::ContinueSwitchSection, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Consumes the closing delimiter and publishes either the switch or its declaration wrapper.
void Parser::ParseContinuationCoordinator::completeSwitch(SwitchFrame& frame)
{
    parser_.expect(TokenKind::RightBrace, "'}' to close the switch");
    frame.node->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    if (frame.conditionDeclaration == nullptr)
    {
        complete(frame.resultSlot, frame.node);
        return;
    }
    ast::AstNode* wrapper = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement,
        frame.node->range());
    wrapper->setAuxiliaryText("condition-declaration");
    wrapper->addChild(frame.conditionDeclaration);
    wrapper->addChild(frame.node);
    complete(frame.resultSlot, wrapper);
}

// Opens one match frame and schedules its governing expression on the shared worklist.
void Parser::ParseContinuationCoordinator::startMatch(const Work& work)
{
    const std::size_t frameIndex = matchFrames_.size();
    matchFrames_.emplace_back();
    MatchFrame& frame = matchFrames_.back();
    frame.beginIndex = parser_.index_;
    frame.resultSlot = work.resultSlot;
    frame.keywordRange = parser_.consume().range();
    parser_.expect(TokenKind::LeftParen, "'(' after 'match'");
    Work finish{WorkKind::FinishMatchGoverning, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Completes the match header and advances to its first arm.
void Parser::ParseContinuationCoordinator::finishMatchGoverning(const Work& work)
{
    MatchFrame& frame = matchFrames_[work.frameIndex];
    frame.governing = childValue(work);
    parser_.expect(TokenKind::RightParen, "')' after match governing expression");
    parser_.expect(TokenKind::LeftBrace, "'{' to open match arms");
    Work next{WorkKind::ContinueMatch, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Parses one arm prefix or closes and lowers the completed statement match.
void Parser::ParseContinuationCoordinator::continueMatch(const Work& work)
{
    MatchFrame& frame = matchFrames_[work.frameIndex];
    if (parser_.at(TokenKind::RightBrace) || parser_.at(TokenKind::EndOfFile))
    {
        parser_.expect(TokenKind::RightBrace, "'}' to close match arms");
        complete(frame.resultSlot, lowerMatchStatement(frame));
        return;
    }
    parseMatchArmHeader(frame);
    if (parser_.atContextualKeyword("when"))
    {
        parser_.consume();
        Work finish{WorkKind::FinishMatchGuard, frame.resultSlot};
        finish.frameIndex = work.frameIndex;
        finish.childSlot = makeResultSlot();
        const std::size_t childSlot = finish.childSlot;
        work_.push_back(finish);
        work_.push_back({WorkKind::ConditionalExpression, childSlot});
        return;
    }
    validateMatchArmReachability(frame);
    parser_.expect(TokenKind::FatArrow, "'=>' after match pattern");
    scheduleMatchBody(work.frameIndex);
}

// Parses and analyzes one pattern exactly once before any optional guard.
void Parser::ParseContinuationCoordinator::parseMatchArmHeader(MatchFrame& frame)
{
    frame.armBeginIndex = parser_.index_;
    frame.pendingArm = MatchArm{};
    frame.pendingArm.pattern = parser_.parsePattern(false);
    frame.pendingArm.range = frame.pendingArm.pattern.range;
    frame.pendingArm.analysis =
        parser_.analyzeMatchExpressionPattern(frame.pendingArm.pattern, false);
    reportMatchPatternDiagnostics(frame.pendingArm.analysis);
    accumulateMatchOwnership(frame, frame.pendingArm.analysis);
}

// Stores the conditional-expression guard before reachability and arrow validation.
void Parser::ParseContinuationCoordinator::finishMatchGuard(const Work& work)
{
    MatchFrame& frame = matchFrames_[work.frameIndex];
    frame.pendingArm.guard = childValue(work);
    validateMatchArmReachability(frame);
    parser_.expect(TokenKind::FatArrow, "'=>' after match pattern");
    scheduleMatchBody(work.frameIndex);
}

// Schedules the required block body or performs the historical missing-body recovery.
void Parser::ParseContinuationCoordinator::scheduleMatchBody(std::size_t frameIndex)
{
    MatchFrame& frame = matchFrames_[frameIndex];
    Work finish{WorkKind::FinishMatchArm, frame.resultSlot};
    finish.frameIndex = frameIndex;
    if (parser_.at(TokenKind::LeftBrace))
    {
        finish.childSlot = makeResultSlot();
        const std::size_t childSlot = finish.childSlot;
        work_.push_back(finish);
        work_.push_back({WorkKind::Block, childSlot});
        return;
    }
    reportMissingMatchArmBody();
    finish.node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, parser_.current().range());
    parser_.synchronizeStatement();
    work_.push_back(finish);
}

// Attaches one completed body, closes its source range, and resumes arm parsing.
void Parser::ParseContinuationCoordinator::finishMatchArm(const Work& work)
{
    MatchFrame& frame = matchFrames_[work.frameIndex];
    frame.pendingArm.body = work.childSlot == noSlot ? work.node : childValue(work);
    frame.pendingArm.range = parser_.tokenRangeFrom(frame.armBeginIndex);
    frame.arms.push_back(std::move(frame.pendingArm));
    parser_.consumeIf(TokenKind::Comma);
    Work next{WorkKind::ContinueMatch, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Preserves the historical missing-block diagnostic before synchronization.
void Parser::ParseContinuationCoordinator::reportMissingMatchArmBody()
{
    parser_.diagnostics_.report(
        diagnostics::DiagnosticId::ExpectedToken,
        diagnostics::DiagnosticSeverity::Error,
        parser_.current().range(),
        "a match arm requires a block body");
}

// Returns the stable spelling embedded in binding-mode diagnostics.
std::string_view Parser::ParseContinuationCoordinator::matchBindingModeName(
    PatternBindingMode mode) noexcept
{
    switch (mode)
    {
    case PatternBindingMode::Value: return "value";
    case PatternBindingMode::Reference: return "ref";
    case PatternBindingMode::Move: return "move";
    case PatternBindingMode::Copy: return "copy";
    case PatternBindingMode::ReferenceMutable: return "ref mut";
    }
    return "value";
}

// Publishes the two historical diagnostic families in their original order.
void Parser::ParseContinuationCoordinator::reportMatchPatternDiagnostics(
    const Parser::MatchPatternAnalysis& analysis)
{
    for (const auto& [range, mode] : analysis.nonAddressableBindings)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            range,
            std::string(matchBindingModeName(mode)) +
                " pattern binding cannot cross a tuple deconstruction or variant payload "
                "copy; bind the aggregate by ref/move first");
    for (const SourceRange& range : analysis.invalidOrRanges)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            range,
            "OR-pattern alternatives must bind identical names with identical "
            "value/ref/move modes");
}

// Accumulates governing ownership requirements without rescanning patterns.
void Parser::ParseContinuationCoordinator::accumulateMatchOwnership(
    MatchFrame& frame,
    const Parser::MatchPatternAnalysis& analysis)
{
    for (const auto& [name, mode] : analysis.bindings)
    {
        static_cast<void>(name);
        frame.hasReferenceBinding = frame.hasReferenceBinding ||
            mode == PatternBindingMode::Reference ||
            mode == PatternBindingMode::ReferenceMutable;
        frame.hasMoveBinding = frame.hasMoveBinding || mode == PatternBindingMode::Move;
    }
}

// Updates cross-arm reachability after the optional guard is available.
void Parser::ParseContinuationCoordinator::validateMatchArmReachability(MatchFrame& frame)
{
    MatchArm& arm = frame.pendingArm;
    const bool guardAlwaysTrue = arm.guard == nullptr ||
        (arm.guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
         arm.guard->tokenKind() == TokenKind::KwTrue);
    const bool guardAlwaysFalse = arm.guard != nullptr &&
        arm.guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
        arm.guard->tokenKind() == TokenKind::KwFalse;
    if (guardAlwaysFalse) reportConstantFalseMatchGuard(arm);
    if (frame.sawCatchAll) reportMatchArmAfterCatchAll(arm);
    if (arm.analysis.wildcard && guardAlwaysTrue) frame.sawCatchAll = true;
    validateMatchIntegerIntervals(frame, guardAlwaysTrue);
}

// Emits the established constant-false guard diagnostic at the guard range.
void Parser::ParseContinuationCoordinator::reportConstantFalseMatchGuard(const MatchArm& arm)
{
    parser_.diagnostics_.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        arm.guard->range(),
        "match arm is unreachable because its guard is the constant false");
}

// Emits the established diagnostic after an unconditional catch-all.
void Parser::ParseContinuationCoordinator::reportMatchArmAfterCatchAll(const MatchArm& arm)
{
    parser_.diagnostics_.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        arm.range,
        "match arm is unreachable after a catch-all wildcard or binding arm");
}

// Collects one literal/range OR domain iteratively; false marks a mixed domain.
bool Parser::ParseContinuationCoordinator::collectMatchIntegerIntervals(
    const Pattern& root,
    std::vector<std::pair<std::int64_t, std::int64_t>>& intervals)
{
    bool complete = true;
    std::vector<const Pattern*> pending{&root};
    while (!pending.empty())
    {
        const Pattern* pattern = pending.back();
        pending.pop_back();
        if (appendMatchIntegerInterval(*pattern, intervals)) continue;
        if (pattern->kind != PatternKind::Or) { complete = false; continue; }
        for (auto child = pattern->children.rbegin(); child != pattern->children.rend(); ++child)
            pending.push_back(&*child);
    }
    return complete;
}

// Appends one bounded integral pattern as a half-open interval.
bool Parser::ParseContinuationCoordinator::appendMatchIntegerInterval(
    const Pattern& pattern,
    std::vector<std::pair<std::int64_t, std::int64_t>>& intervals)
{
    if (pattern.kind == PatternKind::Literal && pattern.firstInteger.has_value() &&
        *pattern.firstInteger != std::numeric_limits<std::int64_t>::max())
    {
        intervals.emplace_back(*pattern.firstInteger, *pattern.firstInteger + 1);
        return true;
    }
    if (pattern.kind == PatternKind::Range && pattern.firstInteger.has_value() &&
        pattern.secondInteger.has_value() && *pattern.firstInteger < *pattern.secondInteger)
    {
        intervals.emplace_back(*pattern.firstInteger, *pattern.secondInteger);
        return true;
    }
    return false;
}

// Diagnoses overlap and indexes only unrestricted arms.
void Parser::ParseContinuationCoordinator::validateMatchIntegerIntervals(
    MatchFrame& frame,
    bool guardAlwaysTrue)
{
    std::vector<std::pair<std::int64_t, std::int64_t>> current;
    if (!collectMatchIntegerIntervals(frame.pendingArm.pattern, current)) return;
    const bool overlapsWithinArm = normalizeMatchIntegerIntervals(current);
    const bool overlapsEarlierArm = matchIntervalsOverlap(current, frame.coveredIntegerRanges);
    if (overlapsWithinArm || overlapsEarlierArm)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            frame.pendingArm.range,
            "integer literal/range pattern overlaps an earlier alternative");
    if (guardAlwaysTrue)
        for (const auto& interval : current)
            insertMatchIntegerInterval(frame.coveredIntegerRanges, interval);
}

// Sorts and coalesces one arm while recording strict overlap.
bool Parser::ParseContinuationCoordinator::normalizeMatchIntegerIntervals(
    std::vector<std::pair<std::int64_t, std::int64_t>>& intervals)
{
    std::ranges::sort(intervals);
    std::size_t output = 0;
    bool overlaps = false;
    for (const auto& interval : intervals)
    {
        if (output == 0 || intervals[output - 1].second < interval.first)
        {
            intervals[output++] = interval;
            continue;
        }
        overlaps = overlaps || interval.first < intervals[output - 1].second;
        intervals[output - 1].second =
            std::max(intervals[output - 1].second, interval.second);
    }
    intervals.resize(output);
    return overlaps;
}

// Queries the ordered disjoint interval index in logarithmic time.
bool Parser::ParseContinuationCoordinator::matchIntervalsOverlap(
    const std::vector<std::pair<std::int64_t, std::int64_t>>& current,
    const MatchIntegerIntervalIndex& covered)
{
    for (const auto& interval : current)
    {
        const auto next = covered.lower_bound(interval.first);
        if (next != covered.end() && next->first < interval.second) return true;
        if (next == covered.begin()) continue;
        if (interval.first < std::prev(next)->second) return true;
    }
    return false;
}

// Inserts one interval and coalesces overlap or adjacency.
void Parser::ParseContinuationCoordinator::insertMatchIntegerInterval(
    MatchIntegerIntervalIndex& covered,
    std::pair<std::int64_t, std::int64_t> interval)
{
    auto next = covered.lower_bound(interval.first);
    if (next != covered.begin())
    {
        const auto previous = std::prev(next);
        if (previous->second >= interval.first)
        {
            interval.first = previous->first;
            interval.second = std::max(interval.second, previous->second);
            covered.erase(previous);
        }
    }
    next = covered.lower_bound(interval.first);
    while (next != covered.end() && next->first <= interval.second)
    {
        interval.second = std::max(interval.second, next->second);
        next = covered.erase(next);
    }
    covered.emplace(interval);
}

// Materializes the historical first-match lowering after every arm is complete.
ast::AstNode* Parser::ParseContinuationCoordinator::lowerMatchStatement(MatchFrame& frame)
{
    if (frame.arms.empty()) reportEmptyMatch(frame);
    const SourceRange range = parser_.tokenRangeFrom(frame.beginIndex);
    ast::AstNode* lowered = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, range);
    const std::string temporary =
        "$__nsc_match_" + std::to_string(parser_.nextMatchTemporary_++);
    lowered->setText(temporary);
    lowered->setAuxiliaryText(buildMatchCoverage(frame.arms));
    appendMatchGoverningState(*lowered, frame, temporary);
    for (const MatchArm& arm : frame.arms)
        lowered->addChild(lowerMatchArm(arm, range, temporary));
    return lowered;
}

// Reports an empty statement match while retaining its lowering shell.
void Parser::ParseContinuationCoordinator::reportEmptyMatch(const MatchFrame& frame)
{
    parser_.diagnostics_.report(
        diagnostics::DiagnosticId::InvalidOperand,
        diagnostics::DiagnosticSeverity::Error,
        frame.keywordRange,
        "a match statement requires at least one arm");
}

// Builds the source-ordered semantic coverage bridge consumed downstream.
std::string Parser::ParseContinuationCoordinator::buildMatchCoverage(
    const std::vector<MatchArm>& arms)
{
    std::string coverage = "match-coverage:v1:";
    for (std::size_t index = 0; index < arms.size(); ++index)
    {
        if (index != 0) coverage += ';';
        appendMatchArmCoverage(coverage, arms[index]);
    }
    return coverage;
}

// Encodes one arm as wildcard, list-length, nominal-case, or unknown coverage.
void Parser::ParseContinuationCoordinator::appendMatchArmCoverage(
    std::string& coverage,
    const MatchArm& arm)
{
    std::vector<std::string> cases = arm.analysis.cases;
    std::vector<std::pair<std::size_t, bool>> lengths = arm.analysis.listLengths;
    const bool restricted = arm.guard != nullptr &&
        !(arm.guard->kind() == ast::AstNodeKind::BooleanLiteralExpression &&
          arm.guard->tokenKind() == TokenKind::KwTrue);
    if (arm.analysis.wildcard)
    {
        if (restricted) coverage += '~';
        coverage += '*';
        return;
    }
    if (!lengths.empty())
    {
        appendMatchListCoverage(coverage, lengths, arm.analysis.partial || restricted);
        return;
    }
    appendMatchCaseCoverage(coverage, cases, arm.analysis.partial || restricted);
}

// Sorts and deduplicates list domains before emitting their stable marker format.
void Parser::ParseContinuationCoordinator::appendMatchListCoverage(
    std::string& coverage,
    std::vector<std::pair<std::size_t, bool>>& lengths,
    bool partial)
{
    std::ranges::sort(lengths);
    lengths.erase(std::unique(lengths.begin(), lengths.end()), lengths.end());
    if (partial) coverage += '~';
    for (std::size_t index = 0; index < lengths.size(); ++index)
    {
        if (index != 0) coverage += '|';
        coverage += '#' + std::to_string(lengths[index].first);
        if (lengths[index].second) coverage += '+';
    }
}

// Sorts and deduplicates nominal cases before emitting their stable marker format.
void Parser::ParseContinuationCoordinator::appendMatchCaseCoverage(
    std::string& coverage,
    std::vector<std::string>& cases,
    bool partial)
{
    std::ranges::sort(cases);
    cases.erase(std::unique(cases.begin(), cases.end()), cases.end());
    if (cases.empty()) { coverage += '?'; return; }
    if (partial) coverage += '~';
    for (std::size_t index = 0; index < cases.size(); ++index)
    {
        if (index != 0) coverage += '|';
        coverage += cases[index];
    }
}

// Creates governing-value and selected-flag locals with preserved ownership mode.
void Parser::ParseContinuationCoordinator::appendMatchGoverningState(
    ast::AstNode& lowered,
    const MatchFrame& frame,
    const std::string& temporary)
{
    ast::TypeSyntax* inferred = parser_.arena_.make<ast::TypeSyntax>(
        frame.keywordRange, TokenKind::KwVar, "var");
    ast::AstNode* governing = parser_.makeVariableDeclaration(
        inferred,
        temporary,
        frame.governing,
        false,
        !frame.hasReferenceBinding && !frame.hasMoveBinding);
    if (frame.hasReferenceBinding) governing->setAuxiliaryText("local:ref");
    lowered.addChild(governing);
    lowered.addChild(makeMatchSelectedDeclaration(frame.keywordRange, temporary));
}

// Creates the false-initialized selected flag used by the lowered decision chain.
ast::AstNode* Parser::ParseContinuationCoordinator::makeMatchSelectedDeclaration(
    SourceRange range,
    const std::string& temporary)
{
    ast::TypeSyntax* boolType = parser_.arena_.make<ast::TypeSyntax>(
        range, TokenKind::KwBool, "bool");
    return parser_.makeVariableDeclaration(
        boolType,
        temporary + "_selected",
        parser_.makeBooleanLiteral(range, false),
        false,
        false);
}

// Lowers one arm to the historical not-selected guard and pattern matcher.
ast::AstNode* Parser::ParseContinuationCoordinator::lowerMatchArm(
    const MatchArm& arm,
    SourceRange matchRange,
    const std::string& temporary)
{
    ast::AstNode* selectedBody = makeMatchSelectedBody(arm, temporary);
    PatternExpressionFactory root = [this, &temporary, range = arm.range]
    {
        return parser_.makeIdentifierExpression(range, temporary);
    };
    ast::AstNode* continuation = wrapMatchGuard(arm, selectedBody);
    ast::AstNode* matcher = parser_.lowerPattern(
        arm.pattern, root, continuation, temporary);
    ast::AstNode* guardedBody = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, arm.range);
    guardedBody->addChild(matcher);
    ast::AstNode* guard = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, arm.range);
    guard->setFirst(makeMatchNotSelected(matchRange, temporary));
    guard->setSecond(guardedBody);
    return guard;
}

// Prepends selected=true to original body statements without copying their AST nodes.
ast::AstNode* Parser::ParseContinuationCoordinator::makeMatchSelectedBody(
    const MatchArm& arm,
    const std::string& temporary)
{
    ast::AstNode* selected = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, arm.range);
    selected->addChild(parser_.makeExpressionStatement(parser_.makeAssignmentExpression(
        parser_.makeIdentifierExpression(arm.range, temporary + "_selected"),
        parser_.makeBooleanLiteral(arm.range, true))));
    for (ast::AstNode* statement : arm.body->children()) selected->addChild(statement);
    return selected;
}

// Evaluates a source guard only after pattern bindings are materialized.
ast::AstNode* Parser::ParseContinuationCoordinator::wrapMatchGuard(
    const MatchArm& arm,
    ast::AstNode* selectedBody)
{
    if (arm.guard == nullptr) return selectedBody;
    ast::AstNode* guarded = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::IfStatement, arm.guard->range());
    guarded->setFirst(arm.guard);
    guarded->setSecond(selectedBody);
    ast::AstNode* block = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::BlockStatement, arm.guard->range());
    block->addChild(guarded);
    return block;
}

// Builds the outer !selected condition that enforces first-match semantics.
ast::AstNode* Parser::ParseContinuationCoordinator::makeMatchNotSelected(
    SourceRange range,
    const std::string& temporary)
{
    ast::AstNode* result = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::UnaryExpression, range);
    result->setTokenKind(TokenKind::Bang);
    result->setFirst(parser_.makeIdentifierExpression(range, temporary + "_selected"));
    return result;
}

// Parses the non-recursive catch declaration prefix and leaves any filter/body at the cursor.
}
