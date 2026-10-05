#include "nativesharp/Language/CoroutineSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;

    const TaskExecutorProtocol customExecutor{
        "GameJobExecutor", true, true, true, true, true};
    const std::array children = {
        ScopedTask{"physics", true, true, true},
        ScopedTask{"render", false, false, true},
        ScopedTask{"audio", false, false, true}};
    const auto scope = buildTaskScopePlan(
        customExecutor, children, 2, true, error);
    tests.expect(scope.has_value() && scope->executorType == "GameJobExecutor" &&
            scope->nestingDepth == 2 && scope->noHeap && !scope->fixesThreadPool,
        "nested task scopes use a custom executor without fixing a runtime thread pool");
    tests.expect(scope.has_value() && scope->children[0].borrowsFromScope,
        "a child may borrow scope storage only when its join and lifetime are proven");

    auto escaping = children;
    escaping[0].joinedBeforeScopeExit = false;
    tests.expect(!buildTaskScopePlan(
            customExecutor, escaping, 0, true, error).has_value() &&
            error.find("joined before scope exit") != std::string::npos,
        "a borrowed child cannot escape its task scope");

    const std::array<std::string, 1> completed = {"physics"};
    const auto earlyError = scope.has_value()
        ? planTaskScopeExit(*scope, completed, TaskScopeExitReason::EarlyError)
        : std::vector<TaskScopeAction>{};
    tests.expect(earlyError.size() == 5 &&
            earlyError[0].kind == TaskScopeActionKind::Cancel && earlyError[0].child == "audio" &&
            earlyError[1].child == "render" &&
            earlyError[2].kind == TaskScopeActionKind::Join && earlyError[4].kind == TaskScopeActionKind::PropagateError,
        "early errors cancel outstanding children, join them, then propagate the error");

    const auto cancellation = scope.has_value()
        ? planTaskScopeExit(*scope, {}, TaskScopeExitReason::Cancellation)
        : std::vector<TaskScopeAction>{};
    tests.expect(cancellation.size() == 7 &&
            cancellation.front().kind == TaskScopeActionKind::Cancel &&
            cancellation.back().kind == TaskScopeActionKind::PropagateCancellation,
        "parent cancellation is cooperative and cannot bypass child joins");

    const auto panic = scope.has_value()
        ? planTaskScopeExit(*scope, completed, TaskScopeExitReason::Panic)
        : std::vector<TaskScopeAction>{};
    tests.expect(!panic.empty() && panic.back().kind == TaskScopeActionKind::PropagatePanic,
        "panic propagation happens only after outstanding children are cancelled and joined");

    TaskExecutorProtocol heapOnly = customExecutor;
    heapOnly.supportsNoHeap = false;
    tests.expect(!buildTaskScopePlan(heapOnly, children, 0, true, error).has_value() &&
            error.find("noheap") != std::string::npos,
        "a noheap scope rejects an executor that cannot honor the allocation contract");

    TaskExecutorProtocol incomplete = customExecutor;
    incomplete.hasCancel = false;
    tests.expect(!buildTaskScopePlan(incomplete, children, 0, false, error).has_value() &&
            error.find("spawn, poll/join, and cancel") != std::string::npos,
        "executor customization is protocol-based and fails closed when cancellation is absent");
    return tests.result();
}
