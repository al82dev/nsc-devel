#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCostExplorer.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include "../support/TestContext.h"

#include <memory>
#include <sstream>
#include <string_view>
#include <vector>

namespace
{
std::unique_ptr<nativesharp::frontend::ProjectOopMirNode> node(
    nativesharp::frontend::ProjectOopMirNodeKind kind)
{
    auto result = std::make_unique<nativesharp::frontend::ProjectOopMirNode>();
    result->kind = kind;
    return result;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::frontend;
    using namespace nativesharp::llvm_backend;

    const std::string optimization = llvmOptimizationContractManifest();
    tests.expect(optimization.find("NSOPTIMIZATION 1.0") != std::string::npos &&
            optimization.find("cache-key=pipeline-name-and-version") != std::string::npos &&
            optimization.find("O0 pipeline=default<O0> version=1 intent=fast-debug") != std::string::npos &&
            optimization.find("O3 pipeline=default<O3> version=1 intent=aggressive-budgeted") != std::string::npos,
        "O0 through O3 publish one versioned optimization and cache contract");

    ProjectOopMirFunction function;
    function.debugDisplayName = "Costs.Sample";
    function.genericSpecializationKey = "Sample<int>";
    function.body = node(ProjectOopMirNodeKind::Block);
    auto stack = node(ProjectOopMirNodeKind::StackAllocation);
    stack->immediate = 32;
    function.body->children.push_back(std::move(stack));
    function.body->children.push_back(node(ProjectOopMirNodeKind::AllocateAndConstruct));
    function.body->children.push_back(node(ProjectOopMirNodeKind::MoveValue));
    function.body->children.push_back(node(ProjectOopMirNodeKind::InterfaceInstanceCall));
    function.body->children.push_back(node(ProjectOopMirNodeKind::CoreMutexLock));
    function.body->children.push_back(node(ProjectOopMirNodeKind::CoreAtomicLoadInt32));
    function.body->children.push_back(node(ProjectOopMirNodeKind::Throw));
    ProjectOopMirCleanupRegion cleanup;
    cleanup.ordinal = 7;
    cleanup.automaticDestruction = true;
    function.cleanupRegions.push_back(cleanup);
    function.exceptionFlow.requiresPersonality = true;
    std::vector<ProjectOopMirFunction> functions;
    functions.push_back(std::move(function));

    const ProjectCostReport report = exploreProjectCosts(functions);
    tests.expect(report.functions.size() == 1 &&
            report.functions[0].staticStackBytesLowerBound == 32 &&
            report.functions[0].heapAllocationSites == 1 &&
            report.functions[0].moveSites == 1 && report.functions[0].dropSites == 1,
        "cost explorer derives stack, heap, move and cleanup costs from MIR");
    tests.expect(report.functions[0].dynamicDispatchSites == 1 &&
            report.functions[0].lockSites == 1 && report.functions[0].atomicSites == 1 &&
            report.functions[0].unwindSites == 2 && report.functions[0].monomorphizations == 1,
        "cost explorer exposes dispatch, synchronization, unwind and specialization costs");
    const std::string text = projectCostReportText(report);
    const std::string json = projectCostReportJson(report);
    tests.expect(text.find("NSCOST 1.0") != std::string::npos &&
            text.find("cause: heap <- allocate-and-construct") != std::string::npos &&
            text.find("totals heap-sites=1") != std::string::npos,
        "text cost report includes deterministic causal chains and totals");
    tests.expect(json.find("\"schema\":\"NSCOST 1.0\"") != std::string::npos &&
            json.find("\"dynamicDispatchSites\":1") != std::string::npos &&
            json.find("\"function\":\"Costs.Sample\"") != std::string::npos,
        "JSON cost report carries the same machine-readable facts");

    nativesharp::driver::Driver driver;
    std::ostringstream output;
    std::ostringstream error;
    constexpr std::string_view explainOptimization[] = {"explain", "optimization"};
    tests.expect(driver.run(explainOptimization, output, error) == nativesharp::ExitCode::Success &&
            output.str() == optimization && error.str().empty(),
        "nsc explain optimization routes to the versioned productive contract");
    output.str({});
    output.clear();
    error.str({});
    error.clear();
    constexpr std::string_view explainSession[] = {
        "explain", "session", "--output-format=json"};
    tests.expect(driver.run(explainSession, output, error) == nativesharp::ExitCode::Success &&
            output.str().find("\"status\":\"partial\"") != std::string::npos &&
            output.str().find("persistent isolated compiler server") != std::string::npos,
        "nsc explain reports session evidence and the persistent-server gap honestly");
    output.str({});
    output.clear();
    constexpr std::string_view explainAllocator[] = {"explain", "debug-allocator"};
    tests.expect(driver.run(explainAllocator, output, error) == nativesharp::ExitCode::Success &&
            output.str().find("status=missing") != std::string::npos &&
            output.str().find("leak, UAF and double-drop") != std::string::npos,
        "nsc explain does not claim the debug allocator from MIR visibility alone");
    return tests.result();
}
