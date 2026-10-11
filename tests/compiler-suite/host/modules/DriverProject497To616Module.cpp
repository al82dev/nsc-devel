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
    addDriverContract(registry, 77U, "unit", explainContract);
}
}
