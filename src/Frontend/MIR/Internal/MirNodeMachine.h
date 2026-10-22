#pragma once

#include "nativesharp/Frontend/ProjectOopMirModel.h"

namespace nativesharp::frontend
{
class ProjectOopMirModel::MirNodeMachine final
{
public:
    explicit MirNodeMachine(ProjectOopMirModel& model) noexcept
        : model_(model), structLayouts_(model.structLayouts_)
    {
    }
    MirNodeMachine(
        ProjectOopMirModel& model,
        std::span<const ProjectOopMirStructLayout> structLayouts) noexcept
        : model_(model), structLayouts_(structLayouts)
    {
    }

    std::unique_ptr<ProjectOopMirNode> lower(
        const ProjectOopHirNode* node,
        std::string& error);

private:
    void copyIdentity(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result);
    void copyControlState(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result);
    void copyAttributes(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result);
    bool lowerKind(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerKindGroups01To12(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerKindGroups13To24(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerInlineAssembly(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result);
    int lowerMatchExpression(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerResultPropagation(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerRawElementAddress(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerRawStorage(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerUtf8StringLiteral(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result);
    int lowerAggregateOperation(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup07FirstHalf(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result);
    int lowerGroup08FirstHalf(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result);
    int lowerGroup09FirstHalf(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result);
    int lowerMonitorWait(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup10FirstHalf(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result);
    int lowerGroup11FirstHalf(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result);
    int lowerNullConditionalArrayStore(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerRangeSlice(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerCompoundStore(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerAwaitFuture(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    const ProjectOopMirStructLayout* structLayoutForTypeKey(
        std::string_view typeKey) const noexcept;
    int lowerGroup01(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup02(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup03(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup04(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup05(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup06(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup07(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup08(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup09(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup10(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup11(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup12(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup13(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup14(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup15(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup16(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup17(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup18(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup19(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup20(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup21(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup22(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup23(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    int lowerGroup24(
        const ProjectOopHirNode* node,
        ProjectOopMirNode* result,
        std::string& error);
    ProjectOopMirModel& model_;
    std::span<const ProjectOopMirStructLayout> structLayouts_;
};
}
