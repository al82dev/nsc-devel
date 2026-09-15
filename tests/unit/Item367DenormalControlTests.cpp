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

    const auto dump = [&](std::string_view policy, std::string& text, std::string& diagnostic) {
        const std::array<std::string_view, 3> arguments = {"--dump-project-oop-llvm", project, policy};
        std::ostringstream output;
        std::ostringstream error;
        const ExitCode result = Driver{}.run(arguments, output, error);
        text = output.str();
        diagnostic = error.str();
        return result;
    };

    std::string ftz;
    std::string ftzError;
    tests.expect(dump("--fp-denormal=ftz", ftz, ftzError) == ExitCode::Success &&
            ftz.find("floating-point-denormal: ftz") != std::string::npos &&
            ftz.find("denormal_fpenv(preservesign|ieee)") != std::string::npos,
        "FTZ flushes results while preserving denormal inputs (" + ftzError + ")");

    std::string daz;
    std::string dazError;
    tests.expect(dump("--fp-denormal=daz", daz, dazError) == ExitCode::Success &&
            daz.find("denormal_fpenv(ieee|preservesign)") != std::string::npos,
        "DAZ flushes inputs independently from results (" + dazError + ")");

    std::string both;
    std::string bothError;
    tests.expect(dump("--fp-denormal=ftz-daz", both, bothError) == ExitCode::Success &&
            both.find("denormal_fpenv(preservesign)") != std::string::npos,
        "combined FTZ/DAZ reaches both sides of the backend contract (" + bothError + ")");
    return tests.result();
}
