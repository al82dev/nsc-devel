#include "nativesharp/Driver/CompatibilityPolicy.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

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
    tests.expect(matrix.starts_with("NSCOMPAT-MATRIX 1\ncertification=pending-final-baseline\n"),
        "N-1/N matrix remains pending until the final baseline is certified");
    tests.expect(matrix.find("compiler.N=1.0.0\ncompiler.N-1=none\n") != std::string::npos,
        "compiler window records the current product and no previous release");
    tests.expect(matrix.find("nsmeta=content-compatible\n") != std::string::npos &&
            matrix.find("nsmeta.N") == std::string::npos && matrix.find("row=nsmeta|") == std::string::npos,
        "metadata compatibility is expressed by content");
    tests.expect(matrix.find("native-abi.N=1.0\nnative-abi.N-1=1.0\n") != std::string::npos &&
            matrix.find("runtime-abi.N=1.2\nruntime-abi.N-1=1.0\n") != std::string::npos,
        "Native ABI stays at 1.0 while Runtime ABI advances from 1.0 to 1.2");
    tests.expect(matrix.find("row=compiler|consumer:N|producer:N|accept") != std::string::npos &&
            matrix.find("row=compiler|consumer:N-1|") == std::string::npos &&
            matrix.find("row=compiler|consumer:N|producer:N-1|") == std::string::npos,
        "compiler matrix does not claim a previous release");
    for (const std::string surface : {"native-abi", "runtime-abi"})
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
    const std::filesystem::path path = root / "NativeSharpCompatibility.matrix";
    std::string verificationError;
    tests.expect(writeCompatibilityMatrix(path, verificationError),
        "final compatibility matrix writes: " + verificationError);
    tests.expect(verifyCompatibilityMatrix(path, verificationError),
        "final compatibility matrix verifies: " + verificationError);

    std::string opaqueCompilerLabels = matrix;
    const std::string currentCompiler = "compiler.N=1.0.0\ncompiler.N-1=none\n";
    const auto compilerPosition = opaqueCompilerLabels.find(currentCompiler);
    opaqueCompilerLabels.replace(compilerPosition, currentCompiler.size(),
        "compiler.N=historic-label\ncompiler.N-1=other-label\n");
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << opaqueCompilerLabels;
    }
    tests.expect(verifyCompatibilityMatrix(path, verificationError),
        "compiler version labels do not gate compatibility when contracts match");

    std::string incompatibleAbi = matrix;
    const std::string nativeAbi = "native-abi.N=1.0";
    const auto abiPosition = incompatibleAbi.find(nativeAbi);
    incompatibleAbi.replace(abiPosition, nativeAbi.size(), "native-abi.N=incompatible");
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << incompatibleAbi;
    }
    tests.expect(!verifyCompatibilityMatrix(path, verificationError),
        "an incompatible native ABI remains rejected");

    tests.expect(writeCompatibilityMatrix(path, verificationError),
        "restore the canonical matrix for the forward-claim negative: " + verificationError);

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
