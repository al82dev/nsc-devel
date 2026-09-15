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
    for (const std::string surface : {"compiler", "nsmeta", "native-abi", "runtime-abi"})
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
        std::ofstream stream(path, std::ios::binary | std::ios::app);
        stream << "row=compiler|consumer:N-1|producer:N|accept\n";
    }
    tests.expect(!verifyCompatibilityMatrix(path, error) &&
            error.find("does not match") != std::string::npos,
        "forward-compatible mutation fails certification");
    return tests.resultKeeping(root);
}
