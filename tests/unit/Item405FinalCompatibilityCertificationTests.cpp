#include "nativesharp/Driver/CompatibilityPolicy.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp;
    using namespace nativesharp::driver;

    const std::string matrix = currentCompatibilityMatrix();
    tests.expect(matrix.starts_with("NSCOMPAT-MATRIX 1\ncertification=final\n"),
        "final N-1/N matrix is explicitly certified");
    tests.expect(matrix.find("compiler.N=2.0.0\ncompiler.N-1=1.0.1\n") != std::string::npos,
        "compiler window is explicit 1.0.1 and 2.0.0 rather than arithmetic");
    tests.expect(matrix.find("nsmeta.N=1.20\nnsmeta.N-1=1.20\n") != std::string::npos,
        "NSMETA 1.20 is preserved across the final window");
    tests.expect(matrix.find("native-abi.N=1.0\nnative-abi.N-1=1.0\n") != std::string::npos &&
            matrix.find("runtime-abi.N=1.0\nruntime-abi.N-1=1.0\n") != std::string::npos,
        "native and runtime ABI 1.0 are preserved across the final window");
    for (const std::string surface : {"compiler", "nsmeta", "native-abi", "runtime-abi"})
    {
        tests.expect(matrix.find("row=" + surface + "|consumer:N|producer:N-1|accept") !=
                std::string::npos,
            surface + " accepts its certified N-1 producer");
        tests.expect(matrix.find("row=" + surface + "|consumer:N-1|producer:N|reject") !=
                std::string::npos,
            surface + " rejects unsupported forward consumption");
    }

    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item405_final_compatibility_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::filesystem::path path = root / "NativeSharp2Compatibility.matrix";
    std::string verificationError;
    tests.expect(writeCompatibilityMatrix(path, verificationError),
        "final compatibility matrix writes: " + verificationError);
    tests.expect(verifyCompatibilityMatrix(path, verificationError),
        "final compatibility matrix verifies: " + verificationError);

    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string_view> arguments = {"--print-compatibility-matrix"};
    tests.expect(Driver{}.run(arguments, output, error) == ExitCode::Success &&
            output.str() == matrix && error.str().empty(),
        "driver prints the exact final compatibility certification");

    {
        std::ofstream stream(path, std::ios::binary | std::ios::app);
        stream << "row=compiler|consumer:N-1|producer:N|accept\n";
    }
    tests.expect(!verifyCompatibilityMatrix(path, verificationError),
        "unsupported forward-compatibility claim fails certification");
    return tests.resultKeeping(root);
}
