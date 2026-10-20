#pragma once

#include "nativesharp/Basic/ArtifactTransaction.h"
#include "DynamicLibraryPublication.h"
#include "ExecutablePublication.h"
#include "ObjectPublication.h"
#include "StaticLibraryPublication.h"

#include <filesystem>
#include <span>
#include <string>

namespace nativesharp::driver
{
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
