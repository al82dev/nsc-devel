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
void Parser::ParseContinuationCoordinator::startExpression(const Work& work)
{
    if (!parser_.lambdaBlockBodyStartsHere())
    {
        startExpressionContinuation(work, ContinuationRequestKind::Expression);
        return;
    }
    ast::AstNode* lambda = parser_.parseLambdaExpressionHead();
    const std::size_t bodySlot = makeResultSlot();
    Work finish{WorkKind::FinishLambdaBlock, work.resultSlot};
    finish.childSlot = bodySlot;
    finish.node = lambda;
    work_.push_back(std::move(finish));
    work_.push_back({WorkKind::Block, bodySlot});
}

void Parser::ParseContinuationCoordinator::startLoopExpression(const Work& work)
{
    startExpressionContinuation(work, ContinuationRequestKind::LoopExpression);
}

void Parser::ParseContinuationCoordinator::startComprehensionExpression(const Work& work)
{
    startExpressionContinuation(work, ContinuationRequestKind::ComprehensionExpression);
}

void Parser::ParseContinuationCoordinator::startExpressionContinuation(
    const Work& work,
    ContinuationRequestKind rootKind)
{
    const std::size_t frameIndex = expressionFrames_.size();
    expressionFrames_.push_back(
        {work.resultSlot, parser_.makeExpressionContinuation(rootKind)});
    advanceExpression(frameIndex, nullptr);
}

void Parser::ParseContinuationCoordinator::advanceExpression(
    std::size_t frameIndex,
    ast::AstNode* completedBlock)
{
    ExpressionFrame& frame = expressionFrames_[frameIndex];
    const ExpressionContinuationStatus status = frame.continuation->advance(completedBlock);
    if (status == ExpressionContinuationStatus::Complete)
    {
        ast::AstNode* result = frame.continuation->result();
        frame.continuation.reset();
        complete(frame.resultSlot, result);
        return;
    }
    const std::size_t blockSlot = makeResultSlot();
    Work resume{WorkKind::ResumeExpression, frame.resultSlot};
    resume.frameIndex = frameIndex;
    resume.childSlot = blockSlot;
    work_.push_back(std::move(resume));
    work_.push_back({WorkKind::Block, blockSlot});
}

void Parser::ParseContinuationCoordinator::resumeExpression(const Work& work)
{
    advanceExpression(work.frameIndex, childValue(work));
}

// Preserves the inline-assembly output grammar without widening it to assignment-expression.
void Parser::ParseContinuationCoordinator::startUnaryExpression(const Work& work)
{
    complete(work.resultSlot, parser_.parseUnaryExpression());
}

// Preserves the historical guard grammar without widening it to assignment-expression.
void Parser::ParseContinuationCoordinator::startConditionalExpression(const Work& work)
{
    complete(work.resultSlot, parser_.parseConditionalExpression());
}

// Attaches the completed block with the canonical lambda flags and range update.
void Parser::ParseContinuationCoordinator::finishLambdaBlock(const Work& work)
{
    parser_.finishLambdaExpression(work.node, childValue(work), true);
    complete(work.resultSlot, work.node);
}

// Creates one lexical block frame and schedules its first bounded continuation.
}
