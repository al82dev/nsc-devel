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
    tests.expect(guide.find("source_release=none\n") != std::string::npos &&
            guide.find("target_release=1.0.0\n") != std::string::npos,
        "migration guide fixes both release endpoints");
    tests.expect(guide.find("breaking_change_ledger=NSBREAKING 1\n") != std::string::npos,
        "migration guide is bound to the certified breaking-change ledger");
    tests.expect(guide.find("build-all-understood-source-with-the-cumulative-language") !=
            std::string::npos &&
            guide.find("compat.source-language=cumulative\n") != std::string::npos,
        "migration guide publishes one cumulative source language");
    tests.expect(guide.find("compat.nsmeta=content-compatible\n") != std::string::npos &&
            guide.find("compat.runtime-abi-1.0=accepted\n") != std::string::npos,
        "migration guide records preserved exchange contracts");
    tests.expect(guide.find("diagnostic.precompiled=incompatible-runtime-or-native-abi\n") !=
            std::string::npos && guide.find("unsupported-producer") == std::string::npos,
        "migration guidance identifies ABI incompatibility without rejecting a producer label");

    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item403_migration_guide_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::filesystem::path path = root / "NativeSharpMigration.guide";
    std::string verificationError;
    tests.expect(writeMigrationGuide(path, verificationError),
        "canonical migration guide writes: " + verificationError);
    tests.expect(verifyMigrationGuide(path, verificationError),
        "canonical migration guide verifies: " + verificationError);
    {
        std::string legacy = guide;
        const auto position = legacy.find("compat.nsmeta=content-compatible");
        legacy.replace(position, std::string("compat.nsmeta=content-compatible").size(),
                       "compat.nsmeta-opaque=accepted");
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << legacy;
    }
    tests.expect(verifyMigrationGuide(path, verificationError),
        "metadata annotations do not block migration guide verification");
    {
        std::ofstream stream(path, std::ios::binary | std::ios::app);
        stream << "compat.nsmetadata-unrelated=accepted\n";
    }
    tests.expect(!verifyMigrationGuide(path, verificationError),
        "unrelated fields with a similar prefix are not discarded");
    {
        std::string legacy = guide;
        const auto position = legacy.find("compat.source-language=cumulative");
        legacy.replace(position, std::string("compat.source-language=cumulative").size(),
                       "compat.source-language=opaque-label");
        legacy += "source_language=\nlanguage_label=opaque-label\nlanguage_extra=another-label\n";
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << legacy;
    }
    tests.expect(verifyMigrationGuide(path, verificationError),
        "historical language labels do not block migration guide verification");
    tests.expect(writeMigrationGuide(path, verificationError), "canonical migration guide restored");

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
