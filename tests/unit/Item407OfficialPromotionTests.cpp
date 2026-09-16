#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/PromotionAct.h"
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

    tests.expect(kCompilerReleaseVersion == "2.0.0" && kLanguageVersion == "2.0.0",
        "compiler and language identities are officially 2.0.0");
    tests.expect(kCompilerVersion.starts_with("2.0.0+build.") &&
            kCompilerVersion.find("-dev") == std::string_view::npos,
        "visible compiler identity is stable and retains exact build metadata");
    tests.expect(kCompilerStage == "70", "official 2.0 promotion uses stage 70");

    const std::string act = currentPromotionAct();
    tests.expect(act.starts_with("NSPROMOTION 1\n") &&
            act.find("decision=officially-promoted\n") != std::string::npos,
        "promotion act records the official decision");
    tests.expect(act.find("predecessor=1.0.1\n") != std::string::npos &&
            act.find("release=2.0.0\n") != std::string::npos,
        "promotion act binds predecessor and promoted release");

    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata metadata;
    metadata.moduleName = "PromotionCompatibility";
    metadata.targetTriple = "x86_64-pc-windows-msvc";
    metadata.producerVersion = "1.0.1+build.20260915T000000Z.0000000000000000";
    std::string verificationError;
    tests.expect(codec.verifyCurrentCompiler(metadata, verificationError),
        "2.0 accepts NSMETA from the certified 1.0.1 producer");
    metadata.producerVersion = "0.9.0";
    tests.expect(!codec.verifyCurrentCompiler(metadata, verificationError) &&
            verificationError.find("outside the supported producer window") != std::string::npos,
        "2.0 rejects producers older than the certified N-1 window");

    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item407_official_promotion_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::filesystem::path path = root / "NativeSharp2Promotion.act";
    tests.expect(writePromotionAct(path, verificationError),
        "official promotion act writes: " + verificationError);
    tests.expect(verifyPromotionAct(path, verificationError),
        "official promotion act verifies: " + verificationError);

    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string_view> arguments = {"--print-promotion-act"};
    tests.expect(Driver{}.run(arguments, output, error) == ExitCode::Success &&
            output.str() == act && error.str().empty(),
        "driver prints the exact official promotion act");

    {
        std::ofstream stream(path, std::ios::binary | std::ios::app);
        stream << "decision=development\n";
    }
    tests.expect(!verifyPromotionAct(path, verificationError),
        "promotion cannot be reverted by an appended claim");
    return tests.resultKeeping(root);
}
