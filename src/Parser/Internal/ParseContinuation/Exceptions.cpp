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
ast::AstNode* Parser::ParseContinuationCoordinator::parseCatchHeader(std::size_t beginIndex)
{
    parser_.consume();
    ast::AstNode* clause = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::CatchClause,
        parser_.tokenRangeFrom(beginIndex));
    if (!parser_.consumeIf(TokenKind::LeftParen))
    {
        clause->setFlag(true);
        return clause;
    }
    if (parser_.canStartTypeAt(parser_.index_))
        clause->setTypeSyntax(parser_.parseType());
    else
        parser_.expect(TokenKind::Identifier, "exception type");
    if (parser_.at(TokenKind::Identifier))
        clause->setText(std::string(parser_.consume().lexeme()));
    parser_.expect(TokenKind::RightParen, "')' after catch declaration");
    return clause;
}

// Opens one try frame and schedules the protected block on the shared worklist.
void Parser::ParseContinuationCoordinator::startTry(const Work& work)
{
    const std::size_t frameIndex = tryFrames_.size();
    const std::size_t beginIndex = parser_.index_;
    parser_.consume();
    ast::AstNode* node = parser_.arena_.make<ast::AstNode>(
        ast::AstNodeKind::TryStatement,
        parser_.tokenRangeFrom(beginIndex));
    tryFrames_.push_back({beginIndex, work.resultSlot, 0, node, nullptr, false});
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after try");
        work_.push_back({WorkKind::ContinueTry, work.resultSlot});
        work_.back().frameIndex = frameIndex;
        return;
    }
    Work finish{WorkKind::FinishTryBlock, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Selects the next catch/finally clause or closes the completed try statement.
void Parser::ParseContinuationCoordinator::continueTry(const Work& work)
{
    TryFrame& frame = tryFrames_[work.frameIndex];
    if (parser_.at(TokenKind::KwCatch))
    {
        frame.pendingCatchBeginIndex = parser_.index_;
        frame.pendingCatch = parseCatchHeader(frame.pendingCatchBeginIndex);
        if (!parser_.atContextualKeyword("when"))
        {
            scheduleTryCatchBody(work.frameIndex);
            return;
        }
        parser_.consume();
        parser_.expect(TokenKind::LeftParen, "'(' after catch filter 'when'");
        Work finish{WorkKind::FinishTryCatchFilter, frame.resultSlot};
        finish.frameIndex = work.frameIndex;
        finish.childSlot = makeResultSlot();
        const std::size_t childSlot = finish.childSlot;
        work_.push_back(finish);
        work_.push_back({WorkKind::Expression, childSlot});
        return;
    }
    if (parser_.consumeIf(TokenKind::KwFinally))
    {
        frame.hasFinally = true;
        scheduleTryFinally(work.frameIndex);
        return;
    }
    completeTry(frame);
}

// Stores the protected block and advances to the ordered catch/finally sequence.
void Parser::ParseContinuationCoordinator::finishTryBlock(const Work& work)
{
    tryFrames_[work.frameIndex].node->setFirst(childValue(work));
    Work next{WorkKind::ContinueTry, work.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Attaches the filter after the expression continuation consumes its exact closing boundary.
void Parser::ParseContinuationCoordinator::finishTryCatchFilter(const Work& work)
{
    TryFrame& frame = tryFrames_[work.frameIndex];
    frame.pendingCatch->setSecond(childValue(work));
    parser_.expect(TokenKind::RightParen, "')' after catch filter expression");
    scheduleTryCatchBody(work.frameIndex);
}

// Schedules one catch body or preserves the canonical missing-block recovery.
void Parser::ParseContinuationCoordinator::scheduleTryCatchBody(std::size_t frameIndex)
{
    TryFrame& frame = tryFrames_[frameIndex];
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after catch");
        attachPendingCatch(frame, nullptr);
        Work next{WorkKind::ContinueTry, frame.resultSlot};
        next.frameIndex = frameIndex;
        work_.push_back(next);
        return;
    }
    Work finish{WorkKind::FinishTryCatchBlock, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches one completed catch body and resumes clause discovery in source order.
void Parser::ParseContinuationCoordinator::finishTryCatchBlock(const Work& work)
{
    TryFrame& frame = tryFrames_[work.frameIndex];
    attachPendingCatch(frame, childValue(work));
    Work next{WorkKind::ContinueTry, frame.resultSlot};
    next.frameIndex = work.frameIndex;
    work_.push_back(next);
}

// Finalizes one pending catch without changing child ordering or range construction.
void Parser::ParseContinuationCoordinator::attachPendingCatch(
    TryFrame& frame,
    ast::AstNode* body)
{
    frame.pendingCatch->setFirst(body);
    frame.pendingCatch->setRange(parser_.tokenRangeFrom(frame.pendingCatchBeginIndex));
    frame.node->addChild(frame.pendingCatch);
    frame.pendingCatch = nullptr;
}

// Schedules the optional finally block or preserves its canonical recovery diagnostic.
void Parser::ParseContinuationCoordinator::scheduleTryFinally(std::size_t frameIndex)
{
    TryFrame& frame = tryFrames_[frameIndex];
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after finally");
        completeTry(frame);
        return;
    }
    Work finish{WorkKind::FinishTryFinallyBlock, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches the final cleanup block and publishes the completed try statement.
void Parser::ParseContinuationCoordinator::finishTryFinallyBlock(const Work& work)
{
    TryFrame& frame = tryFrames_[work.frameIndex];
    frame.node->setSecond(childValue(work));
    completeTry(frame);
}

// Preserves the missing-clause diagnostic and exact full-statement source range.
void Parser::ParseContinuationCoordinator::completeTry(TryFrame& frame)
{
    if (frame.node->children().empty() && !frame.hasFinally)
        parser_.diagnostics_.report(
            diagnostics::DiagnosticId::ExpectedToken,
            diagnostics::DiagnosticSeverity::Error,
            parser_.current().range(),
            "expected 'catch' or 'finally' after try block");
    frame.node->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    complete(frame.resultSlot, frame.node);
}

// Opens one standalone catch frame for the parser's internal catch-clause facade.
void Parser::ParseContinuationCoordinator::startStandaloneCatch(const Work& work)
{
    const std::size_t frameIndex = catchFrames_.size();
    const std::size_t beginIndex = parser_.index_;
    catchFrames_.push_back({beginIndex, work.resultSlot, parseCatchHeader(beginIndex)});
    if (!parser_.atContextualKeyword("when"))
    {
        scheduleStandaloneCatchBody(frameIndex);
        return;
    }
    parser_.consume();
    parser_.expect(TokenKind::LeftParen, "'(' after catch filter 'when'");
    Work finish{WorkKind::FinishStandaloneCatchFilter, work.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Expression, childSlot});
}

// Stores a standalone filter before scheduling the clause body.
void Parser::ParseContinuationCoordinator::finishStandaloneCatchFilter(const Work& work)
{
    catchFrames_[work.frameIndex].clause->setSecond(childValue(work));
    parser_.expect(TokenKind::RightParen, "')' after catch filter expression");
    scheduleStandaloneCatchBody(work.frameIndex);
}

// Schedules a standalone catch body or completes the same malformed-clause recovery shape.
void Parser::ParseContinuationCoordinator::scheduleStandaloneCatchBody(std::size_t frameIndex)
{
    CatchFrame& frame = catchFrames_[frameIndex];
    if (!parser_.at(TokenKind::LeftBrace))
    {
        parser_.expect(TokenKind::LeftBrace, "'{' after catch");
        frame.clause->setRange(parser_.tokenRangeFrom(frame.beginIndex));
        complete(frame.resultSlot, frame.clause);
        return;
    }
    Work finish{WorkKind::FinishStandaloneCatchBlock, frame.resultSlot};
    finish.frameIndex = frameIndex;
    finish.childSlot = makeResultSlot();
    const std::size_t childSlot = finish.childSlot;
    work_.push_back(finish);
    work_.push_back({WorkKind::Block, childSlot});
}

// Attaches and publishes one standalone catch body with its canonical range.
void Parser::ParseContinuationCoordinator::finishStandaloneCatchBlock(const Work& work)
{
    CatchFrame& frame = catchFrames_[work.frameIndex];
    frame.clause->setFirst(childValue(work));
    frame.clause->setRange(parser_.tokenRangeFrom(frame.beginIndex));
    complete(frame.resultSlot, frame.clause);
}

// Parses the resource header and schedules its initializer on the shared expression worklist.
}
