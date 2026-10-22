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

enum class CompilationBundleMissReason
{
    None,
    Disabled,
    NotFound,
    ManifestUnreadable,
    ManifestInvalid,
    IdentityMismatch,
    ArtifactMissing,
    ArtifactUnreadable,
    ArtifactSizeMismatch,
    ArtifactDigestMismatch,
    RequiredArtifactMissing,
    Hit
};

const char* compilationBundleMissReasonName(
    CompilationBundleMissReason reason) noexcept;

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
    CompilationBundleMissReason reason = CompilationBundleMissReason::Disabled;
    std::string detail;
    std::filesystem::path entryDirectory;
    std::filesystem::path manifestPath;
    std::string resultOutput;
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
    bool store(
        std::string_view identity,
        const std::filesystem::path& outputDirectory,
        const std::vector<std::string>& outputNames,
        std::string_view resultOutput,
        std::string& error) const;

private:
    void quarantine(
        CompilationBundleLookup& lookup,
        CompilationBundleMissReason reason,
        std::string detail) const noexcept;
    std::filesystem::path root_;
};
}
