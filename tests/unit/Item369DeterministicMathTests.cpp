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
    nativesharp::tests::TestContext tests;
    const std::string project =
        (std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--dump-project-oop-llvm", project, "--fp-mode=deterministic"};
    std::ostringstream output;
    std::ostringstream error;
    const ExitCode result = Driver{}.run(arguments, output, error);
    const std::string ir = output.str();
    tests.expect(result == ExitCode::Success,
        "the core library compiles under the deterministic contract (" + error.str() + ")");
    tests.expect(ir.find("DeterministicMath") != std::string::npos &&
            ir.find("llvm.experimental.constrained.f") != std::string::npos &&
            ir.find("round.tonearest") != std::string::npos,
        "transcendentals are library-owned and every arithmetic step uses constrained IR");
    tests.expect(ir.find("@sin") == std::string::npos && ir.find("@cos") == std::string::npos &&
            ir.find("@exp") == std::string::npos && ir.find("@log") == std::string::npos,
        "deterministic math does not depend on platform libm entry points");
    return tests.result();
}
