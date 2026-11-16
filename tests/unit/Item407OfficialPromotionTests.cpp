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

    tests.expect(kCompilerReleaseVersion == "1.0.0",
        "compiler product identity is 1.0.0");
    tests.expect(kCompilerVersion.starts_with("1.0.0+build."),
        "visible compiler identity retains exact build metadata");
    tests.expect(kCompilerStage == "70", "development retains its traceable compiler stage");

    const std::string act = currentPromotionAct();
    tests.expect(act.starts_with("NSPROMOTION 1\n") &&
            act.find("channel=development\n") != std::string::npos &&
            act.find("decision=pending-final-baseline\n") != std::string::npos,
        "promotion act refuses to claim stable promotion before the final baseline");
    tests.expect(act.find("predecessor=none\n") != std::string::npos &&
            act.find("release=1.0.0\n") != std::string::npos,
        "promotion act records no previous release and the current product");

    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata metadata;
    metadata.moduleName = "PromotionCompatibility";
    metadata.targetTriple = "x86_64-pc-windows-msvc";
    metadata.producerVersion = "development-build-a";
    std::string verificationError;
    tests.expect(codec.verifyCurrentCompiler(metadata, verificationError),
        "compatible metadata is accepted independently of producer identity");
    metadata.producerVersion = "development-build-b";
    verificationError.clear();
    tests.expect(codec.verifyCurrentCompiler(metadata, verificationError),
        "a different producer identity does not override compatible decoded NSMETA contracts");

    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_item407_official_promotion_tests");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::filesystem::path path = root / "NativeSharpPromotion.act";
    tests.expect(writePromotionAct(path, verificationError),
        "pending promotion act writes: " + verificationError);
    tests.expect(verifyPromotionAct(path, verificationError),
        "pending promotion act verifies: " + verificationError);

    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string_view> arguments = {"--print-promotion-act"};
    tests.expect(Driver{}.run(arguments, output, error) == ExitCode::Success &&
            output.str() == act && error.str().empty(),
        "driver prints the exact pending promotion act");

    {
        std::ofstream stream(path, std::ios::binary | std::ios::app);
        stream << "decision=officially-promoted\n";
    }
    tests.expect(!verifyPromotionAct(path, verificationError),
        "promotion cannot be forged by an appended claim");
    return tests.resultKeeping(root);
}
