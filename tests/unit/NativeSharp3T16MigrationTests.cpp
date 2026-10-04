#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/MigrationAnalyzer.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
std::filesystem::path writeSource(std::string_view text)
{
    const auto path = std::filesystem::temp_directory_path() /
        "nativesharp-t16-migration-lints.ns";
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
    return path;
}
}

int main()
{
    using namespace nativesharp;
    tests::TestContext tests;
    constexpr std::string_view source = R"NS(
using Old.Core;
int match = 0;
void Probe()
{
    Select(null);
    owned Box destination = source;
    Widget* pointer = null;
    byte narrow = 42;
    raii Guard guard;
    return;
}
// Select(null); byte ignored = 1; int yield = 2;
string text = "using Fake; int trait = 0;";
// nsc-migration:disable-next-line MIG3006
byte suppressed = 7;
)NS";
    const auto analysis = driver::analyzeMigrationSource("fixture.ns", source);
    tests.expect(analysis.findings.size() == 7,
        "migration analyzer reports each specified 2.x to 3.0 semantic risk once");
    for (std::size_t index = 0; index < analysis.findings.size(); ++index)
    {
        tests.expect(!analysis.findings[index].message.empty() &&
                !analysis.findings[index].alternative.empty(),
            "every migration lint has an actionable alternative");
        if (index != 0)
            tests.expect(analysis.findings[index - 1].offset <= analysis.findings[index].offset,
                "migration diagnostics are emitted in deterministic source order");
    }

    const auto generated = driver::analyzeMigrationSource(
        "generated.ns", "// nsc-migration:generated\nbyte value = 1;\n");
    tests.expect(generated.generatedSource && generated.findings.empty(),
        "generated migration sources are recognized and skipped explicitly");

    const std::filesystem::path path = writeSource(source);
    const std::string pathText = path.string();
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string_view> arguments = {"--analyze-migration", pathText};
    const driver::Driver compiler;
    const ExitCode result = compiler.run(arguments, output, error);
    tests.expect(result == ExitCode::Success && error.str().empty() &&
            output.str().find("NSMIGRATION-ANALYSIS 1") != std::string::npos &&
            output.str().find("MIG3001") != std::string::npos &&
            output.str().find("MIG3007") != std::string::npos,
        "nsc exposes the migration analyzer as a productive driver command");
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
    return tests.result();
}
