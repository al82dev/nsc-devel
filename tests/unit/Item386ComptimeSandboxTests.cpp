#include "nativesharp/Frontend/ComptimeEngine.h"

#include <iostream>
#include <string>

namespace
{
int failures = 0;
void expect(bool condition, const char* message)
{
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}
}

int main()
{
    using nativesharp::frontend::ComptimeEngine;
    using nativesharp::frontend::ComptimeLimits;

    const auto host = ComptimeEngine().execute("int x = #run(Environment.TickCount64());");
    expect(!host.success && host.error.find("sandbox forbids") != std::string::npos,
        "the hermetic core rejects host calls and identifiers");

    ComptimeLimits tinySteps;
    tinySteps.maximumSteps = 8;
    const auto steps = ComptimeEngine(tinySteps).execute("int x = #run(1 + 2 + 3 + 4);");
    expect(!steps.success && steps.error.find("step budget") != std::string::npos,
        "the evaluator stops at its deterministic step budget");

    ComptimeLimits tinyDepth;
    tinyDepth.maximumNesting = 2;
    const auto depth = ComptimeEngine(tinyDepth).execute("int x = #run((((1))));");
    expect(!depth.success && depth.error.find("nesting limit") != std::string::npos,
        "nested evaluation is bounded");

    ComptimeLimits oneDirective;
    oneDirective.maximumDirectives = 1;
    const auto directives = ComptimeEngine(oneDirective).execute(
        "int x = #run(1); int y = #run(2);");
    expect(!directives.success && directives.error.find("directive limit") != std::string::npos,
        "the number of executions per source is bounded");

    ComptimeLimits tinySource;
    tinySource.maximumSourceBytes = 4;
    const auto source = ComptimeEngine(tinySource).execute("12345");
    expect(!source.success && source.error.find("source byte limit") != std::string::npos,
        "source memory exposure is bounded before evaluation");

    return failures == 0 ? 0 : 1;
}
