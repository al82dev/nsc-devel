#pragma once

#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <filesystem>
#include <cstdint>
#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
std::string codeGenerationPolicyIdentity(LinkTimeOptimizationMode linkTimeOptimization);

// NSC305-P02. Stable reason for a per-code-unit artifact-cache lookup. Every miss must be able
// to explain its cause; the driver aggregates these reasons in the build report.
enum class CacheMissReason
{
    Hit,                   // Object restored and verified successfully.
    Disabled,              // Artifact cache is not enabled for this compilation.
    KeyMismatch,           // Cache key is not a canonical SHA-256.
    Cold,                  // No object is present for this key (first build or changed key).
    IncompletePublication, // Object present but its metadata was not fully published.
    Corruption,            // Metadata key or object digest mismatch; entry quarantined.
};

const char* cacheMissReasonName(CacheMissReason reason) noexcept;

std::filesystem::path defaultProjectArtifactCacheRoot(
    const std::filesystem::path& projectFile);

class CompilationArtifactCache final
{
public:
    void setRoot(std::filesystem::path root);
    void setCodegenIdentity(std::string identity);

    bool enabled() const noexcept;
    const std::filesystem::path& root() const noexcept;

    std::string objectKey(
        std::string_view optimizedLlvm,
        std::string_view targetTriple,
        std::string_view dataLayout,
        std::string_view codeGenerationPolicy,
        LlvmOptimizationLevel optimization,
        DebugInformationKind debugInformation) const;

    std::string inputObjectKey(
        std::string_view mirIdentity,
        std::string_view targetIdentity,
        std::string_view codeGenerationPolicy,
        LlvmOptimizationLevel optimization,
        DebugInformationKind debugInformation) const;

    bool restoreObject(
        std::string_view key,
        const std::filesystem::path& destination,
        bool& hit,
        std::string& error,
        CacheMissReason* reason = nullptr) const;

    bool storeObject(
        std::string_view key,
        const std::filesystem::path& source,
        std::string& error) const;

    // NSC305-L05. Restores/stores the private cache-aware LLVM partition plan. The payload is
    // integrity checked and atomically published, but remains an internal rebuildable artifact.
    bool restoreCodePartitionPlan(
        std::string_view key,
        std::string& payload,
        bool& hit,
        std::string& error) const;

    bool storeCodePartitionPlan(
        std::string_view key,
        std::string_view payload,
        std::string& error) const;

    bool prune(
        std::uintmax_t maximumBytes,
        std::size_t& evictedEntries,
        std::uintmax_t& evictedBytes,
        std::string& error) const;

private:
    std::filesystem::path root_;
    std::string codegenIdentity_;
};

class CompilationArtifactCacheScope final
{
public:
    explicit CompilationArtifactCacheScope(const CompilationArtifactCache& cache) noexcept;
    ~CompilationArtifactCacheScope();

    CompilationArtifactCacheScope(const CompilationArtifactCacheScope&) = delete;
    CompilationArtifactCacheScope& operator=(const CompilationArtifactCacheScope&) = delete;

private:
    const CompilationArtifactCache* previous_ = nullptr;
};

const CompilationArtifactCache* activeCompilationArtifactCache() noexcept;
}
