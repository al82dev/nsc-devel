#include "ArtifactPublisher.h"

#include "../support/TestContext.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
namespace fs = std::filesystem;

void writeFile(const fs::path& path, std::string_view contents)
{
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << contents;
}

std::string readFile(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

void testObjectPublication(
    nativesharp::tests::TestContext& tests,
    const fs::path& root)
{
    nativesharp::driver::ArtifactPublisher publisher;
    nativesharp::driver::ObjectPublication publication;
    std::string error;
    const fs::path destination = root / "object" / "Product.obj";
    const bool prepared = publisher.prepareObject(destination, publication, error);
    tests.expect(prepared && publication.destination == destination &&
            publication.stage.filename() == destination.filename(),
        "object publication preserves the final basename in its isolated stage");
    if (prepared) writeFile(publication.stage, "object");
    tests.expect(prepared && publisher.commit(error) && readFile(destination) == "object",
        "object publication commits the staged artifact: " + error);
}

void testExecutablePublication(
    nativesharp::tests::TestContext& tests,
    const fs::path& root)
{
    const fs::path destination = root / "executable" / "Product.exe";
    const fs::path pdb = root / "executable" / "Product.pdb";
    const fs::path state = root / "executable" / "Product.exe.nslinc";
    writeFile(destination, "old-exe");
    writeFile(pdb, "old-pdb");
    writeFile(state, "old-state");

    nativesharp::driver::ArtifactPublisher publisher;
    nativesharp::driver::ExecutablePublication publication;
    std::string error;
    const bool prepared = publisher.prepareExecutable(
        destination, true, true, publication, error);
    const bool seeded = prepared &&
        nativesharp::driver::ArtifactPublisher::seedExecutable(publication, error) &&
        nativesharp::driver::ArtifactPublisher::seedExecutablePdb(publication, error);
    tests.expect(seeded && readFile(publication.stage) == "old-exe" &&
            readFile(publication.pdbStage) == "old-pdb" &&
            readFile(publication.stateStage) == "old-state",
        "incremental executable staging seeds PE, PDB, and link state without renaming");

    const fs::path runtimeStage = root / "runtime-stage" / "Runtime.dll";
    if (seeded)
    {
        writeFile(publication.stage, "new-exe");
        writeFile(publication.pdbStage, "new-pdb");
        writeFile(publication.stateStage, "new-state");
        writeFile(runtimeStage, "runtime");
    }
    const std::array<fs::path, 1> runtimes = {runtimeStage};
    const bool companionsAdded = seeded &&
        publisher.addExecutablePdb(publication, error) &&
        publisher.addExecutableState(publication, error) &&
        publisher.addRuntimeArtifacts(destination.parent_path(), runtimes, error);
    const bool committed = companionsAdded && publisher.commit(error);
    tests.expect(committed && readFile(destination) == "new-exe" &&
            readFile(pdb) == "new-pdb" && readFile(state) == "new-state" &&
            readFile(destination.parent_path() / "Runtime.dll") == "runtime",
        "executable publication commits PE, PDB, state, and runtimes as one set: " + error);
}

void testStaticLibraryPublication(
    nativesharp::tests::TestContext& tests,
    const fs::path& root)
{
    nativesharp::driver::ArtifactPublisher publisher;
    nativesharp::driver::StaticLibraryPublication publication;
    std::string error;
    const fs::path destination = root / "static" / "Product.lib";
    const bool prepared = publisher.prepareStaticLibrary(destination, publication, error);
    tests.expect(prepared && publication.headerDestination == root / "static" / "Product.h" &&
            publication.metadataDestination == root / "static" / "Product.nsmeta",
        "static-library strategy derives the established header and NSMETA companions");
    if (prepared)
    {
        writeFile(publication.stage, "library");
        writeFile(publication.headerStage, "header");
    }
    const bool committed = prepared && publisher.commit(error);
    tests.expect(!committed && !fs::exists(destination) &&
            !fs::exists(publication.headerDestination) &&
            error.find("stage is missing") != std::string::npos,
        "a missing static-library companion prevents every member from publishing");
}

void testDynamicLibraryPublication(
    nativesharp::tests::TestContext& tests,
    const fs::path& root)
{
    nativesharp::driver::ArtifactPublisher publisher;
    nativesharp::driver::DynamicLibraryPublication publication;
    std::string error;
    const fs::path destination = root / "dynamic" / "Product.lib";
    const bool prepared = publisher.prepareDynamicLibrary(
        destination, true, publication, error);
    tests.expect(prepared &&
            publication.importLibraryDestination == root / "dynamic" / "Product.lib.import.lib" &&
            publication.headerDestination == root / "dynamic" / "Product.h" &&
            publication.metadataDestination == root / "dynamic" / "Product.nsmeta" &&
            publication.pdbDestination == root / "dynamic" / "Product.pdb",
        "DLL strategy preserves import-library fallback and companion names");
    if (prepared)
    {
        writeFile(publication.stage, "dll");
        writeFile(publication.importLibraryStage, "import");
        writeFile(publication.headerStage, "header");
        writeFile(publication.metadataStage, "metadata");
        writeFile(publication.pdbStage, "pdb");
    }
    const bool committed = prepared &&
        publisher.addDynamicLibraryPdb(publication, error) &&
        publisher.commit(error);
    tests.expect(committed && readFile(destination) == "dll" &&
            readFile(publication.importLibraryDestination) == "import" &&
            readFile(publication.headerDestination) == "header" &&
            readFile(publication.metadataDestination) == "metadata" &&
            readFile(publication.pdbDestination) == "pdb",
        "DLL publication commits its primary and every companion together: " + error);
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    const fs::path root = fs::path(NSC_BINARY_DIR) / "artifact-publisher-tests";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root, ignored);

    testObjectPublication(tests, root);
    testExecutablePublication(tests, root);
    testStaticLibraryPublication(tests, root);
    testDynamicLibraryPublication(tests, root);

    fs::remove_all(root, ignored);
    return tests.result();
}
