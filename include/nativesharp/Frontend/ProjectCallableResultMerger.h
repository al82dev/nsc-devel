#pragma once

#include <cstddef>

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

private:
    static void mergeScopeNames(
        ProjectMethodBodyModel& destination,
        ProjectMethodBodyModel& source,
        std::size_t localBase);
    static void appendShardCollections(
        ProjectMethodBodyModel& destination,
        ProjectMethodBodyModel& source,
        std::size_t scopeBase,
        std::size_t localBase,
        std::size_t cleanupBase,
        std::size_t lambdaBase,
        std::size_t localBorrowBase);
    static void mergeShardState(
        ProjectMethodBodyModel& destination,
        ProjectMethodBodyModel& source,
        ProjectCallableAnalysisResult& result);
    static void indexMergedBindings(
        ProjectMethodBodyModel& destination,
        std::size_t nameBase,
        std::size_t expressionBase,
        std::size_t constructionBase,
        std::size_t callBase);
    static void indexMergedSyntaxBindings(
        ProjectMethodBodyModel& destination,
        std::size_t aggregateBase,
        std::size_t deconstructionBase,
        std::size_t matchBase,
        std::size_t loopExpressionBase,
        std::size_t loopBreakBase,
        std::size_t namespaceCallBase);
};
}
