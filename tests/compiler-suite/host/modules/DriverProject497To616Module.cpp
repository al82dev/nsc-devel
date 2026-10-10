#include "DriverTestSupport.h"

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/ProjectCommands.h"

#include <sstream>

namespace nsc::suite
{
namespace
{
using namespace driver_support;
using namespace nativesharp::driver;

void explainContract(TestContext& context, std::size_t variant)
{
    const std::string_view topic = variant == 3U ? "unknown-topic" :
        variant == 4U ? "optimization" : variant == 5U ? "debug-allocator" : "build";
    std::vector<std::string_view> arguments{"explain", topic};
    if (variant == 6U) arguments.push_back("--output-format=json");
    std::ostringstream output;
    std::ostringstream error;
    std::vector<std::string> delegated;
    const auto executor = [&delegated](std::span<const std::string_view> nested,
        std::ostream& nestedOutput, std::ostream&) -> nativesharp::ExitCode
    {
        for (const auto argument : nested) delegated.emplace_back(argument);
        nestedOutput << "delegated\n";
        return nativesharp::ExitCode::Success;
    };
    const nativesharp::ExitCode result = runProjectCommand(arguments, output, error, executor);
    if (variant == 3U)
    {
        context.expect(result == nativesharp::ExitCode::CommandLineError &&
                contains(error.str(), "unknown explain topic"),
            "unknown explain topic is rejected with the canonical topic inventory");
        return;
    }
    if (variant == 4U)
    {
        context.expect(result == nativesharp::ExitCode::Success && delegated ==
                std::vector<std::string>({"--print-optimization-contract"}),
            "optimization explanation delegates to the versioned compiler contract");
        return;
    }
    const bool json = variant == 6U;
    context.expect(result == nativesharp::ExitCode::Success &&
            contains(output.str(), json ? "\"schema\":\"NSEXPLAIN 1.0\"" : "NSEXPLAIN 1.0") &&
            contains(output.str(), topic) && isProjectCommand("explain") && !isProjectCommand("unknown"),
        "explain dispatch exposes exact text/JSON schema and project-command membership");
}
}

void registerDriverProject497To616Module(TestRegistry& registry)
{
    addInfrastructureContract(registry, 63U, "unit",
        "CF-DRIVER-063 JSON parser is private to ProjectCommands.cpp and has no isolated public seam");
    addInfrastructureContract(registry, 64U, "unit",
        "CF-DRIVER-064 sidecar model and renderer are private to ProjectCommands.cpp");
    addInfrastructureContract(registry, 65U, "integration",
        "CF-DRIVER-065 requires template roots and transactional filesystem fault injection");
    addInfrastructureContract(registry, 66U, "integration",
        "CF-DRIVER-066 requires controlled current-directory and multi-project inference fixtures");
    addInfrastructureContract(registry, 67U, "integration",
        "CF-DRIVER-067 requires package store, artifact manifest, and path-confinement fixtures");
    addInfrastructureContract(registry, 68U, "integration",
        "CF-DRIVER-068 requires verified package store closure and deterministic lock fixtures");
    addInfrastructureContract(registry, 69U, "integration",
        "CF-DRIVER-069 requires restore staging, rollback fault injection, and publication fixtures");
    addInfrastructureContract(registry, 70U, "integration",
        "CF-DRIVER-070 requires generated project injection and cleanup observation");
    addInfrastructureContract(registry, 71U, "integration",
        "CF-DRIVER-071 requires transactional Content publication and confinement fault injection");
    addInfrastructureContract(registry, 72U, "integration",
        "CF-DRIVER-072 requires project graph, installed compiler, and NSLink dispatch fixtures");
    addInfrastructureContract(registry, 73U, "integration",
        "CF-DRIVER-073 requires a resolved dependency graph and independently fingerprinted provenance report");
    addInfrastructureContract(registry, 74U, "integration",
        "CF-DRIVER-074 requires Windows child-process launch and argument round-trip fixtures");
    addInfrastructureContract(registry, 75U, "integration",
        "CF-DRIVER-075 requires confined generated build trees and removal observation");
    addInfrastructureContract(registry, 76U, "integration",
        "CF-DRIVER-076 requires transactional sidecar persistence and package CRUD fixtures");
    addDriverContract(registry, 77U, "unit", explainContract);
}
}
