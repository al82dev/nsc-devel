#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <algorithm>
#include <utility>

namespace nativesharp::frontend
{
using internal::IterativeExpressionFrame;
using internal::IterativeExpressionKind;
using internal::IterativeInputKind;
using internal::LoopBlockAdvance;
using internal::TryStatementFrameState;

void ProjectOopHirModel::rollbackMatchCleanups(
    std::vector<IterativeExpressionFrame>& frames,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    // A failed If condition must still lower its then block to preserve
    // the historical diagnostic ordering.  Its continuation owns the
    // precise descendant unwind, so do not discard enclosing cleanup
    // state before that continuation has had a chance to run.
    for (auto frame = frames.rbegin(); frame != frames.rend(); ++frame)
    {
        if ((frame->kind == IterativeExpressionKind::IfStatement ||
             frame->kind == IterativeExpressionKind::LoopStatement) &&
            frame->stage == 0U)
        {
            return;
        }
    }
    bool loopBlockActive = false;
    for (auto frame = frames.rbegin(); frame != frames.rend(); ++frame)
    {
        if (frame->kind == IterativeExpressionKind::Match && frame->cleanupActive)
            activeHirCleanupRegions_.resize(frame->cleanupDepth);
        if (frame->kind == IterativeExpressionKind::SwitchStatement &&
            frame->cleanupActive)
        {
            activeHirCleanupRegions_.resize(frame->cleanupDepth);
        }
        if (frame->kind == IterativeExpressionKind::ForStatement &&
            frame->cleanupActive)
        {
            activeHirCleanupRegions_.resize(frame->cleanupDepth);
        }
        if (frame->kind == IterativeExpressionKind::UsingStatement &&
            frame->cleanupActive)
        {
            activeHirCleanupRegions_.resize(frame->cleanupDepth);
        }
        if (frame->kind == IterativeExpressionKind::TryStatement &&
            frame->tryStatement.has_value())
        {
            TryStatementFrameState& state = *frame->tryStatement;
            if (state.handlerActive)
            {
                activeHirExceptionHandlers_.resize(state.exceptionHandlerDepth);
                state.handlerActive = false;
            }
            if (frame->cleanupActive)
            {
                activeHirCleanupRegions_.resize(frame->cleanupDepth);
                frame->cleanupActive = false;
            }
        }
        if (frame->kind == IterativeExpressionKind::LoopBlock)
            loopBlockActive = true;
    }
    if (loopBlockActive)
    {
        activeHirCleanupRegions_.resize(rootCleanupDepth);
        activeHirExceptionHandlers_.resize(rootExceptionHandlerDepth);
    }
}

bool ProjectOopHirModel::resumePendingConditionStatementAfterFailure(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error)
{
    for (std::size_t index = frames.size(); index > 0U; --index)
    {
        IterativeExpressionFrame& candidate = frames[index - 1U];
        if ((candidate.kind != IterativeExpressionKind::IfStatement &&
             candidate.kind != IterativeExpressionKind::LoopStatement) ||
            candidate.stage != 0U || candidate.syntax == nullptr)
        {
            continue;
        }
        std::size_t retainedCleanupDepth = activeHirCleanupRegions_.size();
        std::size_t retainedExceptionHandlerDepth =
            activeHirExceptionHandlers_.size();
        for (std::size_t child = index; child < frames.size(); ++child)
        {
            if (frames[child].kind == IterativeExpressionKind::LoopBlock)
            {
                retainedCleanupDepth = std::min(
                    retainedCleanupDepth, frames[child].cleanupDepth);
            }
            if (frames[child].kind == IterativeExpressionKind::Match &&
                frames[child].cleanupActive)
            {
                retainedCleanupDepth = std::min(
                    retainedCleanupDepth, frames[child].cleanupDepth);
            }
            if (frames[child].kind == IterativeExpressionKind::TryStatement &&
                frames[child].tryStatement.has_value())
            {
                const TryStatementFrameState& state = *frames[child].tryStatement;
                if (frames[child].cleanupActive)
                {
                    retainedCleanupDepth = std::min(
                        retainedCleanupDepth, frames[child].cleanupDepth);
                }
                if (state.handlerActive)
                {
                    retainedExceptionHandlerDepth = std::min(
                        retainedExceptionHandlerDepth,
                        state.exceptionHandlerDepth);
                }
            }
        }
        activeHirCleanupRegions_.resize(retainedCleanupDepth);
        activeHirExceptionHandlers_.resize(retainedExceptionHandlerDepth);
        frames.resize(index);
        IterativeExpressionFrame& pendingStatement = frames.back();
        pendingStatement.statementConditionFailed = true;
        pendingStatement.statementConditionError = std::move(error);
        pendingStatement.stage = 1U;
        current = pendingStatement.syntax->second();
        currentMemberOrdinal = pendingStatement.memberOrdinal;
        currentAddressContext = false;
        currentInputKind = IterativeInputKind::LoopBlock;
        lowered.reset();
        error.clear();
        return true;
    }
    return false;
}

void ProjectOopHirModel::activateLocalCleanup(
    const std::unique_ptr<ProjectOopHirNode>& node)
{
    const ProjectOopHirNode* declaration = node.get();
    if (declaration != nullptr &&
        declaration->kind == ProjectOopHirNodeKind::Block &&
        declaration->syntax != nullptr &&
        declaration->syntax->kind() ==
            ast::AstNodeKind::VariableDeclarationStatement &&
        !declaration->children.empty())
    {
        declaration = declaration->children.back().get();
    }
    if (declaration != nullptr &&
        declaration->kind == ProjectOopHirNodeKind::LocalDeclaration &&
        declaration->cleanupRegionOrdinal.has_value())
    {
        activeHirCleanupRegions_.push_back(
            *declaration->cleanupRegionOrdinal);
    }
}

void ProjectOopHirModel::activateDirectStatementCleanup(
    const std::unique_ptr<ProjectOopHirNode>& node)
{
    if (node != nullptr &&
        node->kind == ProjectOopHirNodeKind::LocalDeclaration &&
        node->cleanupRegionOrdinal.has_value())
    {
        activeHirCleanupRegions_.push_back(*node->cleanupRegionOrdinal);
    }
}

bool ProjectOopHirModel::isExitValueStatement(
    const ast::AstNode* statement) noexcept
{
    return statement != nullptr && statement->first() != nullptr &&
        (statement->kind() == ast::AstNodeKind::BreakStatement ||
         statement->kind() == ast::AstNodeKind::ReturnStatement ||
         statement->kind() == ast::AstNodeKind::ThrowStatement);
}

bool ProjectOopHirModel::isCoroutineValueStatement(
    const ast::AstNode* statement) noexcept
{
    return statement != nullptr && statement->first() != nullptr &&
        statement->kind() == ast::AstNodeKind::YieldReturnStatement;
}

bool ProjectOopHirModel::beginExitValueStatement(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error,
    const ast::AstNode* statement,
    std::size_t statementMemberOrdinal)
{
    IterativeExpressionFrame exitFrame;
    exitFrame.memberOrdinal = statementMemberOrdinal;
    if (!initializeExitValueStatementFrame(exitFrame, statement, error))
        return false;
    const bool valueAddressContext = exitFrame.addressContext;
    frames.push_back(std::move(exitFrame));
    current = statement->first();
    currentMemberOrdinal = statementMemberOrdinal;
    currentAddressContext = valueAddressContext;
    currentInputKind = IterativeInputKind::Expression;
    return true;
}

void ProjectOopHirModel::beginCoroutineValueStatement(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    const ast::AstNode* statement,
    std::size_t statementMemberOrdinal)
{
    IterativeExpressionFrame coroutineFrame;
    coroutineFrame.memberOrdinal = statementMemberOrdinal;
    initializeCoroutineValueStatementFrame(
        coroutineFrame, statement, statementMemberOrdinal);
    const bool valueAddressContext = coroutineFrame.addressContext;
    frames.push_back(std::move(coroutineFrame));
    current = statement->first();
    currentMemberOrdinal = statementMemberOrdinal;
    currentAddressContext = valueAddressContext;
    currentInputKind = IterativeInputKind::Expression;
}

bool ProjectOopHirModel::beginUsingStatement(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error,
    const ast::AstNode* statement,
    std::size_t statementMemberOrdinal)
{
    IterativeExpressionFrame usingFrame;
    usingFrame.memberOrdinal = statementMemberOrdinal;
    if (!initializeUsingStatementFrame(usingFrame, statement, error))
        return false;
    frames.push_back(std::move(usingFrame));
    current = statement->first();
    currentMemberOrdinal = statementMemberOrdinal;
    currentAddressContext = false;
    currentInputKind = IterativeInputKind::Expression;
    return true;
}

void ProjectOopHirModel::beginTryStatement(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    const ast::AstNode* statement,
    std::size_t statementMemberOrdinal)
{
    IterativeExpressionFrame tryFrame;
    tryFrame.memberOrdinal = statementMemberOrdinal;
    initializeTryStatementFrame(tryFrame, statement);
    frames.push_back(std::move(tryFrame));
    current = statement->first();
    currentMemberOrdinal = statementMemberOrdinal;
    currentAddressContext = false;
    currentInputKind = IterativeInputKind::LoopBlock;
}

bool ProjectOopHirModel::initializeStatementContinuationFrame(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statementContinuation,
    std::size_t memberOrdinal,
    bool& addressContext,
    std::string& error)
{
    std::optional<bool> initialized =
        initializeDeclarationContinuationFrame(frame, statementContinuation);
    if (!initialized.has_value())
    {
        initialized = initializeControlContinuationFrame(
            frame, statementContinuation, error);
    }
    if (!initialized.has_value())
    {
        initialized = initializeValueContinuationFrame(
            frame, statementContinuation, memberOrdinal, addressContext, error);
    }
    if (initialized.has_value() && !*initialized)
        return false;
    if (!initialized.has_value())
        frame.kind = IterativeExpressionKind::ExpressionStatement;
    frame.syntax = statementContinuation;
    frame.memberOrdinal = memberOrdinal;
    return true;
}

std::optional<bool> ProjectOopHirModel::initializeDeclarationContinuationFrame(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statementContinuation)
{
    if (statementContinuation->kind() ==
        ast::AstNodeKind::VariableDeclarationStatement)
    {
        frame.kind = IterativeExpressionKind::VariableDeclaration;
    }
    else if (statementContinuation->kind() ==
        ast::AstNodeKind::DeconstructionDeclarationStatement)
    {
        frame.kind = IterativeExpressionKind::DeconstructionDeclaration;
    }
    else if (statementContinuation->kind() ==
        ast::AstNodeKind::InlineAssemblyStatement)
    {
        frame.kind = IterativeExpressionKind::InlineAssemblyStatement;
    }
    else if (statementContinuation->kind() ==
        ast::AstNodeKind::StaticAssertStatement)
    {
        frame.kind = IterativeExpressionKind::StaticAssertStatement;
    }
    else
    {
        return std::nullopt;
    }
    return true;
}

std::optional<bool> ProjectOopHirModel::initializeControlContinuationFrame(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statementContinuation,
    std::string& error)
{
    if (statementContinuation->kind() == ast::AstNodeKind::IfStatement)
    {
        frame.kind = IterativeExpressionKind::IfStatement;
        frame.loopBlockNode = makeNode(
            ProjectOopHirNodeKind::If,
            statementContinuation,
            "builtin:void");
    }
    else if (statementContinuation->kind() == ast::AstNodeKind::WhileStatement ||
        statementContinuation->kind() == ast::AstNodeKind::DoWhileStatement)
    {
        frame.kind = IterativeExpressionKind::LoopStatement;
        frame.loopBlockNode = makeNode(
            statementContinuation->kind() == ast::AstNodeKind::DoWhileStatement
                ? ProjectOopHirNodeKind::DoWhile
                : ProjectOopHirNodeKind::While,
            statementContinuation,
            "builtin:void");
    }
    else if (statementContinuation->kind() == ast::AstNodeKind::SwitchStatement)
    {
        if (!initializeSwitchStatementFrame(frame, statementContinuation, error))
            return false;
    }
    else if (statementContinuation->kind() == ast::AstNodeKind::ForStatement)
    {
        initializeForStatementFrame(frame, statementContinuation);
    }
    else if (statementContinuation->kind() == ast::AstNodeKind::ForeachStatement)
    {
        if (!initializeForeachStatementFrame(frame, statementContinuation, error))
            return false;
    }
    else
    {
        return std::nullopt;
    }
    return true;
}

std::optional<bool> ProjectOopHirModel::initializeValueContinuationFrame(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statementContinuation,
    std::size_t memberOrdinal,
    bool& addressContext,
    std::string& error)
{
    if ((statementContinuation->kind() == ast::AstNodeKind::BreakStatement ||
              statementContinuation->kind() == ast::AstNodeKind::ReturnStatement ||
              statementContinuation->kind() == ast::AstNodeKind::ThrowStatement) &&
        statementContinuation->first() != nullptr)
    {
        frame.memberOrdinal = memberOrdinal;
        if (!initializeExitValueStatementFrame(frame, statementContinuation, error))
            return false;
        addressContext = frame.addressContext;
    }
    else if (statementContinuation->kind() ==
            ast::AstNodeKind::YieldReturnStatement &&
        statementContinuation->first() != nullptr)
    {
        initializeCoroutineValueStatementFrame(
            frame, statementContinuation, memberOrdinal);
        addressContext = frame.addressContext;
    }
    else if (statementContinuation->kind() == ast::AstNodeKind::UsingStatement)
    {
        if (!initializeUsingStatementFrame(frame, statementContinuation, error))
            return false;
    }
    else if (statementContinuation->kind() == ast::AstNodeKind::TryStatement)
    {
        initializeTryStatementFrame(frame, statementContinuation);
    }
    else
    {
        return std::nullopt;
    }
    return true;
}

bool ProjectOopHirModel::initializeRootLoopWorklist(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error,
    bool& worklistComplete,
    std::size_t rootCleanupDepth,
    std::size_t rootExceptionHandlerDepth)
{
    if (frames.size() != 1U) return true;
    LoopBlockAdvance advance = LoopBlockAdvance::Complete;
    if (frames.back().kind == IterativeExpressionKind::ForStatement)
    {
        advance = advanceForStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, 0U);
    }
    else if (frames.back().kind == IterativeExpressionKind::ForeachStatement)
    {
        advance = advanceForeachStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, 0U);
    }
    else
    {
        return true;
    }
    if (advance == LoopBlockAdvance::Error)
    {
        rollbackMatchCleanups(
            frames, rootCleanupDepth, rootExceptionHandlerDepth);
        return false;
    }
    if (advance == LoopBlockAdvance::Complete)
        worklistComplete = true;
    return true;
}

