#pragma once

namespace nativesharp::frontend
{
class ProjectCallableAnalysisResult;
class ProjectMethodBodyModel;

class ProjectCallableResultMerger final
{
public:
    static void merge(
        ProjectMethodBodyModel& destination,
        ProjectCallableAnalysisResult&& result);
};
}
