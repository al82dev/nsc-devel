#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/FinalBaselineContract.h"

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

    const std::string contract = currentFinalBaselineContract();
    tests.expect(contract.starts_with("NSFINALBASELINE 1\n"),
        "final baseline contract has canonical framing");
    for (const std::string_view requirement : {
             "build=clean-release", "install=verified-C:/native-sharp", "linker.001=lld-link",
             "linker.002=NSLink", "optimization.001=O0", "optimization.002=O2",
             "ctest=all-no-skips", "fixtures=all", "bug_triage=all", "gate0=landed",
             "source_digest=S0-equals-S1", "compiler_hash=build-equals-install-and-stable",
             "nslink_hash=stable", "interactive_debug=disabled", "message_boxes=forbidden",
             "failure_count=0", "promotion_requires=passed-report"})
    {
        tests.expect(contract.find(requirement) != std::string::npos,
            "final baseline requires " + std::string(requirement));
    }

    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item406_final_baseline_contract_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::filesystem::path path = root / "NativeSharp2FinalBaseline.contract";
    std::string verificationError;
    tests.expect(writeFinalBaselineContract(path, verificationError),
        "final baseline contract writes: " + verificationError);
    tests.expect(verifyFinalBaselineContract(path, verificationError),
        "final baseline contract verifies: " + verificationError);

    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string_view> arguments = {"--print-final-baseline-contract"};
    tests.expect(Driver{}.run(arguments, output, error) == ExitCode::Success &&
            output.str() == contract && error.str().empty(),
        "driver prints the exact final baseline gate");

    {
        std::ofstream stream(path, std::ios::binary | std::ios::app);
        stream << "failure_count=allowed\n";
    }
    tests.expect(!verifyFinalBaselineContract(path, verificationError),
        "a weakened final baseline gate is rejected");
    return tests.resultKeeping(root);
}
