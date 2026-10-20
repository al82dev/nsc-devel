#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <utility>

namespace nativesharp::frontend
{
using internal::IterativeExpressionFrame;
using internal::IterativeExpressionKind;
using internal::IterativeInputKind;
using internal::LoopBlockAdvance;
using internal::SwitchStatementFrameState;

LoopBlockAdvance ProjectOopHirModel::advanceSwitchStatementFrame(
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error,
    std::size_t frameIndex)
{
    while (true)
    {
        IterativeExpressionFrame& frame = frames[frameIndex];
        std::optional<LoopBlockAdvance> advance =
            beginNextSwitchSection(frame, frames, lowered, error);
        if (advance.has_value()) return *advance;
        const ast::AstNode* statement = nextSwitchSectionStatement(frame);
        if (statement == nullptr) continue;
        frame.stage = 2U;
        advance = advanceSwitchSimpleStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind);
        if (advance.has_value()) return *advance;
        advance = advanceSwitchDeclarationStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
        if (advance.has_value())
        {
            if (*advance == LoopBlockAdvance::Complete) continue;
            return *advance;
        }
        advance = advanceSwitchControlStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
        if (advance.has_value()) return *advance;
        advance = advanceSwitchIterationStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
        if (advance.has_value()) return *advance;
        advance = advanceSwitchValueStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
        if (advance.has_value()) return *advance;
        SwitchStatementFrameState& state = *frame.switchStatement;
        auto node = lowerStatement(statement, frame.memberOrdinal, error);
        if (node == nullptr) return LoopBlockAdvance::Error;
        activateDirectStatementCleanup(node);
        state.sectionBody->children.push_back(std::move(node));
        frame.stage = 1U;
    }
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::beginNextSwitchSection(
    IterativeExpressionFrame& frame,
    std::vector<IterativeExpressionFrame>& frames,
    std::unique_ptr<ProjectOopHirNode>& lowered,
    std::string& error)
{
    SwitchStatementFrameState& state = *frame.switchStatement;
    if (state.sectionSyntax != nullptr) return std::nullopt;
    while (state.nextSyntaxChild < frame.syntax->children().size())
    {
        const ast::AstNode* candidate =
            frame.syntax->children()[state.nextSyntaxChild++];
        if (candidate == nullptr ||
            candidate->kind() != ast::AstNodeKind::SwitchSection)
            continue;
        if (state.nextBindingSection >= state.binding->sections.size())
        {
            error = "Stage 34 OOP HIR found more switch sections than its semantic binding";
            return LoopBlockAdvance::Error;
        }
        const MethodSwitchSectionBinding& sectionBinding =
            state.binding->sections[state.nextBindingSection++];
        state.sectionSyntax = candidate;
        state.nextSectionChild = 0U;
        state.sectionNode = makeNode(
            ProjectOopHirNodeKind::SwitchSection, candidate, "builtin:void");
        state.sectionNode->switchLabels = sectionBinding.labelValues;
        state.sectionNode->switchHasDefault = sectionBinding.hasDefault;
        state.sectionBody = makeNode(
            ProjectOopHirNodeKind::Block, candidate, "builtin:void");
        frame.cleanupDepth = activeHirCleanupRegions_.size();
        frame.cleanupActive = true;
        break;
    }
    if (state.sectionSyntax != nullptr) return std::nullopt;
    lowered = std::move(state.node);
    frames.pop_back();
    return LoopBlockAdvance::Complete;
}

const ast::AstNode* ProjectOopHirModel::nextSwitchSectionStatement(
    IterativeExpressionFrame& frame)
{
    SwitchStatementFrameState& state = *frame.switchStatement;
    while (state.nextSectionChild < state.sectionSyntax->children().size())
    {
        const ast::AstNode* candidate =
            state.sectionSyntax->children()[state.nextSectionChild++];
        if (candidate == nullptr ||
            candidate->kind() == ast::AstNodeKind::SwitchLabel)
            continue;
        return candidate;
    }
    activeHirCleanupRegions_.resize(frame.cleanupDepth);
    frame.cleanupActive = false;
    state.sectionNode->children.push_back(std::move(state.sectionBody));
    state.node->children.push_back(std::move(state.sectionNode));
    state.sectionSyntax = nullptr;
    return nullptr;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceSwitchSimpleStatement(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind)
{
    if (statement->kind() == ast::AstNodeKind::BlockStatement)
    {
        current = statement;
        currentMemberOrdinal = frame.memberOrdinal;
        currentAddressContext = false;
        currentInputKind = IterativeInputKind::LoopBlock;
        return LoopBlockAdvance::Descend;
    }
    if (statement->kind() != ast::AstNodeKind::ExpressionStatement)
        return std::nullopt;
    IterativeExpressionFrame statementFrame;
    statementFrame.kind = IterativeExpressionKind::LoopBlockExpressionStatement;
    statementFrame.syntax = statement;
    statementFrame.memberOrdinal = frame.memberOrdinal;
    frames.push_back(std::move(statementFrame));
    current = statement->first();
    currentMemberOrdinal = frames.back().memberOrdinal;
    currentAddressContext = false;
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceSwitchDeclarationStatement(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error)
{
    std::optional<LoopBlockAdvance> advance = advanceSwitchVariableStatement(
        frame, statement, frames, current, currentMemberOrdinal,
        currentAddressContext, currentInputKind, error);
    if (!advance.has_value())
    {
        advance = advanceSwitchDeconstructionStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
    }
    if (!advance.has_value())
    {
        advance = advanceSwitchAssemblyStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
    }
    return advance;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceSwitchVariableStatement(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error)
{
    if (statement->kind() != ast::AstNodeKind::VariableDeclarationStatement)
        return std::nullopt;
    IterativeExpressionFrame variableFrame;
    variableFrame.kind = IterativeExpressionKind::VariableDeclaration;
    variableFrame.syntax = statement;
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
        frame.switchStatement->sectionBody->children.push_back(std::move(node));
        frame.stage = 1U;
        return LoopBlockAdvance::Complete;
    }
    const std::size_t childMemberOrdinal = variableFrame.memberOrdinal;
    frames.push_back(std::move(variableFrame));
    current = childSyntax;
    currentMemberOrdinal = childMemberOrdinal;
    currentAddressContext = childAddressContext;
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceSwitchDeconstructionStatement(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error)
{
    if (statement->kind() !=
        ast::AstNodeKind::DeconstructionDeclarationStatement)
        return std::nullopt;
    IterativeExpressionFrame declarationFrame;
    declarationFrame.kind = IterativeExpressionKind::DeconstructionDeclaration;
    declarationFrame.syntax = statement;
    declarationFrame.memberOrdinal = frame.memberOrdinal;
    if (!initializeDeconstructionDeclarationFrame(declarationFrame, error))
        return LoopBlockAdvance::Error;
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (!nextDeconstructionDeclarationChild(
            declarationFrame, childSyntax, childAddressContext))
    {
        auto node = finishDeconstructionDeclarationFrame(declarationFrame, error);
        if (node == nullptr) return LoopBlockAdvance::Error;
        frame.switchStatement->sectionBody->children.push_back(std::move(node));
        frame.stage = 1U;
        return LoopBlockAdvance::Complete;
    }
    const std::size_t childMemberOrdinal = declarationFrame.memberOrdinal;
    frames.push_back(std::move(declarationFrame));
    current = childSyntax;
    currentMemberOrdinal = childMemberOrdinal;
    currentAddressContext = childAddressContext;
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceSwitchAssemblyStatement(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error)
{
    if (statement->kind() != ast::AstNodeKind::InlineAssemblyStatement)
        return std::nullopt;
    IterativeExpressionFrame assemblyFrame;
    assemblyFrame.kind = IterativeExpressionKind::InlineAssemblyStatement;
    assemblyFrame.syntax = statement;
    assemblyFrame.memberOrdinal = frame.memberOrdinal;
    initializeInlineAssemblyFrame(assemblyFrame);
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    const bool hasChild = advanceInlineAssemblyFrame(
        assemblyFrame, childSyntax, childAddressContext, error);
    if (!error.empty()) return LoopBlockAdvance::Error;
    if (!hasChild)
    {
        frame.switchStatement->sectionBody->children.push_back(
            finishInlineAssemblyFrame(assemblyFrame));
        frame.stage = 1U;
        return LoopBlockAdvance::Complete;
    }
    const std::size_t childMemberOrdinal = assemblyFrame.memberOrdinal;
    frames.push_back(std::move(assemblyFrame));
    current = childSyntax;
    currentMemberOrdinal = childMemberOrdinal;
    currentAddressContext = childAddressContext;
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceSwitchControlStatement(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error)
{
    std::optional<LoopBlockAdvance> advance = advanceSwitchConditionalStatement(
        frame, statement, frames, current, currentMemberOrdinal,
        currentAddressContext, currentInputKind);
    if (!advance.has_value())
    {
        advance = advanceSwitchLoopStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
    }
    return advance;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceSwitchConditionalStatement(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind)
{
    IterativeExpressionFrame nestedFrame;
    if (statement->kind() == ast::AstNodeKind::StaticAssertStatement)
    {
        nestedFrame.kind = IterativeExpressionKind::StaticAssertStatement;
    }
    else if (statement->kind() == ast::AstNodeKind::IfStatement)
    {
        nestedFrame.kind = IterativeExpressionKind::IfStatement;
        nestedFrame.loopBlockNode = makeNode(
            ProjectOopHirNodeKind::If, statement, "builtin:void");
    }
    else
    {
        return std::nullopt;
    }
    nestedFrame.syntax = statement;
    nestedFrame.memberOrdinal = frame.memberOrdinal;
    frames.push_back(std::move(nestedFrame));
    current = statement->first();
    currentMemberOrdinal = frames.back().memberOrdinal;
    currentAddressContext = false;
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceSwitchLoopStatement(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error)
{
    IterativeExpressionFrame nestedFrame;
    if (statement->kind() == ast::AstNodeKind::WhileStatement ||
        statement->kind() == ast::AstNodeKind::DoWhileStatement)
    {
        nestedFrame.kind = IterativeExpressionKind::LoopStatement;
        nestedFrame.loopBlockNode = makeNode(
            statement->kind() == ast::AstNodeKind::DoWhileStatement
                ? ProjectOopHirNodeKind::DoWhile
                : ProjectOopHirNodeKind::While,
            statement,
            "builtin:void");
    }
    else if (statement->kind() == ast::AstNodeKind::SwitchStatement)
    {
        nestedFrame.memberOrdinal = frame.memberOrdinal;
        if (!initializeSwitchStatementFrame(nestedFrame, statement, error))
            return LoopBlockAdvance::Error;
    }
    else
    {
        return std::nullopt;
    }
    nestedFrame.syntax = statement;
    nestedFrame.memberOrdinal = frame.memberOrdinal;
    frames.push_back(std::move(nestedFrame));
    current = statement->first();
    currentMemberOrdinal = frames.back().memberOrdinal;
    currentAddressContext = false;
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceSwitchIterationStatement(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error)
{
    IterativeExpressionFrame nestedFrame;
    if (statement->kind() == ast::AstNodeKind::ForStatement)
    {
        nestedFrame.memberOrdinal = frame.memberOrdinal;
        initializeForStatementFrame(nestedFrame, statement);
        frames.push_back(std::move(nestedFrame));
        return advanceForStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, frames.size() - 1U);
    }
    if (statement->kind() != ast::AstNodeKind::ForeachStatement)
        return std::nullopt;
    nestedFrame.memberOrdinal = frame.memberOrdinal;
    if (!initializeForeachStatementFrame(nestedFrame, statement, error))
        return LoopBlockAdvance::Error;
    frames.push_back(std::move(nestedFrame));
    return advanceForeachStatementFrame(
        frames, current, currentMemberOrdinal, currentAddressContext,
        currentInputKind, error, frames.size() - 1U);
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceSwitchValueStatement(
    IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    std::vector<IterativeExpressionFrame>& frames,
    const ast::AstNode*& current,
    std::size_t& currentMemberOrdinal,
    bool& currentAddressContext,
    IterativeInputKind& currentInputKind,
    std::string& error)
{
    if (isExitValueStatement(statement))
    {
        return beginExitValueStatement(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, statement, frame.memberOrdinal)
            ? LoopBlockAdvance::Descend
            : LoopBlockAdvance::Error;
    }
    if (isCoroutineValueStatement(statement))
    {
        beginCoroutineValueStatement(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, statement, frame.memberOrdinal);
        return LoopBlockAdvance::Descend;
    }
    if (statement->kind() == ast::AstNodeKind::UsingStatement)
    {
        return beginUsingStatement(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, statement, frame.memberOrdinal)
            ? LoopBlockAdvance::Descend
            : LoopBlockAdvance::Error;
    }
    if (statement->kind() != ast::AstNodeKind::TryStatement)
        return std::nullopt;
    beginTryStatement(
        frames, current, currentMemberOrdinal, currentAddressContext,
        currentInputKind, statement, frame.memberOrdinal);
    return LoopBlockAdvance::Descend;
}
}
