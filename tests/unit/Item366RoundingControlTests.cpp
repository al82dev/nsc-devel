#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/LLVM/FloatingPointMode.h"

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
    using nativesharp::llvm_backend::FloatingPointRoundingMode;
    using nativesharp::tests::TestContext;
    TestContext tests;

    tests.expect(nativesharp::llvm_backend::parseFloatingPointRoundingMode("nearest-even") ==
            FloatingPointRoundingMode::NearestEven &&
            nativesharp::llvm_backend::floatingPointRoundingMetadata(
                FloatingPointRoundingMode::TowardZero) == "round.towardzero",
        "rounding names map to the LLVM constrained-FP contract");

    const std::string project =
        (std::filesystem::path(NSC_SOURCE_DIR) / "tests/cc1/c30_contratos/c30_contratos.nsproj").string();
    std::ostringstream output;
    std::ostringstream error;
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-llvm", project, "--fp-rounding=downward"};
    tests.expect(Driver{}.run(arguments, output, error) == ExitCode::Success &&
            output.str().find("floating-point-rounding: downward") != std::string::npos &&
            output.str().find("llvm.experimental.constrained.f") != std::string::npos &&
            output.str().find("round.downward") != std::string::npos,
        "explicit rounding emits constrained floating-point operations (" + error.str() + ")");

    std::ostringstream invalidOutput;
    std::ostringstream invalidError;
    const std::array<std::string_view, 3> invalid = {
        "--dump-project-oop-llvm", project, "--fp-rounding=sideways"};
    tests.expect(Driver{}.run(invalid, invalidOutput, invalidError) == ExitCode::CommandLineError &&
            invalidError.str().find("expected --fp-rounding=dynamic, nearest-even") != std::string::npos,
        "unknown rounding modes are rejected with the accepted values");
    return tests.result();
}
