#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::basic
{
enum class OutputFailureKind : std::uint8_t
{
    None,
    PathLimit,
    MissingComponent,
    PermissionDenied,
    FileLocked,
    InsufficientSpace,
    IoError
};

std::string_view outputFailureKindName(OutputFailureKind kind) noexcept;

#if defined(_WIN32)
OutputFailureKind classifyWindowsOutputError(unsigned long code) noexcept;
#endif

// LLVM and lld receive short staging paths. commit() copies each completed artifact beside its
// final destination and replaces the complete set with rollback through Win32 wide-path APIs.
class ArtifactTransaction final
{
public:
    ArtifactTransaction();
    ArtifactTransaction(const ArtifactTransaction&) = delete;
    ArtifactTransaction& operator=(const ArtifactTransaction&) = delete;
    ~ArtifactTransaction();

    bool add(const std::filesystem::path& destination, std::filesystem::path& stage, std::string& error);
    bool addStaged(const std::filesystem::path& destination,
        const std::filesystem::path& stage, std::string& error);
    bool commit(std::string& error);

private:
    struct Entry final
    {
        std::filesystem::path destination;
        std::filesystem::path stage;
    };

    bool ensureStaging(std::string& error);
    std::filesystem::path stagingRoot_;
    std::vector<Entry> entries_;
    bool committed_ = false;
};

bool writeFileTransactionally(
    const std::filesystem::path& destination,
    std::span<const std::uint8_t> bytes,
    std::string& error);

bool writeTextTransactionally(
    const std::filesystem::path& destination,
    std::string_view text,
    std::string& error);

// Copies an existing published artifact into an isolated build stage. A missing source is not an
// error: it represents the cold path. Windows extended paths and durable flush semantics match
// ArtifactTransaction::commit().
bool seedArtifactStage(
    const std::filesystem::path& source,
    const std::filesystem::path& stage,
    std::string& error);
}