bool ProjectOopHirModel::initializeRootVariableWorklist(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    bool& currentAddressContext,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    bool& worklistComplete)
{
    if (!initializeVariableDeclarationFrame(frames.back(), error))
    {
        if (!lastErrorRange_.isValid())
            lastErrorRange_ = frames.back().syntax->range();
        return false;
    }
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (nextVariableDeclarationChild(
            frames.back(), childSyntax, childAddressContext))
    {
        current = childSyntax;
        currentAddressContext = childAddressContext;
        return true;
    }
    lowered = finishVariableDeclarationFrame(frames.back());
    frames.pop_back();
    worklistComplete = true;
    return true;
}

bool ProjectOopHirModel::initializeRootDeconstructionWorklist(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    bool& currentAddressContext,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    bool& worklistComplete,
    const ast::AstNode* statementContinuation)
{
    if (!initializeDeconstructionDeclarationFrame(frames.back(), error))
    {
        if (!lastErrorRange_.isValid())
            lastErrorRange_ = frames.back().syntax->range();
        return false;
    }
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (nextDeconstructionDeclarationChild(
            frames.back(), childSyntax, childAddressContext))
    {
        current = childSyntax;
        currentAddressContext = childAddressContext;
        return true;
    }
    lowered = finishDeconstructionDeclarationFrame(frames.back(), error);
    frames.pop_back();
    worklistComplete = lowered != nullptr;
    if (worklistComplete) return true;
    if (!lastErrorRange_.isValid())
        lastErrorRange_ = statementContinuation->range();
    return false;
}

