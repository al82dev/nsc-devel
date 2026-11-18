#include "nativesharp/Basic/CompilationEvent.h"
#include "nativesharp/Driver/CompilationTelemetry.h"
#include "../support/TestContext.h"

#include <string>
#include <string_view>

namespace nativesharp::driver
{
std::string_view compilerText(std::string_view english, std::string_view) noexcept { return english; }
}

int main()
{
    nativesharp::tests::TestContext tests;
    const std::string controls = std::string("a") + char(0) + char(1) + char(8) + char(12) + char(31) + "b";
    const nativesharp::basic::CompilationEvent event{
        nativesharp::basic::CompilationEventKind::WorkFinished, "parse", controls};
    const std::string progress = nativesharp::driver::renderCompilationEvent(
        event, nativesharp::driver::ProgressFormat::Json);
    tests.expect(progress.find("a\\u0000\\u0001\\u0008\\u000c\\u001fb") != std::string::npos,
                 "JSON progress preserves control characters through escapes");

    const std::string timing = std::string("timing phase=x") + char(1) + "y us=7\n";
    const std::string trace = nativesharp::driver::renderCompilationTimeTraceJson(timing, 0);
    tests.expect(trace.find("x\\u0001y") != std::string::npos,
                 "JSON timing traces preserve control characters in phase names");
    return tests.result();
}
