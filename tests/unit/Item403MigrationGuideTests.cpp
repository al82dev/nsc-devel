#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/MigrationGuide.h"

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

    const std::string guide = currentMigrationGuide();
    tests.expect(guide.starts_with("NSMIGRATION-GUIDE 1\n"),
        "migration guide has canonical versioned framing");
    tests.expect(guide.find("source_release=2.0.0\n") != std::string::npos &&
            guide.find("target_release=3.0.0\n") != std::string::npos,
        "migration guide fixes both release endpoints");
    tests.expect(guide.find("breaking_change_ledger=NSBREAKING 1\n") != std::string::npos,
        "migration guide is bound to the certified breaking-change ledger");
    tests.expect(guide.find("select-Language-3-explicitly-only-for-opted-in-projects") != std::string::npos,
        "migration guide preserves Language 2 by default and makes Language 3 opt-in");
    tests.expect(guide.find("compat.nsmeta-1.21=accepted\n") != std::string::npos &&
            guide.find("compat.runtime-abi-1.0=accepted\n") != std::string::npos,
        "migration guide records preserved exchange contracts");

    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item403_migration_guide_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::filesystem::path path = root / "NativeSharp3Migration.guide";
    std::string verificationError;
    tests.expect(writeMigrationGuide(path, verificationError),
        "canonical migration guide writes: " + verificationError);
    tests.expect(verifyMigrationGuide(path, verificationError),
        "canonical migration guide verifies: " + verificationError);

    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string_view> arguments = {"--print-migration-guide"};
    tests.expect(Driver{}.run(arguments, output, error) == ExitCode::Success &&
            output.str() == guide && error.str().empty(),
        "driver prints the canonical migration guide");

    {
        std::ofstream stream(path, std::ios::binary | std::ios::app);
        stream << "compat.unknown=accepted\n";
    }
    tests.expect(!verifyMigrationGuide(path, verificationError) &&
            verificationError.find("does not match") != std::string::npos,
        "uncertified migration claim is rejected");
    return tests.resultKeeping(root);
}
