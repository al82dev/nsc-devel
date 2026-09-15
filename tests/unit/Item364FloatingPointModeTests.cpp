#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::driver::Driver;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::string project =
        (std::filesystem::path(NSC_SOURCE_DIR) / "tests/cc1/c30_contratos/c30_contratos.nsproj").string();
    const auto dump = [&](std::string_view mode, std::string& output, std::string& error) {
        const std::array<std::string_view, 3> arguments = {
            "--dump-project-oop-llvm", project, mode};
        std::ostringstream outputStream;
        std::ostringstream errorStream;
        const ExitCode result = Driver{}.run(arguments, outputStream, errorStream);
        output = outputStream.str();
        error = errorStream.str();
        return result;
    };

    std::string preciseOutput;
    std::string preciseError;
    tests.expect(dump("--fp-mode=precise", preciseOutput, preciseError) == ExitCode::Success &&
            preciseOutput.find("floating-point-mode: precise") != std::string::npos &&
            preciseOutput.find("fadd fast") == std::string::npos,
        "precise mode is explicit and does not attach fast-math flags (" + preciseError + ")");

    std::string fastOutput;
    std::string fastError;
    tests.expect(dump("--fp-mode=fast", fastOutput, fastError) == ExitCode::Success &&
            fastOutput.find("floating-point-mode: fast") != std::string::npos &&
            (fastOutput.find("fadd fast") != std::string::npos ||
             fastOutput.find("fmul fast") != std::string::npos),
        "fast mode attaches LLVM fast-math flags (" + fastError + ")");

    std::string deterministicOutput;
    std::string deterministicError;
    tests.expect(dump("--fp-mode=deterministic", deterministicOutput, deterministicError) ==
                ExitCode::Success &&
            deterministicOutput.find("floating-point-mode: deterministic") != std::string::npos &&
            deterministicOutput.find("fadd fast") == std::string::npos,
        "deterministic mode remains strict rather than inheriting fast mode (" +
            deterministicError + ")");
    return tests.result();
}
