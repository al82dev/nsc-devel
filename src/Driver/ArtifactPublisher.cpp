#include "ArtifactPublisher.h"

#include "nativesharp/Compilation/PrecompiledProductSidecarPath.h"
#include "nativesharp/LLVM/ThinLtoLibrarySidecarCodec.h"

#include <system_error>

namespace nativesharp::driver
{
namespace
{
std::filesystem::path companionPath(
    const std::filesystem::path& primary,
    std::string_view extension)
{
    std::filesystem::path companion = primary;
    companion.replace_extension(extension);
    return companion;
}

std::filesystem::path importLibraryPath(const std::filesystem::path& destination)
{
    std::filesystem::path importLibrary = companionPath(destination, ".lib");
    if (importLibrary == destination) importLibrary += ".import.lib";
    return importLibrary;
}
}

bool ArtifactPublisher::prepareObject(
    const std::filesystem::path& destination,
    ObjectPublication& publication,
    std::string& error)
{
    publication = {};
    publication.destination = destination;
    return transaction_.add(publication.destination, publication.stage, error);
}

bool ArtifactPublisher::prepareExecutable(
    const std::filesystem::path& destination,
    bool publishPdb,
    bool incrementalLink,
    ExecutablePublication& publication,
    std::string& error)
{
    publication = {};
    publication.destination = destination;
    if (!transaction_.add(publication.destination, publication.stage, error)) return false;
    if (publishPdb)
    {
        publication.pdbDestination = companionPath(destination, ".pdb");
        publication.pdbStage = companionPath(publication.stage, ".pdb");
    }
    if (incrementalLink)
    {
        publication.stateDestination = destination.string() + ".nslinc";
        publication.stateStage = publication.stage.string() + ".nslinc";
    }
    return true;
}

bool ArtifactPublisher::seedExecutable(
    const ExecutablePublication& publication,
    std::string& error)
{
    return basic::seedArtifactStage(
               publication.destination, publication.stage, error) &&
        basic::seedArtifactStage(
               publication.stateDestination, publication.stateStage, error);
}

bool ArtifactPublisher::seedExecutablePdb(
    const ExecutablePublication& publication,
    std::string& error)
{
    return basic::seedArtifactStage(
        publication.pdbDestination, publication.pdbStage, error);
}

bool ArtifactPublisher::addExecutablePdb(
    const ExecutablePublication& publication,
    std::string& error)
{
    return transaction_.addStaged(
        publication.pdbDestination, publication.pdbStage, error);
}

bool ArtifactPublisher::addExecutableState(
    const ExecutablePublication& publication,
    std::string& error)
{
    std::error_code stateError;
    if (!std::filesystem::is_regular_file(publication.stateStage, stateError) || stateError)
        return true;
    return transaction_.addStaged(
        publication.stateDestination, publication.stateStage, error);
}

bool ArtifactPublisher::prepareStaticLibrary(
    const std::filesystem::path& destination,
    StaticLibraryPublication& publication,
    std::string& error)
{
    publication = {};
    publication.destination = destination;
    publication.headerDestination = companionPath(destination, ".h");
    publication.metadataDestination = companionPath(destination, ".nsmeta");
    publication.productSidecarDestination =
        compilation::precompiledProductSidecarPath(publication.metadataDestination);
    if (!transaction_.add(publication.destination, publication.stage, error) ||
        !transaction_.add(
            publication.headerDestination, publication.headerStage, error) ||
        !transaction_.add(
            publication.metadataDestination, publication.metadataStage, error))
    {
        return false;
    }
    publication.productSidecarStage =
        compilation::precompiledProductSidecarPath(publication.metadataStage);
    publication.thinLtoSidecarDestination = llvm_backend::ThinLtoLibrarySidecarCodec::pathFor(destination);
    publication.thinLtoSidecarStage = llvm_backend::ThinLtoLibrarySidecarCodec::pathFor(publication.stage);
    return transaction_.addStaged(
        publication.productSidecarDestination,
        publication.productSidecarStage,
        error);
}

bool ArtifactPublisher::addStaticLibraryThinLtoSidecar(
    const StaticLibraryPublication& publication,
    std::string& error)
{
    return transaction_.addStaged(
        publication.thinLtoSidecarDestination, publication.thinLtoSidecarStage, error);
}

bool ArtifactPublisher::prepareDynamicLibrary(
    const std::filesystem::path& destination,
    bool publishPdb,
    DynamicLibraryPublication& publication,
    std::string& error)
{
    publication = {};
    publication.destination = destination;
    publication.importLibraryDestination = importLibraryPath(destination);
    publication.headerDestination = companionPath(destination, ".h");
    publication.metadataDestination = companionPath(destination, ".nsmeta");
    publication.productSidecarDestination =
        compilation::precompiledProductSidecarPath(publication.metadataDestination);
    if (publishPdb) publication.pdbDestination = companionPath(destination, ".pdb");
    if (!transaction_.add(publication.destination, publication.stage, error) ||
        !transaction_.add(publication.importLibraryDestination,
            publication.importLibraryStage, error) ||
        !transaction_.add(publication.headerDestination, publication.headerStage, error) ||
        !transaction_.add(publication.metadataDestination, publication.metadataStage, error))
        return false;
    publication.productSidecarStage =
        compilation::precompiledProductSidecarPath(publication.metadataStage);
    if (!transaction_.addStaged(
            publication.productSidecarDestination,
            publication.productSidecarStage,
            error))
        return false;
    if (publishPdb) publication.pdbStage = companionPath(publication.stage, ".pdb");
    return true;
}

bool ArtifactPublisher::addDynamicLibraryPdb(
    const DynamicLibraryPublication& publication,
    std::string& error)
{
    return transaction_.addStaged(
        publication.pdbDestination, publication.pdbStage, error);
}

bool ArtifactPublisher::addRuntimeArtifacts(
    const std::filesystem::path& destinationDirectory,
    std::span<const std::filesystem::path> stagedRuntimes,
    std::string& error)
{
    for (const std::filesystem::path& stagedRuntime : stagedRuntimes)
    {
        if (!transaction_.addStaged(
                destinationDirectory / stagedRuntime.filename(), stagedRuntime, error))
            return false;
    }
    return true;
}

bool ArtifactPublisher::commit(std::string& error)
{
    return transaction_.commit(error);
}
} // namespace nativesharp::driver
