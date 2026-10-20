#pragma once

#include "nativesharp/LLVM/CacheMissReason.h"
#include "nativesharp/LLVM/CachePruneTelemetry.h"
#include "nativesharp/LLVM/CompilationArtifactCacheScope.h"
#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/LinkTimeOptimizationMode.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
std::string codeGenerationPolicyIdentity(LinkTimeOptimizationMode linkTimeOptimization);

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

    const CachePruneTelemetry& lastPruneTelemetry() const noexcept;

private:
    std::filesystem::path root_;
    std::string codegenIdentity_;
    mutable CachePruneTelemetry lastPruneTelemetry_;
};

const CompilationArtifactCache* activeCompilationArtifactCache() noexcept;
}
