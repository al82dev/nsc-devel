#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <utility>

namespace nativesharp::frontend
{
using internal::IterativeExpressionFrame;
using internal::IterativeExpressionKind;
using internal::IterativeInputKind;
using internal::LoopBlockAdvance;

bool ProjectOopHirModel::beginMatchArm(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] std::string& error)
{
    const ast::AstNode* armSyntax = frame.syntax->children()[frame.armIndex];
    if (armSyntax == nullptr || armSyntax->first() == nullptr ||
        armSyntax->third() == nullptr)
    {
        error = "T03-F03 malformed match expression arm";
        return false;
    }
    frame.matchArm = makeNode(
        ProjectOopHirNodeKind::MatchArm,
        armSyntax,
        frame.matchNode->typeKey);
    if (frame.armIndex < frame.matchBinding->armTags.size())
        frame.matchArm->switchLabels = frame.matchBinding->armTags[frame.armIndex];
    frame.stage = 1U;
    current = armSyntax->first();
    currentMemberOrdinal = frame.memberOrdinal;
    currentAddressContext = false;
    return true;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceMatchVariableDeclaration(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* declaration,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] std::string& error)
{
    if (declaration == nullptr || declaration->kind() !=
        ast::AstNodeKind::VariableDeclarationStatement)
    {
        return std::nullopt;
    }
    IterativeExpressionFrame variableFrame;
    variableFrame.kind = IterativeExpressionKind::VariableDeclaration;
    variableFrame.syntax = declaration;
    variableFrame.memberOrdinal = frame.memberOrdinal;
    if (!initializeVariableDeclarationFrame(variableFrame, error))
        return LoopBlockAdvance::Error;
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (!nextVariableDeclarationChild(
            variableFrame, childSyntax, childAddressContext))
    {
        auto declarationNode = finishVariableDeclarationFrame(variableFrame);
        activateLocalCleanup(declarationNode);
        frame.matchArm->children.push_back(std::move(declarationNode));
        return LoopBlockAdvance::Complete;
    }
    currentMemberOrdinal = variableFrame.memberOrdinal;
    frame.stage = 3U;
    frames.push_back(std::move(variableFrame));
    current = childSyntax;
    currentAddressContext = childAddressContext;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceMatchDeconstructionDeclaration(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* declaration,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] std::string& error)
{
    if (declaration == nullptr || declaration->kind() !=
        ast::AstNodeKind::DeconstructionDeclarationStatement)
    {
        return std::nullopt;
    }
    IterativeExpressionFrame declarationFrame;
    declarationFrame.kind = IterativeExpressionKind::DeconstructionDeclaration;
    declarationFrame.syntax = declaration;
    declarationFrame.memberOrdinal = frame.memberOrdinal;
    if (!initializeDeconstructionDeclarationFrame(declarationFrame, error))
        return LoopBlockAdvance::Error;
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    if (!nextDeconstructionDeclarationChild(
            declarationFrame, childSyntax, childAddressContext))
    {
        auto declarationNode = finishDeconstructionDeclarationFrame(
            declarationFrame, error);
        if (declarationNode == nullptr) return LoopBlockAdvance::Error;
        frame.matchArm->children.push_back(std::move(declarationNode));
        return LoopBlockAdvance::Complete;
    }
    currentMemberOrdinal = declarationFrame.memberOrdinal;
    frame.stage = 3U;
    frames.push_back(std::move(declarationFrame));
    current = childSyntax;
    currentAddressContext = childAddressContext;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceMatchInlineAssembly(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* declaration,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] std::string& error)
{
    if (declaration == nullptr ||
        declaration->kind() != ast::AstNodeKind::InlineAssemblyStatement)
    {
        return std::nullopt;
    }
    IterativeExpressionFrame assemblyFrame;
    assemblyFrame.kind = IterativeExpressionKind::InlineAssemblyStatement;
    assemblyFrame.syntax = declaration;
    assemblyFrame.memberOrdinal = frame.memberOrdinal;
    initializeInlineAssemblyFrame(assemblyFrame);
    const ast::AstNode* childSyntax = nullptr;
    bool childAddressContext = false;
    const bool hasChild = advanceInlineAssemblyFrame(
        assemblyFrame, childSyntax, childAddressContext, error);
    if (!error.empty()) return LoopBlockAdvance::Error;
    if (!hasChild)
    {
        frame.matchArm->children.push_back(
            finishInlineAssemblyFrame(assemblyFrame));
        return LoopBlockAdvance::Complete;
    }
    currentMemberOrdinal = assemblyFrame.memberOrdinal;
    frame.stage = 3U;
    frames.push_back(std::move(assemblyFrame));
    current = childSyntax;
    currentAddressContext = childAddressContext;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceMatchConditionalOrLoop(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* declaration,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext)
{
    if (declaration != nullptr &&
        declaration->kind() == ast::AstNodeKind::IfStatement)
    {
        IterativeExpressionFrame ifFrame;
        ifFrame.kind = IterativeExpressionKind::IfStatement;
        ifFrame.syntax = declaration;
        ifFrame.memberOrdinal = frame.memberOrdinal;
        ifFrame.loopBlockNode = makeNode(
            ProjectOopHirNodeKind::If, declaration, "builtin:void");
        currentMemberOrdinal = ifFrame.memberOrdinal;
        frame.stage = 3U;
        frames.push_back(std::move(ifFrame));
        current = declaration->first();
        currentAddressContext = false;
        return LoopBlockAdvance::Descend;
    }
    if (declaration == nullptr ||
        (declaration->kind() != ast::AstNodeKind::WhileStatement &&
         declaration->kind() != ast::AstNodeKind::DoWhileStatement))
    {
        return std::nullopt;
    }
    IterativeExpressionFrame loopFrame;
    loopFrame.kind = IterativeExpressionKind::LoopStatement;
    loopFrame.syntax = declaration;
    loopFrame.memberOrdinal = frame.memberOrdinal;
    loopFrame.loopBlockNode = makeNode(
        declaration->kind() == ast::AstNodeKind::DoWhileStatement
            ? ProjectOopHirNodeKind::DoWhile
            : ProjectOopHirNodeKind::While,
        declaration,
        "builtin:void");
    currentMemberOrdinal = loopFrame.memberOrdinal;
    frame.stage = 3U;
    frames.push_back(std::move(loopFrame));
    current = declaration->first();
    currentAddressContext = false;
    return LoopBlockAdvance::Descend;
}

std::optional<LoopBlockAdvance>
ProjectOopHirModel::advanceMatchSwitchOrIteration(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* declaration,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    if (declaration != nullptr &&
        declaration->kind() == ast::AstNodeKind::SwitchStatement)
    {
        IterativeExpressionFrame switchFrame;
        switchFrame.memberOrdinal = frame.memberOrdinal;
        if (!initializeSwitchStatementFrame(switchFrame, declaration, error))
            return LoopBlockAdvance::Error;
        currentMemberOrdinal = switchFrame.memberOrdinal;
        frame.stage = 3U;
        frames.push_back(std::move(switchFrame));
        current = declaration->first();
        currentAddressContext = false;
        return LoopBlockAdvance::Descend;
    }
    if (declaration != nullptr &&
        declaration->kind() == ast::AstNodeKind::ForStatement)
    {
        IterativeExpressionFrame forFrame;
        forFrame.memberOrdinal = frame.memberOrdinal;
        initializeForStatementFrame(forFrame, declaration);
        frame.stage = 3U;
        frames.push_back(std::move(forFrame));
        const LoopBlockAdvance advance = advanceForStatementFrame(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, frames.size() - 1U);
        return advance == LoopBlockAdvance::Descend
            ? advance
            : LoopBlockAdvance::Error;
    }
    if (declaration == nullptr ||
        declaration->kind() != ast::AstNodeKind::ForeachStatement)
    {
        return std::nullopt;
    }
    IterativeExpressionFrame foreachFrame;
    foreachFrame.memberOrdinal = frame.memberOrdinal;
    if (!initializeForeachStatementFrame(foreachFrame, declaration, error))
        return LoopBlockAdvance::Error;
    frame.stage = 3U;
    frames.push_back(std::move(foreachFrame));
    const LoopBlockAdvance advance = advanceForeachStatementFrame(
        frames, current, currentMemberOrdinal, currentAddressContext,
        currentInputKind, error, frames.size() - 1U);
    return advance == LoopBlockAdvance::Descend
        ? advance
        : LoopBlockAdvance::Error;
}

std::optional<LoopBlockAdvance> ProjectOopHirModel::advanceMatchValueStatement(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    const ast::AstNode* declaration,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    if (isExitValueStatement(declaration))
    {
        frame.stage = 3U;
        return beginExitValueStatement(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, declaration, frame.memberOrdinal)
            ? LoopBlockAdvance::Descend
            : LoopBlockAdvance::Error;
    }
    if (isCoroutineValueStatement(declaration))
    {
        frame.stage = 3U;
        beginCoroutineValueStatement(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, declaration, frame.memberOrdinal);
        return LoopBlockAdvance::Descend;
    }
    if (declaration != nullptr &&
        declaration->kind() == ast::AstNodeKind::UsingStatement)
    {
        frame.stage = 3U;
        return beginUsingStatement(
            frames, current, currentMemberOrdinal, currentAddressContext,
            currentInputKind, error, declaration, frame.memberOrdinal)
            ? LoopBlockAdvance::Descend
            : LoopBlockAdvance::Error;
    }
    if (declaration == nullptr ||
        declaration->kind() != ast::AstNodeKind::TryStatement)
    {
        return std::nullopt;
    }
    frame.stage = 3U;
    beginTryStatement(
        frames, current, currentMemberOrdinal, currentAddressContext,
        currentInputKind, declaration, frame.memberOrdinal);
    return LoopBlockAdvance::Descend;
}

bool ProjectOopHirModel::advanceMatchDeclarations(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    const ast::AstNode* armSyntax = frame.syntax->children()[frame.armIndex];
    while (frame.syntheticLocalOrdinal < armSyntax->children().size())
    {
        const ast::AstNode* declaration =
            armSyntax->children()[frame.syntheticLocalOrdinal++];
        std::optional<LoopBlockAdvance> advance =
            advanceMatchVariableDeclaration(
                frame, declaration, frames, current, currentMemberOrdinal,
                currentAddressContext, error);
        if (!advance.has_value())
            advance = advanceMatchDeconstructionDeclaration(
                frame, declaration, frames, current, currentMemberOrdinal,
                currentAddressContext, error);
        if (!advance.has_value())
            advance = advanceMatchInlineAssembly(
                frame, declaration, frames, current, currentMemberOrdinal,
                currentAddressContext, error);
        if (!advance.has_value())
            advance = advanceMatchConditionalOrLoop(
                frame, declaration, frames, current, currentMemberOrdinal,
                currentAddressContext);
        if (!advance.has_value())
            advance = advanceMatchSwitchOrIteration(
                frame, declaration, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, error);
        if (!advance.has_value())
            advance = advanceMatchValueStatement(
                frame, declaration, frames, current, currentMemberOrdinal,
                currentAddressContext, currentInputKind, error);
        if (advance.has_value())
        {
            if (*advance == LoopBlockAdvance::Descend) return true;
            if (*advance == LoopBlockAdvance::Error) return false;
            continue;
        }
        auto declarationNode = lowerStatement(
            declaration, frame.memberOrdinal, error);
        if (declarationNode == nullptr) return false;
        activateLocalCleanup(declarationNode);
        frame.matchArm->children.push_back(std::move(declarationNode));
    }
    frame.stage = 4U;
    current = armSyntax->third();
    currentMemberOrdinal = frame.memberOrdinal;
    currentAddressContext = frame.matchBinding->referenceResult;
    return true;
}

bool ProjectOopHirModel::beginMatchValue(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] std::vector<IterativeExpressionFrame>& frames,
    [[maybe_unused]] const ast::AstNode*& current,
    [[maybe_unused]] std::size_t& currentMemberOrdinal,
    [[maybe_unused]] bool& currentAddressContext,
    [[maybe_unused]] IterativeInputKind& currentInputKind,
    [[maybe_unused]] std::string& error)
{
    frame.cleanupDepth = activeHirCleanupRegions_.size();
    frame.cleanupActive = true;
    frame.syntheticLocalOrdinal = 0U;
    if (advanceMatchDeclarations(
            frame, frames, current, currentMemberOrdinal,
            currentAddressContext, currentInputKind, error))
    {
        return true;
    }
    activeHirCleanupRegions_.resize(frame.cleanupDepth);
    frame.cleanupActive = false;
    return false;
}

void ProjectOopHirModel::finishMatch(
    [[maybe_unused]] IterativeExpressionFrame& frame,
    [[maybe_unused]] std::unique_ptr<ProjectOopHirNode>& lowered)
{
    lowered = std::move(frame.matchNode);
    if (!frame.matchBinding->referenceResult || frame.addressContext) return;
    auto load = makeNode(
        ProjectOopHirNodeKind::PointerLoad,
        frame.syntax,
        lowered->typeKey);
    load->children.push_back(std::move(lowered));
    lowered = std::move(load);
}
}
