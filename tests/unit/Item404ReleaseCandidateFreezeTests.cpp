#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/ReleaseCandidateFreeze.h"

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

    const std::string freeze = currentReleaseCandidateFreeze();
    tests.expect(freeze.starts_with("NSRCFREEZE 1\n"),
        "RC freeze has canonical versioned framing");
    tests.expect(freeze.find("candidate=2.0.0-rc.1\n") != std::string::npos &&
            freeze.find("promotion_target=2.0.0\n") != std::string::npos,
        "RC freeze separates candidate identity from final promotion");
    for (const std::string_view contract : {"NSBREAKING 1", "NSMIGRATION-GUIDE 1", "NSCOMPAT-MATRIX 1"})
    {
        tests.expect(freeze.find(contract) != std::string::npos,
            "RC freeze binds contract " + std::string(contract));
    }
    tests.expect(freeze.find("change_policy=no-contract-or-product-surface-changes\n") != std::string::npos,
        "RC freeze closes contract and product-surface changes");

    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item404_rc_freeze_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::filesystem::path path = root / "NativeSharp2RC.freeze";
    std::string verificationError;
    tests.expect(writeReleaseCandidateFreeze(path, verificationError),
        "canonical RC freeze writes: " + verificationError);
    tests.expect(verifyReleaseCandidateFreeze(path, verificationError),
        "canonical RC freeze verifies: " + verificationError);

    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string_view> arguments = {"--print-release-candidate-freeze"};
    tests.expect(Driver{}.run(arguments, output, error) == ExitCode::Success &&
            output.str() == freeze && error.str().empty(),
        "driver prints the canonical RC freeze");

    {
        std::ofstream stream(path, std::ios::binary | std::ios::app);
        stream << "allowed_changes=features\n";
    }
    tests.expect(!verifyReleaseCandidateFreeze(path, verificationError) &&
            verificationError.find("does not match") != std::string::npos,
        "an opened frozen surface is rejected");
    return tests.resultKeeping(root);
}
