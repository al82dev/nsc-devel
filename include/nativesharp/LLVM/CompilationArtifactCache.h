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

class CompilationArtifactCache final
{
public:
    void setRoot(std::filesystem::path root);
    void setToolchainIdentity(std::string identity);

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
        std::string& error) const;

    bool storeObject(
        std::string_view key,
        const std::filesystem::path& source,
        std::string& error) const;

    bool prune(
        std::uintmax_t maximumBytes,
        std::size_t& evictedEntries,
        std::uintmax_t& evictedBytes,
        std::string& error) const;

private:
    std::filesystem::path root_;
    std::string toolchainIdentity_;
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
