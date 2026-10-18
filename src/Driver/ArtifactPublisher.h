#pragma once

#include "nativesharp/Basic/ArtifactTransaction.h"

#include <filesystem>
#include <span>
#include <string>

namespace nativesharp::driver
{
struct ObjectPublication final
{
    std::filesystem::path destination;
    std::filesystem::path stage;
};

struct ExecutablePublication final
{
    std::filesystem::path destination;
    std::filesystem::path stage;
    std::filesystem::path pdbDestination;
    std::filesystem::path pdbStage;
    std::filesystem::path stateDestination;
    std::filesystem::path stateStage;
};

struct StaticLibraryPublication final
{
    std::filesystem::path destination;
    std::filesystem::path stage;
    std::filesystem::path headerDestination;
    std::filesystem::path headerStage;
    std::filesystem::path metadataDestination;
    std::filesystem::path metadataStage;
};

struct DynamicLibraryPublication final
{
    std::filesystem::path destination;
    std::filesystem::path stage;
    std::filesystem::path importLibraryDestination;
    std::filesystem::path importLibraryStage;
    std::filesystem::path headerDestination;
    std::filesystem::path headerStage;
    std::filesystem::path metadataDestination;
    std::filesystem::path metadataStage;
    std::filesystem::path pdbDestination;
    std::filesystem::path pdbStage;
};

/// Owns one transactional publication set while product generation writes into isolated stages.
class ArtifactPublisher final
{
public:
    bool prepareObject(
        const std::filesystem::path& destination,
        ObjectPublication& publication,
        std::string& error);

    bool prepareExecutable(
        const std::filesystem::path& destination,
        bool publishPdb,
        bool incrementalLink,
        ExecutablePublication& publication,
        std::string& error);
    static bool seedExecutable(
        const ExecutablePublication& publication,
        std::string& error);
    static bool seedExecutablePdb(
        const ExecutablePublication& publication,
        std::string& error);
    bool addExecutablePdb(
        const ExecutablePublication& publication,
        std::string& error);
    bool addExecutableState(
        const ExecutablePublication& publication,
        std::string& error);

    bool prepareStaticLibrary(
        const std::filesystem::path& destination,
        StaticLibraryPublication& publication,
        std::string& error);
    bool prepareDynamicLibrary(
        const std::filesystem::path& destination,
        bool publishPdb,
        DynamicLibraryPublication& publication,
        std::string& error);
    bool addDynamicLibraryPdb(
        const DynamicLibraryPublication& publication,
        std::string& error);

    bool addRuntimeArtifacts(
        const std::filesystem::path& destinationDirectory,
        std::span<const std::filesystem::path> stagedRuntimes,
        std::string& error);
    bool commit(std::string& error);

private:
    basic::ArtifactTransaction transaction_;
};
} // namespace nativesharp::driver
