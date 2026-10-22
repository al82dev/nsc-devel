#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::compilation
{
enum class CompilationBundleLookupKind
{
    Disabled,
    Missing,
    Candidate,
    Verified
};

struct CompilationBundleArtifact final
{
    std::filesystem::path cachedPath;
    std::filesystem::path destinationPath;
    std::uintmax_t size = 0;
    std::string digest;
};

struct CompilationBundleLookup final
{
    CompilationBundleLookupKind kind = CompilationBundleLookupKind::Disabled;
    std::filesystem::path entryDirectory;
    std::filesystem::path manifestPath;
    std::vector<CompilationBundleArtifact> artifacts;
};

class CompilationBundleCache final
{
public:
    void setRoot(std::filesystem::path root);
    bool lookup(
        std::string_view identity,
        CompilationBundleLookup& result,
        std::string& error) const;
    bool verify(
        std::string_view identity,
        const std::filesystem::path& outputDirectory,
        const std::vector<std::string>& requiredOutputNames,
        CompilationBundleLookup& lookup,
        bool& hit,
        std::string& error) const;
    bool restore(
        const CompilationBundleLookup& lookup,
        bool& restored,
        std::string& error) const;

private:
    std::filesystem::path root_;
};
}
