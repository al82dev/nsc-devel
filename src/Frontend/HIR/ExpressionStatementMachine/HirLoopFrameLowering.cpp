#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <utility>

namespace nativesharp::frontend
{
using internal::ForStatementFrameState;
using internal::ForeachStatementFrameState;
using internal::IterativeExpressionFrame;
using internal::IterativeExpressionKind;
using internal::IterativeInputKind;
using internal::LoopBlockAdvance;

LoopBlockAdvance ProjectOopHirModel::advanceForStatementFrame(
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error,
    std::size_t frameIndex)
{
    while (true)
    {
        IterativeExpressionFrame& frame = frames[frameIndex];
        if (frame.stage == 0U)
        {
            const LoopBlockAdvance advance = advanceForInitializer(
                frames, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, error, frameIndex);
            if (advance == LoopBlockAdvance::Complete) continue;
            return advance;
        }
        if (frame.stage == 1U)
        {
            const LoopBlockAdvance advance = advanceForCondition(
                frame, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind);
            if (advance == LoopBlockAdvance::Complete) continue;
            return advance;
        }
        if (frame.stage == 2U)
        {
            const LoopBlockAdvance advance = advanceForIncrement(
                frame, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind);
            if (advance == LoopBlockAdvance::Complete) continue;
            return advance;
        }
        if (frame.stage == 3U)
            return advanceForBody(
                frame, current, currentMemberOrdinal, currentAddressContext,
                currentInputKind, error);
        return LoopBlockAdvance::Complete;
    }
}

LoopBlockAdvance ProjectOopHirModel::advanceForInitializer(
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error,
    std::size_t frameIndex)
{
    IterativeExpressionFrame& frame = frames[frameIndex];
    ForStatementFrameState& state = *frame.forStatement;
    frame.stage = 1U;
    const ast::AstNode* init = frame.syntax->first();
    if (init == nullptr)
    {
        state.node->children.push_back(nullptr);
        return LoopBlockAdvance::Complete;
    }
    if (init->kind() == ast::AstNodeKind::BlockStatement)
    {
        current = init;
        currentMemberOrdinal = frame.memberOrdinal;
        currentAddressContext = false;
        currentInputKind = IterativeInputKind::LoopBlock;
        return LoopBlockAdvance::Descend;
    }
    if (init->kind() != ast::AstNodeKind::VariableDeclarationStatement)
    {
        current = init;
        currentMemberOrdinal = frame.memberOrdinal;
        currentAddressContext = false;
        currentInputKind = IterativeInputKind::Expression;
        return LoopBlockAdvance::Descend;
    }
    IterativeExpressionFrame variableFrame;
    variableFrame.kind = IterativeExpressionKind::VariableDeclaration;
    variableFrame.syntax = init;
    variableFrame.memberOrdinal = frame.memberOrdinal;
    if (!initializeVariableDeclarationFrame(variableFrame, error))
        return LoopBlockAdvance::Error;
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (!nextVariableDeclarationChild(
            variableFrame, childSyntax, childAddressContext))
    {
        auto node = finishVariableDeclarationFrame(variableFrame);
        activateDirectStatementCleanup(node);
        state.node->children.push_back(std::move(node));
        return LoopBlockAdvance::Complete;
    }
    currentMemberOrdinal = variableFrame.memberOrdinal;
    frames.push_back(std::move(variableFrame));
    current = childSyntax;
    currentAddressContext = childAddressContext;
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

LoopBlockAdvance ProjectOopHirModel::advanceForCondition(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind)
{
    frame.stage = 2U;
    const ast::AstNode* condition = frame.syntax->second();
    if (condition == nullptr)
    {
        frame.forStatement->node->children.push_back(nullptr);
        return LoopBlockAdvance::Complete;
    }
    current = condition;
    currentMemberOrdinal = frame.memberOrdinal;
    currentAddressContext = false;
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

LoopBlockAdvance ProjectOopHirModel::advanceForIncrement(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind)
{
    frame.stage = 3U;
    const ast::AstNode* increment = frame.syntax->third();
    if (increment == nullptr)
    {
        frame.forStatement->node->children.push_back(nullptr);
        return LoopBlockAdvance::Complete;
    }
    current = increment;
    currentMemberOrdinal = frame.memberOrdinal;
    currentAddressContext = false;
    currentInputKind = increment->kind() == ast::AstNodeKind::BlockStatement
        ? IterativeInputKind::LoopBlock
        : IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

LoopBlockAdvance ProjectOopHirModel::advanceForBody(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    if (frame.syntax->children().empty())
    {
        error = "Stage 34 for statement is missing a body";
        return LoopBlockAdvance::Error;
    }
    frame.stage = 4U;
    current = frame.syntax->children().front();
    currentMemberOrdinal = frame.memberOrdinal;
    currentAddressContext = false;
    currentInputKind = IterativeInputKind::LoopBlock;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceForeachSimpleSource(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* source,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    std::optional<LoopBlockAdvance> advance = advanceForeachBasicSource(
        frame, source, frames, current, currentMemberOrdinal,
        currentAddressContext, currentInputKind);
    if (!advance.has_value())
    {
        advance = advanceForeachDeclarationSource(
            frame, source, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
    }
    if (!advance.has_value())
    {
        advance = advanceForeachAssemblySource(
            frame, source, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
    }
    return advance;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceForeachBasicSource(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* source,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind)
{
    if (source != nullptr && source->kind() == ast::AstNodeKind::BlockStatement)
    {
        current = source;
        currentMemberOrdinal = frame.memberOrdinal;
        currentAddressContext = false;
        currentInputKind = IterativeInputKind::LoopBlock;
        return LoopBlockAdvance::Descend;
    }
    if (source != nullptr && source->kind() == ast::AstNodeKind::ExpressionStatement)
    {
        IterativeExpressionFrame sourceFrame;
        sourceFrame.kind = IterativeExpressionKind::LoopBlockExpressionStatement;
        sourceFrame.syntax = source;
        sourceFrame.memberOrdinal = frame.memberOrdinal;
        frames.push_back(std::move(sourceFrame));
        current = source->first();
        currentMemberOrdinal = frames.back().memberOrdinal;
        currentAddressContext = false;
        currentInputKind = IterativeInputKind::Expression;
        return LoopBlockAdvance::Descend;
    }
    return std::nullopt;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceForeachDeclarationSource(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* source,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    ForeachStatementFrameState& state = *frame.foreachStatement;
    if (source != nullptr &&
        source->kind() == ast::AstNodeKind::VariableDeclarationStatement)
    {
        IterativeExpressionFrame sourceFrame;
        sourceFrame.kind = IterativeExpressionKind::VariableDeclaration;
        sourceFrame.syntax = source;
        sourceFrame.memberOrdinal = frame.memberOrdinal;
        if (!initializeVariableDeclarationFrame(sourceFrame, error))
            return LoopBlockAdvance::Error;
        const ast::AstNode* childSyntax = nullptr;
        bool childAddressContext = false;
        if (nextVariableDeclarationChild(
                sourceFrame, childSyntax, childAddressContext))
        {
            currentMemberOrdinal = sourceFrame.memberOrdinal;
            frames.push_back(std::move(sourceFrame));
            current = childSyntax;
            currentAddressContext = childAddressContext;
            currentInputKind = IterativeInputKind::Expression;
            return LoopBlockAdvance::Descend;
        }
        state.node->children.push_back(
            finishVariableDeclarationFrame(sourceFrame));
        return LoopBlockAdvance::Complete;
    }
    if (source != nullptr &&
        source->kind() == ast::AstNodeKind::DeconstructionDeclarationStatement)
    {
        IterativeExpressionFrame sourceFrame;
        sourceFrame.kind = IterativeExpressionKind::DeconstructionDeclaration;
        sourceFrame.syntax = source;
        sourceFrame.memberOrdinal = frame.memberOrdinal;
        if (!initializeDeconstructionDeclarationFrame(sourceFrame, error))
            return LoopBlockAdvance::Error;
        const ast::AstNode* childSyntax = nullptr;
        bool childAddressContext = false;
        if (nextDeconstructionDeclarationChild(
                sourceFrame, childSyntax, childAddressContext))
        {
            currentMemberOrdinal = sourceFrame.memberOrdinal;
            frames.push_back(std::move(sourceFrame));
            current = childSyntax;
            currentAddressContext = childAddressContext;
            currentInputKind = IterativeInputKind::Expression;
            return LoopBlockAdvance::Descend;
        }
        auto node = finishDeconstructionDeclarationFrame(sourceFrame, error);
        if (node == nullptr) return LoopBlockAdvance::Error;
        state.node->children.push_back(std::move(node));
        return LoopBlockAdvance::Complete;
    }
    return std::nullopt;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceForeachAssemblySource(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* source,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    ForeachStatementFrameState& state = *frame.foreachStatement;
    if (source == nullptr ||
        source->kind() != ast::AstNodeKind::InlineAssemblyStatement)
    {
        return std::nullopt;
    }
    IterativeExpressionFrame sourceFrame;
    sourceFrame.kind = IterativeExpressionKind::InlineAssemblyStatement;
    sourceFrame.syntax = source;
    sourceFrame.memberOrdinal = frame.memberOrdinal;
    initializeInlineAssemblyFrame(sourceFrame);
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    const bool hasChild = advanceInlineAssemblyFrame(
        sourceFrame, childSyntax, childAddressContext, error);
    if (!error.empty()) return LoopBlockAdvance::Error;
    if (hasChild)
    {
        currentMemberOrdinal = sourceFrame.memberOrdinal;
        frames.push_back(std::move(sourceFrame));
        current = childSyntax;
        currentAddressContext = childAddressContext;
        currentInputKind = IterativeInputKind::Expression;
        return LoopBlockAdvance::Descend;
    }
    state.node->children.push_back(finishInlineAssemblyFrame(sourceFrame));
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceForeachControlSource(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* source,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error,
    std::size_t& frameIndex)
{
    std::optional<LoopBlockAdvance> advance = advanceForeachConditionalSource(
        frame, source, frames, current, currentMemberOrdinal,
        currentAddressContext, currentInputKind, error);
    if (!advance.has_value())
    {
        advance = advanceForeachSwitchSource(
            frame, source, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
    }
    if (!advance.has_value())
    {
        advance = advanceForeachIterationSource(
            frame, source, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error, frameIndex);
    }
    return advance;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceForeachConditionalSource(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* source,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    if (source != nullptr &&
        source->kind() == ast::AstNodeKind::StaticAssertStatement)
    {
        IterativeExpressionFrame sourceFrame;
        sourceFrame.kind = IterativeExpressionKind::StaticAssertStatement;
        sourceFrame.syntax = source;
        sourceFrame.memberOrdinal = frame.memberOrdinal;
        frames.push_back(std::move(sourceFrame));
        current = source->first();
        currentMemberOrdinal = frames.back().memberOrdinal;
        currentAddressContext = false;
        currentInputKind = IterativeInputKind::Expression;
        return LoopBlockAdvance::Descend;
    }
    if (source != nullptr && source->kind() == ast::AstNodeKind::IfStatement)
    {
        IterativeExpressionFrame sourceFrame;
        sourceFrame.kind = IterativeExpressionKind::IfStatement;
        sourceFrame.syntax = source;
        sourceFrame.memberOrdinal = frame.memberOrdinal;
        sourceFrame.loopBlockNode = makeNode(
            ProjectOopHirNodeKind::If, source, "builtin:void");
        frames.push_back(std::move(sourceFrame));
        current = source->first();
        currentMemberOrdinal = frames.back().memberOrdinal;
        currentAddressContext = false;
        currentInputKind = IterativeInputKind::Expression;
        return LoopBlockAdvance::Descend;
    }
    if (source != nullptr &&
        (source->kind() == ast::AstNodeKind::WhileStatement ||
         source->kind() == ast::AstNodeKind::DoWhileStatement))
    {
        IterativeExpressionFrame sourceFrame;
        sourceFrame.kind = IterativeExpressionKind::LoopStatement;
        sourceFrame.syntax = source;
        sourceFrame.memberOrdinal = frame.memberOrdinal;
        sourceFrame.loopBlockNode = makeNode(
            source->kind() == ast::AstNodeKind::DoWhileStatement
                ? ProjectOopHirNodeKind::DoWhile
                : ProjectOopHirNodeKind::While,
            source,
            "builtin:void");
        frames.push_back(std::move(sourceFrame));
        current = source->first();
        currentMemberOrdinal = frames.back().memberOrdinal;
        currentAddressContext = false;
        currentInputKind = IterativeInputKind::Expression;
        return LoopBlockAdvance::Descend;
    }
    return std::nullopt;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceForeachSwitchSource(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* source,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    if (source == nullptr || source->kind() != ast::AstNodeKind::SwitchStatement)
        return std::nullopt;
    IterativeExpressionFrame sourceFrame;
    sourceFrame.memberOrdinal = frame.memberOrdinal;
    if (!initializeSwitchStatementFrame(sourceFrame, source, error))
        return LoopBlockAdvance::Error;
    frames.push_back(std::move(sourceFrame));
    current = source->first();
    currentMemberOrdinal = frames.back().memberOrdinal;
    currentAddressContext = false;
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceForeachIterationSource(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* source,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error,
    std::size_t& frameIndex)
{
    if (source != nullptr && source->kind() == ast::AstNodeKind::ForStatement)
    {
        IterativeExpressionFrame sourceFrame;
        sourceFrame.memberOrdinal = frame.memberOrdinal;
        initializeForStatementFrame(sourceFrame, source);
        frames.push_back(std::move(sourceFrame));
        return advanceForStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, frames.size() - 1U);
    }
    if (source == nullptr ||
        source->kind() != ast::AstNodeKind::ForeachStatement)
    {
        return std::nullopt;
    }
    IterativeExpressionFrame sourceFrame;
    sourceFrame.memberOrdinal = frame.memberOrdinal;
    if (!initializeForeachStatementFrame(sourceFrame, source, error))
        return LoopBlockAdvance::Error;
    frames.push_back(std::move(sourceFrame));
    frameIndex = frames.size() - 1U;
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceForeachValueSource(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* source,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    if (isExitValueStatement(source))
    {
        return beginExitValueStatement(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, source, frame.memberOrdinal)
            ? LoopBlockAdvance::Descend
            : LoopBlockAdvance::Error;
    }
    if (isCoroutineValueStatement(source))
    {
        beginCoroutineValueStatement(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, source, frame.memberOrdinal);
        return LoopBlockAdvance::Descend;
    }
    if (source != nullptr && source->kind() == ast::AstNodeKind::UsingStatement)
    {
        return beginUsingStatement(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, source, frame.memberOrdinal)
            ? LoopBlockAdvance::Descend
            : LoopBlockAdvance::Error;
    }
    if (source == nullptr || source->kind() != ast::AstNodeKind::TryStatement)
        return std::nullopt;
    beginTryStatement(
        frames, current, currentMemberOrdinal, currentAddressContext,
        currentInputKind, source, frame.memberOrdinal);
    return LoopBlockAdvance::Descend;
}

LoopBlockAdvance ProjectOopHirModel::advanceForeachStatementFrame(
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error,
    std::size_t frameIndex)
{
    while (true)
    {
        IterativeExpressionFrame& frame = frames[frameIndex];
        ForeachStatementFrameState& state = *frame.foreachStatement;
        if (frame.stage != 0U) return LoopBlockAdvance::Complete;
        frame.stage = 1U;
        const ast::AstNode* source = frame.syntax->first();
        const std::size_t sourceFrameIndex = frameIndex;
        std::optional<LoopBlockAdvance> advance = advanceForeachSimpleSource(
            frame, source, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
        if (!advance.has_value())
            advance = advanceForeachControlSource(
                frame, source, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, error, frameIndex);
        if (advance.has_value() && *advance == LoopBlockAdvance::Complete &&
            frameIndex != sourceFrameIndex)
        {
            continue;
        }
        if (!advance.has_value())
            advance = advanceForeachValueSource(
                frame, source, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, error);
        if (advance.has_value() && *advance != LoopBlockAdvance::Complete)
            return *advance;
        if (!advance.has_value())
        {
            auto sourceNode = lowerStatement(source, frame.memberOrdinal, error);
            if (sourceNode == nullptr) return LoopBlockAdvance::Error;
            state.node->children.push_back(std::move(sourceNode));
        }
        IterativeExpressionFrame& activeFrame = frames[frameIndex];
        ForeachStatementFrameState& activeState = *activeFrame.foreachStatement;
        activeFrame.stage = 2U;
        current = activeState.chosenCandidate;
        currentMemberOrdinal = activeFrame.memberOrdinal;
        currentAddressContext = false;
        currentInputKind = IterativeInputKind::LoopBlock;
        return LoopBlockAdvance::Descend;
    }
}
}
