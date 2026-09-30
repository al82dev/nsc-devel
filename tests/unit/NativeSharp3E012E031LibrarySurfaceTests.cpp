#include "nativesharp/Interop/LibraryCapabilityAudit.h"

#include "../support/TestContext.h"

#include <filesystem>

namespace
{
nativesharp::interop::LibraryMetadataType type(std::string name)
{
    nativesharp::interop::LibraryMetadataType result;
    result.fullyQualifiedName = "NativeSharp.Test." + name;
    result.identityKey = result.fullyQualifiedName;
    result.accessibility = 1;
    return result;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::interop;

    LibraryMetadata model;
    model.moduleName = "SurfaceFixture";
    model.targetTriple = "x86_64-pc-windows-msvc";
    model.types = {type("FixedList"), type("FormatWriter"), type("IntegerFormatter"),
        type("FloatFormatter"), type("RegexMatcher")};

    const auto surfaces = auditLibraryCapabilitySurfaces(
        std::span<const LibraryMetadata>(&model, 1));
    tests.expect(surfaces.size() == 10, "audit has one explicit surface for every scoped E id");
    tests.expect(surfaces[1].id == "E013" && surfaces[1].missing.empty() &&
            surfaces[1].present == std::vector<std::string>{"FixedList"},
        "bounded-container surface is present only from real NSMETA type evidence");
    tests.expect(surfaces[0].id == "E012" && surfaces[0].missing.size() == 6,
        "partial bounded text remains missing instead of receiving a false green");
    tests.expect(surfaces[5].id == "E027" && surfaces[5].missing.empty(),
        "typed formatting requires all three public surface types");
    tests.expect(surfaces[9].id == "E031" && surfaces[9].present.size() == 1 &&
            surfaces[9].missing == std::vector<std::string>{"UnicodeRegexData"},
        "plain regex evidence does not claim Unicode regex completion");

    const std::string report = libraryCapabilitySurfaceReport(
        std::span<const LibraryMetadata>(&model, 1));
    tests.expect(report.find("NSLIBSURFACE 1.0") != std::string::npos &&
            report.find("semantics-certified=false") != std::string::npos &&
            report.find("E027 typed-formatting surface=present") != std::string::npos,
        "report is versioned and explicitly separates surface from semantic certification");

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "nsc_ns3_library_surface";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);
    std::string directoryReport;
    std::string error;
    tests.expect(!auditLibraryCapabilityDirectory(root, directoryReport, error) &&
            error.find("contains no .nsmeta files") != std::string::npos,
        "directory audit fails closed when no metadata evidence exists");
    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
