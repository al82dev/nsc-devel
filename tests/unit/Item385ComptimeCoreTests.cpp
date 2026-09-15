#include "nativesharp/Frontend/ComptimeEngine.h"
#include "nativesharp/Frontend/FrontendCompilation.h"

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
    const std::string source = R"NS(
comptime int Main() { return #run((6 * 7) | 0); }
// #run(1 / 0) remains inert in a comment
// "comptime #run(9)"
)NS";
    const auto result = ComptimeEngine().execute(source);
    expect(result.success, "a valid #run expression executes");
    expect(result.source.size() == source.size(), "execution preserves every source offset");
    expect(result.evaluations.size() == 1 && result.evaluations.front().value == 42,
        "the core evaluates deterministic signed integer expressions");
    expect(result.source.find("return 42") != std::string::npos,
        "the evaluated value enters the ordinary frontend");
    const auto quoted = ComptimeEngine().execute("string text = \"comptime #run(9)\";");
    expect(quoted.success && quoted.evaluations.empty() && quoted.source.find("#run(9)") != std::string::npos,
        "strings are not interpreted as directives");

    nativesharp::frontend::FrontendCompilation compilation;
    expect(compilation.compileBuffer("comptime.ns", result.source, true),
        "a transformed comptime program compiles through the real frontend");

    const auto division = ComptimeEngine().execute("int x = #run(5 / 0);");
    expect(!division.success && division.error.find("division by zero") != std::string::npos,
        "invalid execution produces a deterministic compile-time error");

    const auto overflow = ComptimeEngine().execute("int x = #run(9223372036854775807 + 1);");
    expect(!overflow.success && overflow.error.find("overflow") != std::string::npos,
        "signed overflow is rejected rather than invoking host undefined behavior");

    return failures == 0 ? 0 : 1;
}
