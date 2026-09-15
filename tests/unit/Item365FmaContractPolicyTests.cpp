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
    const auto dump = [&](std::string_view policy, std::string& output, std::string& error) {
        const std::array<std::string_view, 4> arguments = {
            "--dump-project-oop-llvm", project, "--fp-mode=precise", policy};
        std::ostringstream out;
        std::ostringstream err;
        const ExitCode result = Driver{}.run(arguments, out, err);
        output = out.str();
        error = err.str();
        return result;
    };

    std::string off;
    std::string offError;
    tests.expect(dump("--fp-contract=off", off, offError) == ExitCode::Success &&
            off.find("fma-contract-policy: off") != std::string::npos &&
            off.find("fmul contract") == std::string::npos,
        "off forbids contraction in precise IR (" + offError + ")");

    std::string on;
    std::string onError;
    tests.expect(dump("--fp-contract=on", on, onError) == ExitCode::Success &&
            on.find("fma-contract-policy: on") != std::string::npos &&
            (on.find("fmul contract") != std::string::npos ||
             on.find("fadd contract") != std::string::npos),
        "on marks contractable multiply/add operations (" + onError + ")");

    std::ostringstream invalidOutput;
    std::ostringstream invalidError;
    const std::array<std::string_view, 3> invalid = {
        "--dump-project-oop-llvm", project, "--fp-contract=maybe"};
    tests.expect(Driver{}.run(invalid, invalidOutput, invalidError) == ExitCode::CommandLineError &&
            invalidError.str().find("expected --fp-contract=off, on, or fast") != std::string::npos,
        "unknown contraction policies are rejected with the accepted values");
    return tests.result();
}
