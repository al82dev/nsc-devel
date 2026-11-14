#include "nativesharp/Driver/CompatibilityPolicy.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::driver;

    const std::string matrix = currentCompatibilityMatrix();
    tests.expect(matrix.starts_with("NSCOMPAT-MATRIX 1\n"),
        "N-1/N matrix has canonical versioned framing");
    tests.expect(matrix.find("compiler.N=3.0.0\ncompiler.N-1=2.0.0\n") != std::string::npos,
        "N-1/N matrix uses the explicit final compiler generations");
    tests.expect(matrix.find("nsmeta=content-compatible\n") != std::string::npos &&
            matrix.find("nsmeta.N") == std::string::npos &&
            matrix.find("row=nsmeta|") == std::string::npos,
        "metadata compatibility is based on content rather than generations");
    for (const std::string surface : {"compiler", "native-abi", "runtime-abi"})
    {
        tests.expect(matrix.find("row=" + surface + "|consumer:N|producer:N-1|accept") !=
                std::string::npos,
            surface + " accepts the previous producer generation");
        tests.expect(matrix.find("row=" + surface + "|consumer:N|producer:N|accept") !=
                std::string::npos,
            surface + " accepts the current producer generation");
        tests.expect(matrix.find("row=" + surface + "|consumer:N-1|producer:N|reject") !=
                std::string::npos,
            surface + " rejects unsupported forward consumption");
    }

    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item393_compatibility_matrix_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::filesystem::path path = root / "NativeSharpCompatibility.matrix";
    std::string error;
    tests.expect(writeCompatibilityMatrix(path, error),
        "canonical compatibility matrix writes: " + error);
    tests.expect(verifyCompatibilityMatrix(path, error),
        "canonical compatibility matrix verifies: " + error);
    {
        std::string legacy = matrix;
        const auto position = legacy.find("nsmeta=content-compatible\n");
        legacy.replace(position, std::string("nsmeta=content-compatible\n").size(),
                       "nsmeta.N=99.123\nnsmeta.N-1=1.21\n");
        legacy += "row=nsmeta|consumer:N-1|producer:N|reject\n";
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << legacy;
    }
    tests.expect(verifyCompatibilityMatrix(path, error),
        "metadata generation labels do not gate compatibility matrix verification");
    tests.expect(writeCompatibilityMatrix(path, error), "canonical matrix restored");
    {
        std::ofstream stream(path, std::ios::binary | std::ios::app);
        stream << "row=compiler|consumer:N-1|producer:N|accept\n";
    }
    tests.expect(!verifyCompatibilityMatrix(path, error) &&
            error.find("does not match") != std::string::npos,
        "forward-compatible mutation fails certification");
    return tests.resultKeeping(root);
}
