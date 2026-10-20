#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <utility>

namespace nativesharp::frontend
{
using internal::IterativeExpressionFrame;
using internal::IterativeExpressionKind;
using internal::IterativeInputKind;
using internal::LoopBlockAdvance;

LoopBlockAdvance ProjectOopHirModel::advanceLoopBlockFrame(
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    std::size_t frameIndex)
{
    while (true)
    {
        IterativeExpressionFrame& frame = frames[frameIndex];
        if (frame.armIndex >= frame.syntax->children().size())
        {
            activeHirCleanupRegions_.resize(frame.cleanupDepth);
            lowered = std::move(frame.loopBlockNode);
            frames.pop_back();
            return LoopBlockAdvance::Complete;
        }

        const ast::AstNode* statement = frame.syntax->children()[frame.armIndex++];
        std::optional<LoopBlockAdvance> advance = advanceLoopBlockSimpleStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind);
        if (advance.has_value()) return *advance;
        advance = advanceLoopBlockDeclarationStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
        if (advance.has_value())
        {
            if (*advance == LoopBlockAdvance::Complete) continue;
            return *advance;
        }
        advance = advanceLoopBlockControlStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
        if (advance.has_value()) return *advance;
        advance = advanceLoopBlockIterationStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, lowered, error, frameIndex);
        if (advance.has_value())
        {
            if (*advance == LoopBlockAdvance::Complete) continue;
            return *advance;
        }
        advance = advanceLoopBlockValueStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
        if (advance.has_value()) return *advance;
        std::unique_ptr<ProjectOopHirNode> statementNode =
            lowerStatement(statement, frame.memberOrdinal, error);
        if (statementNode == nullptr) return LoopBlockAdvance::Error;
        activateLocalCleanup(statementNode);
        frame.loopBlockNode->children.push_back(std::move(statementNode));
    }
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceLoopBlockSimpleStatement(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind)
{
    if (statement != nullptr &&
        statement->kind() == ast::AstNodeKind::BlockStatement)
    {
        current = statement;
        currentMemberOrdinal = frame.memberOrdinal;
        currentAddressContext = false;
        currentInputKind = IterativeInputKind::LoopBlock;
        return LoopBlockAdvance::Descend;
    }
    if (statement == nullptr ||
        statement->kind() != ast::AstNodeKind::ExpressionStatement)
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
ProjectOopHirModel::advanceLoopBlockDeclarationStatement(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    std::optional<LoopBlockAdvance> advance = advanceLoopBlockVariableStatement(
        frame, statement, frames, current, currentMemberOrdinal,
        currentAddressContext, currentInputKind, error);
    if (!advance.has_value())
    {
        advance = advanceLoopBlockDeconstructionStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
    }
    if (!advance.has_value())
    {
        advance = advanceLoopBlockAssemblyStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
    }
    return advance;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceLoopBlockVariableStatement(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    if (statement == nullptr ||
        statement->kind() != ast::AstNodeKind::VariableDeclarationStatement)
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
        auto variableNode = finishVariableDeclarationFrame(variableFrame);
        activateLocalCleanup(variableNode);
        frame.loopBlockNode->children.push_back(std::move(variableNode));
        return LoopBlockAdvance::Complete;
    }
    const std::size_t variableMemberOrdinal = variableFrame.memberOrdinal;
    frames.push_back(std::move(variableFrame));
    current = childSyntax;
    currentMemberOrdinal = variableMemberOrdinal;
    currentAddressContext = childAddressContext;
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceLoopBlockDeconstructionStatement(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    if (statement == nullptr || statement->kind() !=
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
        auto declarationNode =
            finishDeconstructionDeclarationFrame(declarationFrame, error);
        if (declarationNode == nullptr) return LoopBlockAdvance::Error;
        frame.loopBlockNode->children.push_back(std::move(declarationNode));
        return LoopBlockAdvance::Complete;
    }
    const std::size_t declarationMemberOrdinal = declarationFrame.memberOrdinal;
    frames.push_back(std::move(declarationFrame));
    current = childSyntax;
    currentMemberOrdinal = declarationMemberOrdinal;
    currentAddressContext = childAddressContext;
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceLoopBlockAssemblyStatement(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    if (statement == nullptr ||
        statement->kind() != ast::AstNodeKind::InlineAssemblyStatement)
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
        frame.loopBlockNode->children.push_back(
            finishInlineAssemblyFrame(assemblyFrame));
        return LoopBlockAdvance::Complete;
    }
    const std::size_t assemblyMemberOrdinal = assemblyFrame.memberOrdinal;
    frames.push_back(std::move(assemblyFrame));
    current = childSyntax;
    currentMemberOrdinal = assemblyMemberOrdinal;
    currentAddressContext = childAddressContext;
    currentInputKind = IterativeInputKind::Expression;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceLoopBlockControlStatement(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    std::optional<LoopBlockAdvance> advance =
        advanceLoopBlockConditionalStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind);
    if (!advance.has_value())
    {
        advance = advanceLoopBlockLoopStatement(
            frame, statement, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error);
    }
    return advance;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceLoopBlockConditionalStatement(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind)
{
    if (statement == nullptr) return std::nullopt;
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

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceLoopBlockLoopStatement(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    if (statement == nullptr) return std::nullopt;
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
ProjectOopHirModel::advanceLoopBlockIterationStatement(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered,
    [[maybe_unused]] std::string& error,
    std::size_t frameIndex)
{
    if (statement == nullptr) return std::nullopt;
    LoopBlockAdvance advance = LoopBlockAdvance::Complete;
    if (statement->kind() == ast::AstNodeKind::ForStatement)
    {
        IterativeExpressionFrame nestedFrame;
        nestedFrame.memberOrdinal = frame.memberOrdinal;
        initializeForStatementFrame(nestedFrame, statement);
        frames.push_back(std::move(nestedFrame));
        advance = advanceForStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, frames.size() - 1U);
    }
    else if (statement->kind() == ast::AstNodeKind::ForeachStatement)
    {
        IterativeExpressionFrame nestedFrame;
        nestedFrame.memberOrdinal = frame.memberOrdinal;
        if (!initializeForeachStatementFrame(nestedFrame, statement, error))
            return LoopBlockAdvance::Error;
        frames.push_back(std::move(nestedFrame));
        advance = advanceForeachStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, frames.size() - 1U);
    }
    else
    {
        return std::nullopt;
    }
    if (advance != LoopBlockAdvance::Complete) return advance;
    IterativeExpressionFrame& parent = frames[frameIndex];
    activateLocalCleanup(lowered);
    parent.loopBlockNode->children.push_back(std::move(lowered));
    return LoopBlockAdvance::Complete;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceLoopBlockValueStatement(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* statement,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
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
    if (statement != nullptr &&
        statement->kind() == ast::AstNodeKind::UsingStatement)
    {
        return beginUsingStatement(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, statement, frame.memberOrdinal)
            ? LoopBlockAdvance::Descend
            : LoopBlockAdvance::Error;
    }
    if (statement == nullptr ||
        statement->kind() != ast::AstNodeKind::TryStatement)
        return std::nullopt;
    beginTryStatement(
        frames, current, currentMemberOrdinal, currentAddressContext,
        currentInputKind, statement, frame.memberOrdinal);
    return LoopBlockAdvance::Descend;
}
}