bool ProjectOopHirModel::initializeRootInlineAssemblyWorklist(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    bool& currentAddressContext,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    bool& worklistComplete)
{
    initializeInlineAssemblyFrame(frames.back());
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    const bool hasChild = advanceInlineAssemblyFrame(
        frames.back(), childSyntax, childAddressContext, error);
    if (!error.empty())
    {
        if (!lastErrorRange_.isValid())
            lastErrorRange_ = frames.back().syntax->range();
        return false;
    }
    if (hasChild)
    {
        current = childSyntax;
        currentAddressContext = childAddressContext;
        return true;
    }
    lowered = finishInlineAssemblyFrame(frames.back());
    frames.pop_back();
    worklistComplete = true;
    return true;
}

bool ProjectOopHirModel::initializeRootDeclarationWorklist(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    bool& currentAddressContext,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    bool& worklistComplete,
    const ast::AstNode* statementContinuation)
{
    if (frames.size() != 1U) return true;
    if (frames.back().kind == IterativeExpressionKind::VariableDeclaration)
    {
        return initializeRootVariableWorklist(
            frames, current, currentAddressContext,
            lowered, error, worklistComplete);
    }
    if (frames.back().kind == IterativeExpressionKind::DeconstructionDeclaration)
    {
        return initializeRootDeconstructionWorklist(
            frames, current, currentAddressContext, lowered, error,
            worklistComplete, statementContinuation);
    }
    if (frames.back().kind != IterativeExpressionKind::InlineAssemblyStatement)
        return true;
    return initializeRootInlineAssemblyWorklist(
        frames, current, currentAddressContext,
        lowered, error, worklistComplete);
}
}
