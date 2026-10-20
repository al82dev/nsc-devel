#include "../HirTuplePatternFrame.h"
#include "HirExpressionStatementMachineTypes.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"

#include <utility>

namespace nativesharp::frontend
{
using internal::IterativeExpressionFrame;

bool ProjectOopHirModel::expandDeconstructionAssignmentTargetTuple(
    std::string& error,
    const ast::AstNode* targetSyntax,
    const std::string& sourceTypeKey,
    std::uint64_t byteOffset,
    std::vector<HirTuplePatternFrame>& children)
{
    const auto nestedElements =
        ProjectTypeConversion::tupleElementTypeKeys(sourceTypeKey);
    const auto nestedLayout = objectLayoutModel_->tupleLayoutForTypeKey(
        *nameLookup_, sourceTypeKey);
    if (!nestedElements.has_value() || !nestedLayout.has_value() ||
        nestedElements->size() != targetSyntax->children().size() ||
        nestedLayout->offsets.size() != nestedElements->size())
    {
        error = "T02-F03 nested assignment target lost its tuple layout";
        return false;
    }
    children.reserve(nestedElements->size());
    for (std::size_t index = 0; index < nestedElements->size(); ++index)
    {
        children.push_back(HirTuplePatternFrame{
            targetSyntax->children()[index],
            (*nestedElements)[index],
            byteOffset + nestedLayout->offsets[index]});
    }
    return true;
}

bool ProjectOopHirModel::collectDeconstructionAssignmentTargetLeaf(
    IterativeExpressionFrame& frame,
    const ast::AstNode* targetSyntax,
    const std::string& sourceTypeKey,
    std::uint64_t byteOffset)
{
    if (targetSyntax->kind() == ast::AstNodeKind::IdentifierExpression &&
        targetSyntax->text() == "_")
    {
        return true;
    }
    frame.deconstructionLeaves.push_back(
        HirTuplePatternFrame{targetSyntax, sourceTypeKey, byteOffset});
    return true;
}

void ProjectOopHirModel::materializeSetterReceiver(
    IterativeExpressionFrame& frame,
    std::unique_ptr<ProjectOopHirNode>& lowered)
{
    frame.first = std::move(lowered);
    if (!frame.materializeReceiverTemporary) return;
    auto temporary = makeNode(
        ProjectOopHirNodeKind::TemporaryAddress,
        frame.callTarget.receiverSyntax,
        std::string(frame.callTarget.receiverTypeKey));
    temporary->elementTypeKey = frame.callTarget.receiverTypeKey;
    temporary->children.push_back(std::move(frame.first));
    frame.first = std::move(temporary);
}
}
