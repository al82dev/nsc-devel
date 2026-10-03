#include "nativesharp/Driver/BreakingChangeLedger.h"
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

    const std::string ledger = currentBreakingChangeLedger();
    tests.expect(ledger.starts_with("NSBREAKING 1\n"),
        "breaking-change ledger has canonical versioned framing");
    tests.expect(ledger.find("entry_count=1\n") != std::string::npos,
        "ledger declares its complete entry count");
    for (const std::string_view field : {
             "2x_contract", "consumer", "compatible_alternatives_rejected", "new_semantics",
             "impact", "diagnostics_migration", "minimum_component_version", "compatibility_tests"})
    {
        tests.expect(ledger.find("change.001." + std::string(field) + "=") != std::string::npos,
            "ledger entry contains required field " + std::string(field));
    }
    tests.expect(ledger.find("runtime-abi:1.1") != std::string::npos &&
            ledger.find("nsmeta:1.26") != std::string::npos,
        "ledger records the 3.0 runtime and metadata migration");

    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item402_breaking_change_ledger_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::filesystem::path path = root / "NativeSharp3BreakingChanges.ledger";
    std::string verificationError;
    tests.expect(writeBreakingChangeLedger(path, verificationError),
        "canonical ledger writes: " + verificationError);
    tests.expect(verifyBreakingChangeLedger(path, verificationError),
        "canonical ledger verifies: " + verificationError);

    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string_view> printArguments = {"--print-breaking-change-ledger"};
    tests.expect(Driver{}.run(printArguments, output, error) == ExitCode::Success &&
            output.str() == ledger && error.str().empty(),
        "driver prints the canonical ledger");

    {
        std::ofstream stream(path, std::ios::binary | std::ios::app);
        stream << "change.002.id=uncertified\n";
    }
    tests.expect(!verifyBreakingChangeLedger(path, verificationError) &&
            verificationError.find("does not match") != std::string::npos,
        "uncertified ledger mutation is rejected");
    return tests.resultKeeping(root);
}
